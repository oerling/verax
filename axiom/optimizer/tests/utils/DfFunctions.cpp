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

#include "axiom/optimizer/tests/utils/DfFunctions.h"
namespace facebook::velox::optimizer::test {

namespace lp = facebook::velox::logical_plan;

namespace {
std::unordered_map<std::string, logical_plan::ExprResolver::FunctionRewriteHook>
    functionHooks;
}

lp::ExprPtr featureFuncHook(
    const std::string& name,
    const std::vector<lp::ExprPtr>& args) {
  auto it = functionHooks.find(name);
  if (it == functionHooks.end()) {
    return nullptr;
  }
  return it->second(name, args);
}

void registerFeatureFuncHook(
    const std::string& name,
    logical_plan::ExprResolver::FunctionRewriteHook hook) {
  functionHooks[name] = hook;
}

lp::ExprPtr makeRowFromMapHook(
    const std::string& name,
    const std::vector<lp::ExprPtr>& args) {
  VELOX_CHECK_EQ(3, args.size());
  std::vector<std::string> names;
  std::vector<TypePtr> types;
  VELOX_CHECK_EQ(TypeKind::MAP, args[0]->type()->kind());
  auto type = args[0]->type()->childAt(1);
  auto* namesVariant = args[1]->asUnchecked<lp::ConstantExpr>()->value().get();
  auto namesArray = namesVariant->value<TypeKind::ARRAY>();
  for (auto i = 0; i < namesArray.size(); ++i) {
  }
  return nullptr;
}

lp::ExprPtr makeNamedRowHook(
    const std::string& name,
    const std::vector<lp::ExprPtr>& args) {
  VELOX_CHECK_EQ(3, args.size());
  return nullptr;
}

void registerDfFunctions() {
  registerFeatureFuncHook("make_row_from_map", makeRowFromMapHook);
  registerFeatureFuncHook("make_named_row", makeNamedRowHook);
}


} // namespace facebook::velox::optimizer::test
