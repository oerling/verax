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

#include "axiom/optimizer/tests/PlanMatcherGenerator.h"
#include <sstream>
#include <unordered_map>
#include <vector>
#include "axiom/optimizer/tests/ExprPrinters.h"
#include "velox/connectors/hive/TableHandle.h"
#include "velox/core/PlanNode.h"
#include "velox/type/Filter.h"
#include "velox/type/tests/SubfieldFiltersBuilder.h"

namespace facebook::velox::core {

namespace {

/// Escapes a string for use in C++ string literals.
std::string escapeString(const std::string& str) {
  std::ostringstream oss;
  for (char c : str) {
    switch (c) {
      case '"':
        oss << "\\\"";
        break;
      case '\\':
        oss << "\\\\";
        break;
      case '\n':
        oss << "\\n";
        break;
      case '\t':
        oss << "\\t";
        break;
      default:
        oss << c;
    }
  }
  return oss.str();
}

/// Generates C++ code to create a Filter object.
std::string generateFilterCode(const common::Filter& filter) {
  std::ostringstream oss;

  switch (filter.kind()) {
    case common::FilterKind::kIsNull:
      oss << "std::make_unique<common::IsNull>()";
      break;

    case common::FilterKind::kIsNotNull:
      oss << "std::make_unique<common::IsNotNull>()";
      break;

    case common::FilterKind::kBoolValue: {
      auto& boolFilter = static_cast<const common::BoolValue&>(filter);
      // Access value through testBool - true passes if value matches
      bool value = boolFilter.testBool(true);
      oss << "std::make_unique<common::BoolValue>(" << (value ? "true" : "false")
          << ", " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kBigintRange: {
      auto& range = static_cast<const common::BigintRange&>(filter);
      oss << "std::make_unique<common::BigintRange>("
          << range.lower() << ", " << range.upper() << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kNegatedBigintRange: {
      auto& negRange = static_cast<const common::NegatedBigintRange&>(filter);
      oss << "std::make_unique<common::NegatedBigintRange>("
          << negRange.lower() << ", " << negRange.upper() << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kBigintValuesUsingHashTable: {
      auto& values = static_cast<const common::BigintValuesUsingHashTable&>(filter);
      oss << "std::make_unique<common::BigintValuesUsingHashTable>("
          << values.min() << ", " << values.max() << ", "
          << "std::vector<int64_t>{";
      bool first = true;
      for (auto val : values.values()) {
        if (!first) oss << ", ";
        oss << val;
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kBigintValuesUsingBitmask: {
      auto& values = static_cast<const common::BigintValuesUsingBitmask&>(filter);
      auto valVec = values.values();
      oss << "std::make_unique<common::BigintValuesUsingBitmask>("
          << values.min() << ", " << values.max() << ", "
          << "std::vector<int64_t>{";
      bool first = true;
      for (auto val : valVec) {
        if (!first) oss << ", ";
        oss << val;
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kNegatedBigintValuesUsingHashTable: {
      auto& negValues = static_cast<const common::NegatedBigintValuesUsingHashTable&>(filter);
      oss << "std::make_unique<common::NegatedBigintValuesUsingHashTable>("
          << negValues.min() << ", " << negValues.max() << ", "
          << "std::vector<int64_t>{";
      bool first = true;
      for (auto val : negValues.values()) {
        if (!first) oss << ", ";
        oss << val;
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kNegatedBigintValuesUsingBitmask: {
      auto& negValues = static_cast<const common::NegatedBigintValuesUsingBitmask&>(filter);
      auto valVec = negValues.values();
      oss << "std::make_unique<common::NegatedBigintValuesUsingBitmask>("
          << negValues.min() << ", " << negValues.max() << ", "
          << "std::vector<int64_t>{";
      bool first = true;
      for (auto val : valVec) {
        if (!first) oss << ", ";
        oss << val;
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kDoubleRange: {
      auto& range = static_cast<const common::DoubleRange&>(filter);
      oss << "std::make_unique<common::DoubleRange>("
          << range.lower() << ", " << (range.lowerUnbounded() ? "true" : "false") << ", "
          << (range.lowerExclusive() ? "true" : "false") << ", "
          << range.upper() << ", " << (range.upperUnbounded() ? "true" : "false") << ", "
          << (range.upperExclusive() ? "true" : "false") << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kFloatRange: {
      auto& range = static_cast<const common::FloatRange&>(filter);
      oss << "std::make_unique<common::FloatRange>("
          << range.lower() << "f, " << (range.lowerUnbounded() ? "true" : "false") << ", "
          << (range.lowerExclusive() ? "true" : "false") << ", "
          << range.upper() << "f, " << (range.upperUnbounded() ? "true" : "false") << ", "
          << (range.upperExclusive() ? "true" : "false") << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kBytesRange: {
      auto& range = static_cast<const common::BytesRange&>(filter);
      oss << "std::make_unique<common::BytesRange>(";
      oss << "\"" << escapeString(range.lower()) << "\", "
          << (range.lowerUnbounded() ? "true" : "false") << ", "
          << (range.lowerExclusive() ? "true" : "false") << ", "
          << "\"" << escapeString(range.upper()) << "\", "
          << (range.upperUnbounded() ? "true" : "false") << ", "
          << (range.upperExclusive() ? "true" : "false") << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kNegatedBytesRange: {
      auto& negRange = static_cast<const common::NegatedBytesRange&>(filter);
      oss << "std::make_unique<common::NegatedBytesRange>(";
      oss << "\"" << escapeString(negRange.lower()) << "\", "
          << (negRange.lowerUnbounded() ? "true" : "false") << ", "
          << (negRange.lowerExclusive() ? "true" : "false") << ", "
          << "\"" << escapeString(negRange.upper()) << "\", "
          << (negRange.upperUnbounded() ? "true" : "false") << ", "
          << (negRange.upperExclusive() ? "true" : "false") << ", "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kBytesValues: {
      auto& values = static_cast<const common::BytesValues&>(filter);
      oss << "std::make_unique<common::BytesValues>(std::vector<std::string>{";
      bool first = true;
      for (const auto& val : values.values()) {
        if (!first) oss << ", ";
        oss << "\"" << escapeString(val) << "\"";
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kNegatedBytesValues: {
      auto& negValues = static_cast<const common::NegatedBytesValues&>(filter);
      oss << "std::make_unique<common::NegatedBytesValues>(std::vector<std::string>{";
      bool first = true;
      for (const auto& val : negValues.values()) {
        if (!first) oss << ", ";
        oss << "\"" << escapeString(val) << "\"";
        first = false;
      }
      oss << "}, " << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kHugeintRange: {
      auto& range = static_cast<const common::HugeintRange&>(filter);
      oss << "std::make_unique<common::HugeintRange>("
          << "int128_t(" << range.lower() << "), "
          << "int128_t(" << range.upper() << "), "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    case common::FilterKind::kTimestampRange: {
      auto& range = static_cast<const common::TimestampRange&>(filter);
      oss << "std::make_unique<common::TimestampRange>("
          << "Timestamp(" << range.lower().getSeconds() << ", "
          << range.lower().getNanos() << "), "
          << "Timestamp(" << range.upper().getSeconds() << ", "
          << range.upper().getNanos() << "), "
          << (filter.nullAllowed() ? "true" : "false") << ")";
      break;
    }

    default:
      oss << "/* Unsupported filter kind: " << static_cast<int>(filter.kind()) << " */";
      break;
  }

  return oss.str();
}

/// Generates C++ code to create a SubfieldFilters map.
std::string generateSubfieldFiltersCode(
    const common::SubfieldFilters& subfieldFilters) {
  if (subfieldFilters.empty()) {
    return "{}";
  }

  std::ostringstream oss;
  oss << "common::test::SubfieldFiltersBuilder()";

  for (const auto& [subfield, filter] : subfieldFilters) {
    oss << "\n        .add(\"" << escapeString(subfield.toString()) << "\", "
        << generateFilterCode(*filter) << ")";
  }

  oss << "\n        .build()";

  return oss.str();
}

/// Generates a vector literal from a vector of strings.
std::string generateVectorLiteral(const std::vector<std::string>& items) {
  if (items.empty()) {
    return "{}";
  }

  std::ostringstream oss;
  oss << "{";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << "\"" << escapeString(items[i]) << "\"";
  }
  oss << "}";
  return oss.str();
}

/// Generates code for a TableScanNode.
std::string generateTableScanCode(const TableScanNode& node) {
  const auto& tableName = node.tableHandle()->name();

  // Check if this is a HiveTableHandle
  const auto* hiveTableHandle =
      dynamic_cast<const connector::hive::HiveTableHandle*>(
          node.tableHandle().get());

  std::ostringstream oss;

  if (hiveTableHandle != nullptr) {
    // Generate hiveScan call with subfield filters and remaining filter
    const auto& subfieldFilters = hiveTableHandle->subfieldFilters();
    const auto& remainingFilter = hiveTableHandle->remainingFilter();

    oss << ".hiveScan(\"" << escapeString(tableName) << "\", ";

    // Generate subfield filters
    oss << generateSubfieldFiltersCode(subfieldFilters);

    // Generate remaining filter
    if (remainingFilter != nullptr) {
      oss << ", \"" << escapeString(ITypedExprPrinter::toText(*remainingFilter))
          << "\"";
    }

    oss << ")";
  } else {
    // Use the standard tableScan for non-Hive table handles
    oss << ".tableScan(\"" << escapeString(tableName) << "\")";
  }

  return oss.str();
}

/// Generates code for a ValuesNode.
std::string generateValuesCode(const ValuesNode& node) {
  return ".values()";
}

/// Generates code for a FilterNode.
std::string generateFilterCode(const FilterNode& node) {
  const auto& predicate = node.filter();
  std::ostringstream oss;
  oss << ".filter(\"" << escapeString(ITypedExprPrinter::toText(*predicate)) << "\")";
  return oss.str();
}

/// Generates code for a ProjectNode.
std::string generateProjectCode(const ProjectNode& node) {
  const auto& projections = node.projections();
  const auto& names = node.names();

  std::vector<std::string> expressions;
  for (size_t i = 0; i < projections.size(); ++i) {
    std::string exprStr = ITypedExprPrinter::toText(*projections[i]);
    // Add alias if the name differs from the expression
    if (names[i] != exprStr) {
      exprStr += " AS " + names[i];
    }
    expressions.push_back(exprStr);
  }

  std::ostringstream oss;
  oss << ".project(" << generateVectorLiteral(expressions) << ")";
  return oss.str();
}

/// Generates code for a ParallelProjectNode.
std::string generateParallelProjectCode(const ParallelProjectNode& node) {
  const auto& projections = node.projections();
  const auto& names = node.names();

  std::vector<std::string> expressions;
  for (size_t i = 0; i < projections.size(); ++i) {
    std::string exprStr = ITypedExprPrinter::toText(*projections[i]);
    if (names[i] != exprStr) {
      exprStr += " AS " + names[i];
    }
    expressions.push_back(exprStr);
  }

  std::ostringstream oss;
  oss << ".parallelProject(" << generateVectorLiteral(expressions) << ")";
  return oss.str();
}

/// Generates code for an AggregationNode.
std::string generateAggregationCode(const AggregationNode& node) {
  const auto& groupingKeys = node.groupingKeys();
  const auto& aggregates = node.aggregates();
  const auto& aggregateNames = node.aggregateNames();
  const auto step = node.step();

  std::vector<std::string> groupingKeyStrs;
  for (const auto& key : groupingKeys) {
    groupingKeyStrs.push_back(ITypedExprPrinter::toText(*key));
  }

  std::vector<std::string> aggregateStrs;
  for (size_t i = 0; i < aggregates.size(); ++i) {
    std::string aggStr = ITypedExprPrinter::toText(*aggregates[i].call);
    if (aggregateNames[i] != aggStr) {
      aggStr += " AS " + aggregateNames[i];
    }
    aggregateStrs.push_back(aggStr);
  }

  std::ostringstream oss;

  // Determine the aggregation type
  std::string stepMethod;
  switch (step) {
    case AggregationNode::Step::kSingle:
      stepMethod = "singleAggregation";
      break;
    case AggregationNode::Step::kPartial:
      stepMethod = "partialAggregation";
      break;
    case AggregationNode::Step::kFinal:
      stepMethod = "finalAggregation";
      break;
    case AggregationNode::Step::kIntermediate:
      stepMethod = "aggregation"; // Use generic for intermediate
      break;
  }

  oss << "." << stepMethod << "(";
  oss << generateVectorLiteral(groupingKeyStrs);
  oss << ", ";
  oss << generateVectorLiteral(aggregateStrs);
  oss << ")";

  return oss.str();
}

/// Generates code for an UnnestNode.
std::string generateUnnestCode(const UnnestNode& node) {
  const auto& replicateVars = node.replicateVariables();
  const auto& unnestVars = node.unnestVariables();

  std::vector<std::string> replicateStrs;
  for (const auto& var : replicateVars) {
    replicateStrs.push_back(ITypedExprPrinter::toText(*var));
  }

  std::vector<std::string> unnestStrs;
  for (const auto& var : unnestVars) {
    unnestStrs.push_back(ITypedExprPrinter::toText(*var));
  }

  std::ostringstream oss;
  oss << ".unnest(";
  oss << generateVectorLiteral(replicateStrs);
  oss << ", ";
  oss << generateVectorLiteral(unnestStrs);
  oss << ")";

  return oss.str();
}

/// Generates code for a LimitNode.
std::string generateLimitCode(const LimitNode& node) {
  const auto offset = node.offset();
  const auto count = node.count();
  const auto isPartial = node.isPartial();

  std::ostringstream oss;
  if (isPartial) {
    oss << ".partialLimit(" << offset << ", " << count << ")";
  } else {
    oss << ".finalLimit(" << offset << ", " << count << ")";
  }

  return oss.str();
}

/// Generates code for a TopNNode.
std::string generateTopNCode(const TopNNode& node) {
  const auto count = node.count();

  std::ostringstream oss;
  oss << ".topN(" << count << ")";

  return oss.str();
}

/// Generates code for an OrderByNode.
std::string generateOrderByCode(const OrderByNode& node) {
  const auto& sortingKeys = node.sortingKeys();
  const auto& sortingOrders = node.sortingOrders();

  std::vector<std::string> orderingStrs;
  for (size_t i = 0; i < sortingKeys.size(); ++i) {
    std::ostringstream oss;
    oss << ITypedExprPrinter::toText(*sortingKeys[i]);
    oss << " " << (sortingOrders[i].isAscending() ? "ASC" : "DESC");
    oss << " NULLS " << (sortingOrders[i].isNullsFirst() ? "FIRST" : "LAST");
    orderingStrs.push_back(oss.str());
  }

  std::ostringstream oss;
  oss << ".orderBy(" << generateVectorLiteral(orderingStrs) << ")";

  return oss.str();
}

/// Generates code for a LocalPartitionNode.
std::string generateLocalPartitionCode(const LocalPartitionNode& node) {
  return ".localPartition()";
}

/// Generates code for a LocalMergeNode.
std::string generateLocalMergeCode(const LocalMergeNode& node) {
  return ".localMerge()";
}

/// Generates code for a PartitionedOutputNode.
std::string generatePartitionedOutputCode(const PartitionedOutputNode& node) {
  return ".partitionedOutput()";
}

/// Generates code for an ExchangeNode.
std::string generateExchangeCode(const ExchangeNode& node) {
  return ".exchange()";
}

/// Generates code for a MergeExchangeNode.
std::string generateMergeExchangeCode(const MergeExchangeNode& node) {
  return ".mergeExchange()";
}

/// Generates code for a TableWriteNode.
std::string generateTableWriteCode(const TableWriteNode& node) {
  return ".tableWrite()";
}

/// Forward declarations
std::string generatePlanMatcherCodeImpl(
    const PlanNodePtr& planNode,
    std::unordered_map<const PlanNode*, std::string>& rightMatchers);
void generateJoinMatchers(
    const PlanNodePtr& planNode,
    std::vector<std::string>& matchers,
    int& matcherCounter,
    std::unordered_map<const PlanNode*, std::string>& rightMatchers);

/// Generates the matcher variable name for a join side.
std::string getJoinMatcherVarName(int counter) {
  if (counter == 0) {
    return "rightMatcher";
  }
  return "rightMatcher" + std::to_string(counter);
}

/// Collects all join right-side matchers in the tree.
void generateJoinMatchers(
    const PlanNodePtr& planNode,
    std::vector<std::string>& matchers,
    int& matcherCounter,
    std::unordered_map<const PlanNode*, std::string>& rightMatchers) {
  if (auto* joinNode = dynamic_cast<const HashJoinNode*>(planNode.get())) {
    VELOX_CHECK_EQ(
        joinNode->sources().size(), 2, "HashJoinNode must have 2 sources");

    // Recursively process left side for nested joins
    generateJoinMatchers(
        joinNode->sources()[0], matchers, matcherCounter, rightMatchers);

    // Generate matcher for the right side
    const auto& rightSource = joinNode->sources()[1];

    // First, collect any nested joins on the right side
    generateJoinMatchers(rightSource, matchers, matcherCounter, rightMatchers);

    // Then generate the matcher for this right side
    std::string rightMatcherCode =
        generatePlanMatcherCodeImpl(rightSource, rightMatchers);
    std::string matcherVar = getJoinMatcherVarName(matcherCounter++);

    // Record the mapping from right child PlanNode to matcher variable name
    rightMatchers[rightSource.get()] = matcherVar;

    std::ostringstream oss;
    oss << "auto " << matcherVar << " = core::PlanMatcherBuilder()";
    oss << rightMatcherCode;
    oss << ".build();";

    matchers.push_back(oss.str());
  } else if (
      auto* joinNode =
          dynamic_cast<const NestedLoopJoinNode*>(planNode.get())) {
    VELOX_CHECK_EQ(
        joinNode->sources().size(),
        2,
        "NestedLoopJoinNode must have 2 sources");

    // Recursively process left side for nested joins
    generateJoinMatchers(
        joinNode->sources()[0], matchers, matcherCounter, rightMatchers);

    // Generate matcher for the right side
    const auto& rightSource = joinNode->sources()[1];

    // First, collect any nested joins on the right side
    generateJoinMatchers(rightSource, matchers, matcherCounter, rightMatchers);

    // Then generate the matcher for this right side
    std::string rightMatcherCode =
        generatePlanMatcherCodeImpl(rightSource, rightMatchers);
    std::string matcherVar = getJoinMatcherVarName(matcherCounter++);

    // Record the mapping from right child PlanNode to matcher variable name
    rightMatchers[rightSource.get()] = matcherVar;

    std::ostringstream oss;
    oss << "auto " << matcherVar << " = core::PlanMatcherBuilder()";
    oss << rightMatcherCode;
    oss << ".build();";

    matchers.push_back(oss.str());
  } else {
    // Recursively process sources
    for (const auto& source : planNode->sources()) {
      generateJoinMatchers(source, matchers, matcherCounter, rightMatchers);
    }
  }
}

/// Generates code for a HashJoinNode (inline call only, not the right matcher).
std::string generateHashJoinCode(
    const HashJoinNode& node,
    const std::unordered_map<const PlanNode*, std::string>& rightMatchers) {
  // Look up the matcher variable name for this join's right child
  const auto& rightSource = node.sources()[1];
  auto it = rightMatchers.find(rightSource.get());
  VELOX_CHECK(it != rightMatchers.end(), "Right matcher not found for join");
  std::string matcherVar = it->second;

  const auto joinType = node.joinType();
  std::ostringstream oss;
  oss << ".hashJoin(" << matcherVar << ", velox::core::JoinType::";

  // Map JoinType to its enum name
  switch (joinType) {
    case JoinType::kInner:
      oss << "kInner";
      break;
    case JoinType::kLeft:
      oss << "kLeft";
      break;
    case JoinType::kRight:
      oss << "kRight";
      break;
    case JoinType::kFull:
      oss << "kFull";
      break;
    case JoinType::kLeftSemiProject:
      oss << "kLeftSemiProject";
      break;
    case JoinType::kRightSemiProject:
      oss << "kRightSemiProject";
      break;
    case JoinType::kLeftSemiFilter:
      oss << "kLeftSemiFilter";
      break;
    case JoinType::kRightSemiFilter:
      oss << "kRightSemiFilter";
      break;
    case JoinType::kAnti:
      oss << "kAnti";
      break;
    default:
      oss << "kInner"; // Default fallback
  }

  oss << ")";

  return oss.str();
}

/// Generates code for a NestedLoopJoinNode (inline call only, not the right
/// matcher).
std::string generateNestedLoopJoinCode(
    const NestedLoopJoinNode& node,
    const std::unordered_map<const PlanNode*, std::string>& rightMatchers) {
  // Look up the matcher variable name for this join's right child
  const auto& rightSource = node.sources()[1];
  auto it = rightMatchers.find(rightSource.get());
  VELOX_CHECK(it != rightMatchers.end(), "Right matcher not found for join");
  std::string matcherVar = it->second;

  const auto joinType = node.joinType();
  std::ostringstream oss;
  oss << ".nestedLoopJoin(" << matcherVar << ", velox::core::JoinType::";

  // Map JoinType to its enum name
  switch (joinType) {
    case JoinType::kInner:
      oss << "kInner";
      break;
    case JoinType::kLeft:
      oss << "kLeft";
      break;
    case JoinType::kRight:
      oss << "kRight";
      break;
    case JoinType::kFull:
      oss << "kFull";
      break;
    case JoinType::kLeftSemiProject:
      oss << "kLeftSemiProject";
      break;
    case JoinType::kRightSemiProject:
      oss << "kRightSemiProject";
      break;
    case JoinType::kLeftSemiFilter:
      oss << "kLeftSemiFilter";
      break;
    case JoinType::kRightSemiFilter:
      oss << "kRightSemiFilter";
      break;
    case JoinType::kAnti:
      oss << "kAnti";
      break;
    default:
      oss << "kInner"; // Default fallback
  }

  oss << ")";

  return oss.str();
}

/// Recursive implementation of generatePlanMatcherCode
std::string generatePlanMatcherCodeImpl(
    const PlanNodePtr& planNode,
    std::unordered_map<const PlanNode*, std::string>& rightMatchers) {
  std::ostringstream result;

  // Process sources first (post-order traversal for non-join nodes)
  const auto& sources = planNode->sources();

  // Special handling for joins which have two sources
  if (auto* joinNode = dynamic_cast<const HashJoinNode*>(planNode.get())) {
    // For joins, we generate the left source inline
    // The right source matcher is generated separately
    if (!sources.empty()) {
      result << generatePlanMatcherCodeImpl(sources[0], rightMatchers);
    }
    result << generateHashJoinCode(*joinNode, rightMatchers);
  } else if (
      auto* joinNode =
          dynamic_cast<const NestedLoopJoinNode*>(planNode.get())) {
    // For joins, we generate the left source inline
    // The right source matcher is generated separately
    if (!sources.empty()) {
      result << generatePlanMatcherCodeImpl(sources[0], rightMatchers);
    }
    result << generateNestedLoopJoinCode(*joinNode, rightMatchers);
  } else {
    // For non-join nodes, process the first source recursively
    if (!sources.empty()) {
      result << generatePlanMatcherCodeImpl(sources[0], rightMatchers);
    }

    // Generate code for the current node
    if (auto* tableScan = dynamic_cast<const TableScanNode*>(planNode.get())) {
      result << generateTableScanCode(*tableScan);
    } else if (auto* values = dynamic_cast<const ValuesNode*>(planNode.get())) {
      result << generateValuesCode(*values);
    } else if (auto* filter = dynamic_cast<const FilterNode*>(planNode.get())) {
      result << generateFilterCode(*filter);
    } else if (
        auto* parallelProject =
            dynamic_cast<const ParallelProjectNode*>(planNode.get())) {
      result << generateParallelProjectCode(*parallelProject);
    } else if (
        auto* project = dynamic_cast<const ProjectNode*>(planNode.get())) {
      result << generateProjectCode(*project);
    } else if (
        auto* agg = dynamic_cast<const AggregationNode*>(planNode.get())) {
      result << generateAggregationCode(*agg);
    } else if (auto* unnest = dynamic_cast<const UnnestNode*>(planNode.get())) {
      result << generateUnnestCode(*unnest);
    } else if (auto* limit = dynamic_cast<const LimitNode*>(planNode.get())) {
      result << generateLimitCode(*limit);
    } else if (auto* topN = dynamic_cast<const TopNNode*>(planNode.get())) {
      result << generateTopNCode(*topN);
    } else if (
        auto* orderBy = dynamic_cast<const OrderByNode*>(planNode.get())) {
      result << generateOrderByCode(*orderBy);
    } else if (
        auto* localPartition =
            dynamic_cast<const LocalPartitionNode*>(planNode.get())) {
      result << generateLocalPartitionCode(*localPartition);
    } else if (
        auto* localMerge =
            dynamic_cast<const LocalMergeNode*>(planNode.get())) {
      result << generateLocalMergeCode(*localMerge);
    } else if (
        auto* partitionedOutput =
            dynamic_cast<const PartitionedOutputNode*>(planNode.get())) {
      result << generatePartitionedOutputCode(*partitionedOutput);
    } else if (
        auto* mergeExchange =
            dynamic_cast<const MergeExchangeNode*>(planNode.get())) {
      result << generateMergeExchangeCode(*mergeExchange);
    } else if (
        auto* exchange = dynamic_cast<const ExchangeNode*>(planNode.get())) {
      result << generateExchangeCode(*exchange);
    } else if (
        auto* tableWrite =
            dynamic_cast<const TableWriteNode*>(planNode.get())) {
      result << generateTableWriteCode(*tableWrite);
    } else {
      // For unknown node types, add a comment
      result << "  // Unknown node type: "
             << folly::demangle(typeid(*planNode).name());
    }
  }

  return result.str();
}

} // namespace

std::string generatePlanMatcherCode(
    const PlanNodePtr& planNode,
    const std::string& builderVarName) {
  std::ostringstream oss;

  // Create the map to track right child PlanNode -> matcher variable name
  std::unordered_map<const PlanNode*, std::string> rightMatchers;

  // First, collect all join right-side matchers
  std::vector<std::string> joinMatchers;
  int matcherCounter = 0;
  generateJoinMatchers(planNode, joinMatchers, matcherCounter, rightMatchers);

  // Generate the join matchers first
  for (const auto& matcher : joinMatchers) {
    oss << matcher << "\n\n";
  }

  // Then generate the main matcher
  oss << "auto " << builderVarName << " = core::PlanMatcherBuilder()";
  oss << generatePlanMatcherCodeImpl(planNode, rightMatchers);
  oss << ".build();\n";

  return oss.str();
}

} // namespace facebook::velox::core
