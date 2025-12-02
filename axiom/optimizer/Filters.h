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

#include "axiom/optimizer/RelationOp.h"

namespace facebook::axiom::optimizer {

struct PlanState;

using ConstraintMap = folly::F14FastMap<int32_t, Value>;

bool isName(Name name, const char* functionName);

float combineSelectivities(
    float minLimit,
    std::span<const float> selectivities);

const Value& value(const PlanState& state, ExprCP expr);

float exprSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints);

float conjunctsSelectivity(
    const PlanState& state,
    std::span<const ExprCP> conjuncts,
    bool updateConstraints,
    ConstraintMap& newConstraints);

float comparisonSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints);

float functionSelectivity(
    const PlanState& state,
    ExprCP expr,
    bool updateConstraints,
    ConstraintMap& newConstraints);

float rangeSelectivity(
    const PlanState& state,
    ExprCP expr,
    const std::optional<velox::Variant>& lower,
    const std::optional<velox::Variant>& upper);

float rangeSelectivity(
    const PlanState& state,
    std::span<const ExprCP> exprs,
    bool updateConstraints,
    ConstraintMap& newConstraints);

} // namespace facebook::axiom::optimizer
