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

#pragma once

#include "velox/core/Expressions.h"
#include "velox/core/PlanNode.h"

namespace facebook::velox::plan {

  /// Like core;:FieldAccessTypedExpr, except that the name has a correlation name.
class Column : public core::ITypedExpr {
  std::optional<std::string> cname_;
  std::string name_;
};

/// Common base of logical plan nodes.
class LogicalPlanNode {
  // Input nodes. Must be empty for leaves. May be 2 for joins. May be n elements for QueryExprs.
  std::vector<std::shared_ptr<LogicalPlanNode>> inputs_;
};

/// Abstract base of nodes whose output only contains the columns
/// explicitly listed in the node. Aggregation and union are
/// examples. May occur on its own over a Project to explicitly limit
/// visibility of upstream columns.
class Scope : public LogicalPlanNode {
  std::optional<std::string> cname_;
  // AS declaration for inputs. Gives the second part of the name whereas 'cname_' gives the first part. May be empty if the corresponding result has a name.
  std::vector<std::optional<std::string>> aliases_;
};

using LogicalPlanNodePtr = std::shared_ptr<LogicalPlanNode>;

class Table : public LogicalPlanNode {
  // If set, cname.column_name refers to each column defined in the table of 'name'.
  std::optional<std::string> cname_;

  // Name f table. All columns given by schema for 'table' will be referenceable downstream.
  std::string name_;
};

class Join : public LogicalPlanNode {
  cor::JoinType joinType_;

  // Equalities from left and right sides.
  std::vector<core : TypedExprPtr> leftKeys_;
  std::vector<core : TypedExprPtr> rightKeys_;

  // Only set for non-inner joins.
  core::TypedExprPtr filter_;

  // Specifies what join methods (hash, index, merge, ...) can be considered.
  std::vector<JoinMethod> methodHint_;
  // If true, left and right are not swappable.
  bool fixedOrder_;
};

class Project : public LogicalPlanNode {
  // 'exprs' are referenceable downstream with the corresponding 'name_'.
  std::vector<core::TypedExprPtr> exprs_;
  std::vector<std::string> names_;

  // Optional correlation name. If set, only the namedd 'exprs_' will be visible
  // in the output.
  std::optional<std::string> cname_;
};

/// Aggregate function call.
struct Aggregate {
  /// Function name and input column names.
  CallTypedExprPtr call;

  /// Optional name of input column to use as a mask. Column type must be
  /// BOOLEAN.
  core::TypedExprPtr mask;

  /// Optional list of input columns to sort by before applying aggregate
  /// function.
  std::vector<core::TypedExprPtr> sortingKeys;

  /// A list of sorting orders that goes together with 'sortingKeys'.
  std::vector<core::SortOrder> sortingOrders;

  /// Boolean indicating whether inputs must be de-duplicated before
  /// aggregating.
  bool distinct{false};
};

class Aggregation : public Scoep {
  std::optional<std::string> cname_;
  std::vector<core::TypedExprPt> keys_;
  std::vector<core::AggregationNode::Aggregate> aggregates_;
};

class OrderBy : public LogicalPlanNode {
  std::optional<std::string> cname_;
  std::vector<core::TypedExprPtr> keys_;
  std::vector<core::SortOrder> order_;
}

class Filter : public LogicalPlanNode {
  core::TypedExprPtr filter_;
};

class LogicalSubquery : public core::ITypedExpr {
  LogicalPlanNodePtr plan_;
};

enum class QueryOp : uint8_t { kUnion, kUnionAll, kIntersect, kExcept };

/// Represents a querry expr. All leaves must have a Scope on top,
/// explicitly specifying a column order and types. The root of the
/// quey expression tree gives the outside visible column names and
/// optional correlation name.
class QueryExpr : public Scope {
  QueryOp op_;

  // Columns from inputs used for matching. Must be empty if 'op_' == kUnionAll.
  std::vector<core::TypedExprPtr> using_;
};

} // namespace facebook::velox::plan
