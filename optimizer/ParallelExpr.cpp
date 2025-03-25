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

#include "optimizer/FunctionRegistry.h" //@manual
#include "optimizer/Plan.h" //@manual
#include "optimizer/PlanUtils.h" //@manual

namespace facebook::velox::optimizer {

struct LevelData {
  int32_t exprCount{0};
  float levelCost{0};
  PlanObjectSet exprs;
};

int32_t definitionLevel(std::vector<LevelData>& levels, ExprCP expr) {
  for (auto i = 0; i < levels.size(); ++i) {
    if (levels[i].exprs.contains(expr)) {
      return i;
    }
  }
  VELOX_UNREACHABLE();
}

float selfCost(ExprCP expr) {
  switch (expr->type()) {
    case PlanType::kColumn: {
      auto kind = expr->value().type->kind();
      if (kind == TypeKind::ARRAY || kind == TypeKind::MAP) {
        return 200;
      }
      return 10;
    }
    default:
      return 5;
  }
}

void makeExprStats(
    PlanObjectSet exprs,
    std::vector<LevelData>& levelData,
    std::unordered_map<ExprCP, int32_t>& refCount) {
  PlanObjectSet counted;
  for (;;) {
    PlanObjectSet inputs;
    levelData.emplace_back();
    int32_t levelIdx = levelData.size() - 1;
    exprs.forEach([&](PlanObjectCP o) {
      auto* expr = o->as<Expr>();
      float self = selfCost(expr);
      if (counted.contains(expr)) {
        auto i = definitionLevel(levelData, expr);
        levelData[i].exprs.erase(expr);
        levelData[i].levelCost -= self;
      }
      levelData[levelIdx].exprs.add(expr);
      levelData[levelIdx].levelCost += self;
      counted.add(expr);
      if (expr->type() == PlanType::kCall) {
        for (auto& input : expr->as<Call>()->args()) {
          ++refCount[input];
          inputs.add(input);
        }
      }
    });
    if (inputs.empty()) {
      return;
    }
    exprs = std::move(inputs);
  }
}

} // namespace facebook::velox::optimizer
