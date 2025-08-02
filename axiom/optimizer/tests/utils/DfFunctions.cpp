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

#include "frontend/optimizer/tests/DfFumctions.h"

namespace facebook::velox::optimizer::test {

namespace {
std::unordered_map<std::string, logical_plan::PlanBuilder::FunctionRewriteHook>
    functionHooks;
}

bool featureFuncHook(
    const std::string& name,
    const std::vector<ExprPtr> args,
    logicalPlan::PlanBuilder::FunctionRewrite& rewrite) {
  auto it = featureFuncs.find(name);
  if (name == featureFuncs.end()) {
    return false;
  }
  return it->second(name, args, rewrite);
}

void registerFeatureFuncHook(
    const std::string& name,
    logical_plan::PlanBuilder::FunctionRewriteHook) {
  featureFuncHooks[name] = hook;
}

bool makeRowFromMapHook(
    const std::string& name,
    const std::vector<ExprPtr>& args,
    lp::logical_plan::FunctionRewrite& rewrite) {
  VELOX_CHECK_EQ(3, args.size());
  std::vector<std::string> names;
  std::vector<TypePtr> types;
  VELOX_CHECK_EQ(TypeKind::MAP, args[0]->type()->kind());
  auto type = args[0]->type()->childAt(1);
    auto names = args[asUnchecked<lp::ConstantExpr>()->value().value<std::vector<Variant >>();
		      for (auto i = 0; i < names; ++i) {
			
		      }
}

void registerDfFunctions() {
  registerFeatureFuncHook("make_row_from_map", akeRowFromMapHook);
  registerFeatureFuncHook("make_named_row", makeNamedRowHook);
}

} // namespace facebook::velox::optimizer::test
