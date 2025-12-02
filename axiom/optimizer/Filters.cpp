/*
 * Copyright (c) Meta Platforms, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>

#include "axiom/optimizer/Filters.h"
#include "axiom/optimizer/FunctionRegistry.h"
#include "axiom/optimizer/Optimization.h"
#include "axiom/optimizer/Plan.h"
#include "axiom/optimizer/PlanUtils.h"
#include "axiom/optimizer/QueryGraph.h"
#include "axiom/optimizer/RelationOpPrinter.h"
#include "axiom/optimizer/RelationOpVisitor.h"
#include "velox/common/base/SuccinctPrinter.h"
#include "velox/expression/ScopedVarSetter.h"

namespace facebook::axiom::optimizer {

bool isName(Name name, const char* functionName) {
  return name == toName(functionName);
}

float combineSelectivities(
    float minLimit,
    std::span<const float> selectivities) {
  float product = 1.0f;
  for (float c : selectivities) {
    product *= c;
  }

  // Ensures result never goes below minLimit
  // But transitions smoothly from multiplication to the limit
  if (product >= minLimit * 10) {
    return product; // Normal multiplication when far from limit
  }

  // Smooth interpolation: minLimit + dampened product
  return minLimit + (product - minLimit) * (product / (minLimit + product));
}

const Value& value(const PlanState& state, ExprCP expr) {
  return expr->value();
}

float comparisonSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints) {
  // Placeholder implementation
  return 0.8f;
}

float functionSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints) {
  // Placeholder implementation
  return 0.8f;
}

float conjunctsSelectivity(
    const PlanState& state,
    std::span<const ExprCP> conjuncts,
    bool updateConstraints,
    ConstraintMap& newConstraints) {
  std::vector<float> selectivities;
  selectivities.reserve(conjuncts.size());

  // Map from left-hand side expression to list of comparison expressions
  folly::F14FastMap<ExprCP, std::vector<ExprCP>> rangeConditions;
  std::vector<ExprCP> otherConditions;

  // Classify arguments as range conditions or other conditions
  for (auto* arg : conjuncts) {
    if (arg->is(PlanType::kCallExpr)) {
      auto argCall = arg->as<Call>();
      auto argFuncName = argCall->name();

      // Check if this is a comparison operator
      if ((isName(argFuncName, "eq") || isName(argFuncName, "lt") ||
           isName(argFuncName, "lte") || isName(argFuncName, "gt") ||
           isName(argFuncName, "gte")) &&
          argCall->args().size() == 2) {
        // Group by left-hand side
        ExprCP leftSide = argCall->args()[0];
        rangeConditions[leftSide].push_back(arg);
      } else {
        otherConditions.push_back(arg);
      }
    } else {
      otherConditions.push_back(arg);
    }
  }

  // Process range conditions grouped by left-hand side
  for (const auto& [leftSide, conditions] : rangeConditions) {
    selectivities.push_back(
        rangeSelectivity(state, conditions, updateConstraints, newConstraints));
  }

  // Process other conditions
  for (auto* arg : otherConditions) {
    selectivities.push_back(
        exprSelectivity(state, arg, updateConstraints, newConstraints));
  }

  return combineSelectivities(0.0f, selectivities);
}

float exprSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints) {
  // Handle function calls
  if (expr->is(PlanType::kCallExpr)) {
    auto call = expr->as<Call>();
    auto funcName = call->name();
    auto* registry = FunctionRegistry::instance();

    // Check for 'not' function
    if (isName(funcName, registry->negation().c_str())) {
      if (call->args().size() != 1) {
        return 0.8f;
      }
      return 1.0f -
          exprSelectivity(state, call->args()[0], false, newConstraints);
    }

    // Check for 'and' function
    if (isName(funcName, SpecialFormCallNames::kAnd)) {
      return conjunctsSelectivity(
          state, call->args(), updateConstraints, newConstraints);
    }

    // Check for 'or' function
    if (isName(funcName, SpecialFormCallNames::kOr)) {
      const auto& args = call->args();
      std::vector<float> invertedSelectivities;
      invertedSelectivities.reserve(args.size());
      for (auto* arg : args) {
        invertedSelectivities.push_back(
            1.0f - exprSelectivity(state, arg, false, newConstraints));
      }
      return 1.0f - combineSelectivities(0.0f, invertedSelectivities);
    }

    // Check for comparison operators
    if (isName(funcName, "eq") || isName(funcName, "lt") ||
        isName(funcName, "lte") || isName(funcName, "gt") ||
        isName(funcName, "gte")) {
      return comparisonSelectivity(
          state, expr, updateConstraints, newConstraints);
    }

    // Other function
    return functionSelectivity(state, expr, updateConstraints, newConstraints);
  }

  // Handle columns
  if (expr->is(PlanType::kColumnExpr)) {
    const auto& val = value(state, expr);
    if (val.type->isBoolean()) {
      if (val.trueFraction != Value::kUnknown) {
        return val.trueFraction;
      }
      return 0.8f;
    }
    // For other columns, selectivity is 1 - nullFraction
    return 1.0f - val.nullFraction;
  }

  // Handle literals
  if (expr->is(PlanType::kLiteralExpr)) {
    auto literal = expr->as<Literal>();
    const auto& lit = literal->literal();

    // null returns 0
    if (lit.isNull()) {
      return 0.0f;
    }

    // boolean false returns 0
    if (literal->value().type->isBoolean()) {
      if (lit.kind() == velox::TypeKind::BOOLEAN && !lit.value<bool>()) {
        return 0.0f;
      }
    }

    // Any other constant returns 1
    return 1.0f;
  }

  // Default case
  return 0.8f;
}

namespace {

// Returns true if the given TypeKind represents an integer type
bool isIntegerKind(velox::TypeKind kind) {
  switch (kind) {
    case velox::TypeKind::TINYINT:
    case velox::TypeKind::SMALLINT:
    case velox::TypeKind::INTEGER:
    case velox::TypeKind::BIGINT:
    case velox::TypeKind::HUGEINT:
      return true;
    default:
      return false;
  }
}

// Template function for computing range selectivity for scalar types
template <velox::TypeKind KIND>
float rangeSelectivityImpl(
    const Value& exprValue,
    const std::optional<velox::Variant>& lower,
    const std::optional<velox::Variant>& upper) {
  using T = typename velox::TypeTraits<KIND>::NativeType;

  // Non-numeric types (VARCHAR, VARBINARY, TIMESTAMP, etc.) return 0.1
  if constexpr (
      KIND == velox::TypeKind::VARCHAR || KIND == velox::TypeKind::VARBINARY ||
      KIND == velox::TypeKind::TIMESTAMP) {
    return 0.1f;
  } else {
    // Numeric types: compute intersection
    // Check if expr has min/max set
    if (!exprValue.min || !exprValue.max) {
      return 0.1f; // No min/max available
    }

    T exprMin = exprValue.min->value<KIND>();
    T exprMax = exprValue.max->value<KIND>();

    // Determine effective lower bound
    T effectiveLower = exprMin;
    if (lower.has_value()) {
      T lowerVal = lower->value<KIND>();
      effectiveLower = std::max(exprMin, lowerVal);
    }

    // Determine effective upper bound
    T effectiveUpper = exprMax;
    if (upper.has_value()) {
      T upperVal = upper->value<KIND>();
      effectiveUpper = std::min(exprMax, upperVal);
    }

    // Check if intersection is empty
    if (effectiveLower > effectiveUpper) {
      return 0.0f;
    }

    // Compute selectivity as ratio of intersection to expr range
    // Convert to double for computation to avoid overflow/underflow
    double intersectionRange = static_cast<double>(effectiveUpper) -
        static_cast<double>(effectiveLower);
    double exprRange =
        static_cast<double>(exprMax) - static_cast<double>(exprMin);

    // Ensure divisor is at least 1
    if (exprRange < 1.0) {
      exprRange = 1.0;
    }

    float selectivity = static_cast<float>(intersectionRange / exprRange);

    // Clamp to [0, 1]
    return std::clamp(selectivity, 0.0f, 1.0f);
  }
}

} // namespace

float rangeSelectivity(
    const PlanState& state,
    ExprCP expr,
    const std::optional<velox::Variant>& lower,
    const std::optional<velox::Variant>& upper) {
  const auto& exprValue = value(state, expr);
  const auto kind = exprValue.type->kind();

  // Handle non-scalar types explicitly
  switch (kind) {
    case velox::TypeKind::ARRAY:
    case velox::TypeKind::MAP:
    case velox::TypeKind::ROW:
    case velox::TypeKind::UNKNOWN:
    case velox::TypeKind::FUNCTION:
    case velox::TypeKind::OPAQUE:
    case velox::TypeKind::INVALID:
      return 0.1f;
    default:
      break;
  }

  // Dispatch on scalar TypeKind using Velox macro
  return VELOX_DYNAMIC_SCALAR_TYPE_DISPATCH(
      rangeSelectivityImpl, kind, exprValue, lower, upper);
}

void newConstraint(
    const PlanState& state,
    ExprCP expr,
    std::optional<velox::Variant> lower,
    std::optional<velox::Variant> upper,
    float cardinality,
    ConstraintMap& constraints) {
  const auto& oldValue = value(state, expr);

  // Default lower from oldValue.min if not provided
  if (!lower.has_value() && oldValue.min != nullptr) {
    lower = *oldValue.min;
  }

  // Default upper from oldValue.max if not provided
  if (!upper.has_value() && oldValue.max != nullptr) {
    upper = *oldValue.max;
  }

  // Tighten bounds: take intersection of old and new constraints
  // For lower bound: max(old_min, new_lower) - highest lower bound wins
  if (lower.has_value() && oldValue.min != nullptr) {
    if (*oldValue.min < lower.value()) {
      // Old min is less restrictive, use new tighter lower bound
      lower = lower.value();
    } else {
      // Old min is more restrictive, keep it
      lower = *oldValue.min;
    }
  }

  // For upper bound: min(old_max, new_upper) - lowest upper bound wins
  if (upper.has_value() && oldValue.max != nullptr) {
    if (upper.value() < *oldValue.max) {
      // New upper is more restrictive, use it
      upper = upper.value();
    } else {
      // Old max is more restrictive, keep it
      upper = *oldValue.max;
    }
  }

  // Determine min and max pointers
  const velox::Variant* minPtr = oldValue.min;
  const velox::Variant* maxPtr = oldValue.max;

  if (lower.has_value()) {
    minPtr = registerVariant(lower.value());
  }
  if (upper.has_value()) {
    maxPtr = registerVariant(upper.value());
  }

  // Determine final cardinality
  float finalCardinality = cardinality;
  auto kind = oldValue.type->kind();

  // If the type is an integer type and both upper and lower are set (after
  // defaulting), adjust cardinality based on range
  if (isIntegerKind(kind) && lower.has_value() && upper.has_value()) {
    // Calculate range-based cardinality: 1 + (upper - lower)
    float rangeCardinality = 0.0f;
    switch (kind) {
      case velox::TypeKind::TINYINT: {
        auto upperVal = upper->value<velox::TypeKind::TINYINT>();
        auto lowerVal = lower->value<velox::TypeKind::TINYINT>();
        rangeCardinality = 1.0f + static_cast<float>(upperVal - lowerVal);
        break;
      }
      case velox::TypeKind::SMALLINT: {
        auto upperVal = upper->value<velox::TypeKind::SMALLINT>();
        auto lowerVal = lower->value<velox::TypeKind::SMALLINT>();
        rangeCardinality = 1.0f + static_cast<float>(upperVal - lowerVal);
        break;
      }
      case velox::TypeKind::INTEGER: {
        auto upperVal = upper->value<velox::TypeKind::INTEGER>();
        auto lowerVal = lower->value<velox::TypeKind::INTEGER>();
        rangeCardinality = 1.0f + static_cast<float>(upperVal - lowerVal);
        break;
      }
      case velox::TypeKind::BIGINT: {
        auto upperVal = upper->value<velox::TypeKind::BIGINT>();
        auto lowerVal = lower->value<velox::TypeKind::BIGINT>();
        rangeCardinality = 1.0f + static_cast<float>(upperVal - lowerVal);
        break;
      }
      case velox::TypeKind::HUGEINT: {
        auto upperVal = upper->value<velox::TypeKind::HUGEINT>();
        auto lowerVal = lower->value<velox::TypeKind::HUGEINT>();
        rangeCardinality = 1.0f + static_cast<float>(upperVal - lowerVal);
        break;
      }
      default:
        rangeCardinality = cardinality;
        break;
    }

    // Set cardinality to the minimum of the provided cardinality and range
    // cardinality
    finalCardinality = std::min(cardinality, rangeCardinality);
  }

  // Create a new Value with updated fields
  Value newValue(oldValue.type, finalCardinality);
  newValue.min = minPtr;
  newValue.max = maxPtr;
  newValue.trueFraction = oldValue.trueFraction;
  newValue.nullFraction = 0;
  newValue.nullable = false;

  // Add to constraints using expr->id()
  // Erase existing entry if present, then emplace new one (Value has const
  // fields)
  constraints.erase(expr->id());
  constraints.emplace(expr->id(), newValue);
}

float rangeSelectivity(
    const PlanState& state,
    std::span<const ExprCP> exprs,
    bool updateConstraints,
    ConstraintMap& newConstraints) {
  bool empty = false;
  std::optional<velox::Variant> lower;
  std::optional<velox::Variant> upper;
  std::optional<float> eqSelectivity;
  std::optional<velox::Variant> eqValue;

  ExprCP leftSide = nullptr;

  // Loop over the expressions
  for (auto* expr : exprs) {
    if (!expr->is(PlanType::kCallExpr)) {
      continue;
    }

    auto call = expr->as<Call>();
    auto funcName = call->name();

    if (call->args().size() != 2) {
      continue;
    }

    // Get left-hand side and right-hand side
    ExprCP lhs = call->args()[0];
    ExprCP rhs = call->args()[1];

    if (leftSide == nullptr) {
      leftSide = lhs;
    }

    // Handle eq function
    if (isName(funcName, "eq")) {
      if (rhs->is(PlanType::kLiteralExpr)) {
        auto literal = rhs->as<Literal>();
        const auto& litValue = literal->literal();

        if (!eqSelectivity.has_value()) {
          const auto& val = value(state, lhs);
          eqSelectivity = 1.0f / val.cardinality;
          eqValue = litValue;
        } else {
          // Check if the literal is different
          if (eqValue.has_value() && !(eqValue->equals(litValue))) {
            empty = true;
          }
        }
      }
    }
    // Handle lt or lte functions
    else if (isName(funcName, "lt") || isName(funcName, "lte")) {
      if (rhs->is(PlanType::kLiteralExpr)) {
        auto literal = rhs->as<Literal>();
        const auto& litValue = literal->literal();

        if (!upper.has_value() || litValue < upper.value()) {
          upper = litValue;
        }
      }
    }
    // Handle gt or gte functions
    else if (isName(funcName, "gt") || isName(funcName, "gte")) {
      if (rhs->is(PlanType::kLiteralExpr)) {
        auto literal = rhs->as<Literal>();
        const auto& litValue = literal->literal();

        if (!lower.has_value() || lower.value() < litValue) {
          lower = litValue;
        }
      }
    }
  }

  // After looping over the exprs
  if (eqValue.has_value() && !empty) {
    if (updateConstraints && leftSide != nullptr) {
      newConstraint(state, leftSide, eqValue, eqValue, 1.0f, newConstraints);
    }
    return eqSelectivity.value();
  }

  if (upper.has_value() || lower.has_value()) {
    if (leftSide != nullptr) {
      float selectivity = rangeSelectivity(state, leftSide, lower, upper);

      if (updateConstraints) {
        const auto& val = value(state, leftSide);
        newConstraint(
            state, leftSide, lower, upper, val.cardinality, newConstraints);
      }

      return selectivity;
    }
  }

  // Default case
  return 0.5f;
}

} // namespace facebook::axiom::optimizer
