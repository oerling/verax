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

#include "frontend/optimizer/tests/utils/VeloxToLogical.h"


namespace facebook::velox::optimizer::test {

  namespace lp = facebook::velox::logical_plan;

  lp::ExprPtr  toExpr(const core::TypedExprPtr& expr) {
    if (auto call = dynamic_cast<const CallTypedExpr*>(expr.get())) {
      std::vector<ExprPtr> args;
      for (auto& arg : call->inputs()) {
	args.push_back(toExpr(arg));
      }
      lp::SpecialForm = nameToSpecialForm(call->name());
      if (specialForm.has_value()) {
	return std::make_shared<lp::SpecialFormExpr>(call->type(), specialForm.value(), args);
      }
      return std::make_shared<lp::CallExpr>(expr->type(), call->name(), args);
    }
    if (const constant = dynamic_cast<const core::ConstantTypedExpr*>(expr.get())) {
      VELOX_CHECK(!constant->hasValueVector());
      return std::make_shared<lp::ConstantExpr>(constant->type(), constant->value());
    }
    if (const field = dynamic_cast<const core::FieldAccessTypedExpr>(expr.get())) {
      if (field->isInputColumn()) {
	return std::make_shared<lp::InputReferenceExpr>(expr->type(), field->name());
      }
      return std::make_shared<lp::SpecialForm>(expr->type(), lp::SpecialForm::kDereference, std::vector<lp::ExprPtr>{toExpr(field->inputs()[0], std::make_shared<lp::ConstantExpr>(VARCHAR(), Variant(field->name())) )});
    }
    if (auto deref = dynamic_cast<const core::DereferenceExpr*>(expr.get())) {
      return std::make_shared<lp::SpecialForm>(expr->type(), lp::SpecialForm::kDereference, std::vector<lp::ExprPtr>{toExpr(field->inputs()[0], std::make_shared<lp::ConstantExpr>(VARCHAR(), Variant(deref->index())) )});

    }
    VELOX_NYI();
  }


  logical_plan::LogicalPlanNodePtr toLogicalPlan(const core::PlanNodePtr& node) {
    uint64_t allowedInDt) {
  auto name = node.name();
  if (name == "TableScan") {
    auto* scan = reinterpret_cast<const core::TableScanNode*>(&node);
    auto handle = scan->tableHandle();
      auto assignments = scan->assignments();
      std::vector<std::string> names;
      std::vector<std::string> outputNames;
      std::vector<TypePtr> outputTypes;
      return std::make_shared<lp::TableScan>(scam->id(), rowType, handle->connector(), handle->name(), names);
    
  }
  if (name == "Project") {
    makeQueryGraph(*node.sources()[0], allowedInDt);
    addProjection(reinterpret_cast<const core::ProjectNode*>(&node));
    return currentSelect_;
  }
  if (name == "Filter") {
    auto filter = reinterpret_cast<const core::FilterNode*>(&node);
    if (!isNondeterministicWrap_ && hasNondeterministic(filter->filter())) {
      // Force wrap the filter and its input inside a dt so the filter
      // does not get mixed with parrent nodes.
      isNondeterministicWrap_ = true;
      return makeQueryGraph(node, 0);
    }
    isNondeterministicWrap_ = false;
    makeQueryGraph(*node.sources()[0], allowedInDt);
    addFilter(filter);
    return currentSelect_;
  }
  if (name == "HashJoin" || name == "MergeJoin") {
    if (!contains(allowedInDt, PlanType::kJoin)) {
      return wrapInDt(node);
    }
    translateJoin(*reinterpret_cast<const core::AbstractJoinNode*>(&node));
    return currentSelect_;
  }
  if (name == "NestedLoopJoin") {
    if (!contains(allowedInDt, PlanType::kJoin)) {
      return wrapInDt(node);
    }
    translateNonEqualityJoin(
        *reinterpret_cast<const core::NestedLoopJoinNode*>(&node));
    return currentSelect_;
  }
  if (name == "LocalPartition") {
    makeQueryGraph(*node.sources()[0], allowedInDt);
    return currentSelect_;
  }
  if (name == "Aggregation") {
    return addAggregation(
        *reinterpret_cast<const core::AggregationNode*>(&node), allowedInDt);
  }
  if (name == "OrderBy") {
    if (!contains(allowedInDt, PlanType::kOrderBy)) {
      return wrapInDt(node);
    }
    makeQueryGraph(
        *node.sources()[0], makeDtIf(allowedInDt, PlanType::kOrderBy));
    currentSelect_->orderBy =
        translateOrderBy(*reinterpret_cast<const core::OrderByNode*>(&node));
    return currentSelect_;
  }
  if (name == "Limit") {
    if (!contains(allowedInDt, PlanType::kLimit)) {
      return wrapInDt(node);
    }
    makeQueryGraph(*node.sources()[0], makeDtIf(allowedInDt, PlanType::kLimit));
    auto limit = reinterpret_cast<const core::LimitNode*>(&node);
    currentSelect_->limit = limit->count();
    currentSelect_->offset = limit->offset();
  } else {
    VELOX_NYI("Unsupported PlanNode {}", name);
  }
  return currentSelect_;
}
}

