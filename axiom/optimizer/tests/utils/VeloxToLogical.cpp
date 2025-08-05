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

#include "axiom/optimizer/tests/utils/VeloxToLogical.h"
#include "velox/vector/VariantToVector.h"

namespace facebook::velox::optimizer::test {

namespace lp = facebook::velox::logical_plan;

std::optional<lp::SpecialForm> nameToSpecialForm(const std::string& name) {
  if (name == "and") {
    return lp::SpecialForm::kAnd;
  }
  if (name == "or") {
    return lp::SpecialForm::kOr;
  }
  if (name == "try") {
    return lp::SpecialForm::kTry;
  }

  if (name == "coalesce") {
    return lp::SpecialForm::kCoalesce;
  }
  if (name == "if") {
    return lp::SpecialForm::kIf;
  }

  if (name == "switch") {
    return lp::SpecialForm::kSwitch;
  }
  return std::nullopt;
}

lp::ExprPtr toExpr(const core::TypedExprPtr& expr) {
  if (auto call = dynamic_cast<const core::CallTypedExpr*>(expr.get())) {
    std::vector<lp::ExprPtr> args;
    for (auto& arg : call->inputs()) {
      args.push_back(toExpr(arg));
    }
    auto specialForm = nameToSpecialForm(call->name());
    if (specialForm.has_value()) {
      return std::make_shared<lp::SpecialFormExpr>(
          call->type(), specialForm.value(), args);
    }
    return std::make_shared<lp::CallExpr>(expr->type(), call->name(), args);
  }
  if (auto* constant =
          dynamic_cast<const core::ConstantTypedExpr*>(expr.get())) {
    if (constant->hasValueVector()) {
      auto variant = std::make_shared<Variant>(
          vectorToVariant(constant->valueVector(), 0));
      return std::make_shared<lp::ConstantExpr>(
          constant->type(), std::move(variant));
    }
    return std::make_shared<lp::ConstantExpr>(
        constant->type(), std::make_shared<Variant>(constant->value()));
  }
  if (auto* field =
          dynamic_cast<const core::FieldAccessTypedExpr*>(expr.get())) {
    if (field->isInputColumn()) {
      return std::make_shared<lp::InputReferenceExpr>(
          expr->type(), field->name());
    }
    return std::make_shared<lp::SpecialFormExpr>(
        expr->type(),
        lp::SpecialForm::kDereference,
        std::vector<lp::ExprPtr>{
            toExpr(field->inputs()[0]),
            std::make_shared<lp::ConstantExpr>(
                VARCHAR(), std::make_shared<Variant>(field->name()))});
  }
  if (auto deref =
          dynamic_cast<const core::DereferenceTypedExpr*>(expr.get())) {
    return std::make_shared<lp::SpecialFormExpr>(
        expr->type(),
        lp::SpecialForm::kDereference,
        std::vector<lp::ExprPtr>{
            toExpr(deref->inputs()[0]),
            std::make_shared<lp::ConstantExpr>(
                INTEGER(),
                std::make_shared<Variant>(
                    static_cast<int32_t>(deref->index())))});
  }
  VELOX_NYI();
}

lp::LogicalPlanNodePtr toLogicalPlan(const core::PlanNode& node) {
  auto name = node.name();
  if (name == "TableScan") {
    auto* scan = reinterpret_cast<const core::TableScanNode*>(&node);
    auto handle = scan->tableHandle();
    auto assignments = scan->assignments();
    std::vector<std::string> names;
    std::vector<std::string> outputNames;
    for (auto& pair : assignments) {
      outputNames.push_back(pair.first);
      names.push_back(pair.second->name());
    }
    std::vector<TypePtr> outputTypes;
    return std::make_shared<lp::TableScanNode>(
        scan->id(),
        scan->outputType(),
        handle->connectorId(),
        handle->name(),
        names);
  }
  if (name == "Project") {
    auto input = toLogicalPlan(*node.sources()[0]);
    auto& project = *reinterpret_cast<const core::ProjectNode*>(&node);
    std::vector<lp::ExprPtr> exprs;
    std::vector<std::string> names;
    for (auto i = 0; i < project.names().size(); ++i) {
      exprs.push_back(toExpr(project.projections()[i]));
    }
    return std::make_shared<lp::ProjectNode>(
        project.id(), input, project.names(), exprs);
  }
  if (name == "Filter") {
    auto& filter = *reinterpret_cast<const core::FilterNode*>(&node);
    auto input = toLogicalPlan(node.sources()[0]);
    return std::make_shared<lp::FilterNode>(
        filter.id(), input, toExpr(filter.filter()));
  }
  if (name == "HashJoin" || name == "MergeJoin") {
    auto& join = *reinterpret_cast<const core::AbstractJoinNode*>(&node);
    auto& left = join.leftKeys();
    auto& right = join.rightKeys();
    return nullptr;
  }
#if 0
      if (name == "NestedLoopJoin") {
        translateNonEqualityJoin(
            *reinterpret_cast<const core::NestedLoopJoinNode*>(&node));
      }
      if (name == "LocalPartition") {
	VELOX_CHECK_EQ(node.inputs().size(), 1);
        return toLogicalPlan(node.inputs()[0]);
      }
      if (name == "Aggregation") {
        return addAggregation(
            *reinterpret_cast<const core::AggregationNode*>(&node),
            allowedInDt);
      }
      if (name == "OrderBy") {
        if (!contains(allowedInDt, PlanType::kOrderBy)) {
          return wrapInDt(node);
        }
        makeQueryGraph(
            *node.sources()[0], makeDtIf(allowedInDt, PlanType::kOrderBy));
        currentSelect_->orderBy = translateOrderBy(
            *reinterpret_cast<const core::OrderByNode*>(&node));
        return currentSelect_;
      }
      if (name == "Limit") {
        if (!contains(allowedInDt, PlanType::kLimit)) {
          return wrapInDt(node);
        }
        makeQueryGraph(
            *node.sources()[0], makeDtIf(allowedInDt, PlanType::kLimit));
        auto limit = reinterpret_cast<const core::LimitNode*>(&node);
        currentSelect_->limit = limit->count();
        currentSelect_->offset = limit->offset();
      } else {
        VELOX_NYI("Unsupported PlanNode {}", name);
      }
#endif
  return nullptr;
}
} // namespace facebook::velox::optimizer::test
