/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

#include "optimizer/Plan.h" //@manual
#include "optimizer/PlanUtils.h" //@manual
#include "velox/exec/Aggregate.h"
#include "velox/expression/ConstantExpr.h"

namespace facebook::velox::optimizer {

using namespace facebook::velox;


using NodeSubfieldFunc = std::function<void(
    Optimization*,
    core::PlanNode* node,
    const std::vector<const RowType*>& context,
    const std::vector<ContextSource>& sources,
    bool isControl)>;

template <typename T>
int64_t integerValueInner(const BaseVector* vector) {
  return vector->as<ConstantVector<T>>()->valueAt(0);
}

int64_t integerValue(const BaseVector* vector) {
  switch (vector->typeKind()) {
    case TypeKind::TINYINT:
      return integerValueInner<int8_t>(vector);
    case TypeKind::SMALLINT:
      return integerValueInner<int16_t>(vector);
    case TypeKind::INTEGER:
      return integerValueInner<int32_t>(vector);
    case TypeKind::BIGINT:
      return integerValueInner<int64_t>(vector);
    default:
      VELOX_FAIL();
  }
}

void Optimization::markFieldAccessed(
    const ContextSource& source,
    int32_t ordinal,
    std::vector<Step>& steps,
    bool isControl) {
  if (source.planNode) {
    auto name = source.planNode->name();
    auto fields = isControl ? &controlSubfields_ : &payloadSubfields_;
    std::vector<Step> reverse;
    for (int32_t i = steps.size() - 1; i >= 0; --i) {
      reverse.push_back(steps[i]);
    }
    auto path = queryCtx()->toPath(make<Path>(std::move(reverse)));
    fields->nodeFields[source.planNode].resultPaths[ordinal].insert(path);
    if (name == "Project") {
      auto* project = reinterpret_cast<const core::ProjectNode*>(source.planNode);
      markSubfields(
		    project->projections()[ordinal].get(),
          steps,
          isControl,
		    std::vector<const RowType*>{project->outputType().get()},
		    std::vector<ContextSource>{ContextSource{.planNode = project->sources()[0].get()}});
      return;
    }
    auto& sources = source.planNode->sources();
    if (sources.empty()) {
      return;
    }
    auto fieldName = source.planNode->outputType()->nameOf(ordinal);
    for (auto i = 0; i < sources.size(); ++i) {
      auto& type = sources[i]->outputType();
      auto maybeIdx = type->getChildIdxIfExists(fieldName);
      if (maybeIdx.has_value()) {
        ContextSource s{.planNode = sources[i].get()};
        markFieldAccessed(s, maybeIdx.value(), steps, isControl);
        return;
      }
    }
    VELOX_FAIL("Should have found source for expr");
  }
  VELOX_NYI("no lambda");
}

void Optimization::markSubfields(
				 const core::ITypedExpr* expr,
    std::vector<Step>& steps,
    bool isControl,
    const std::vector<const RowType*> context,
    const std::vector<ContextSource>& sources) {
  if (auto* field = dynamic_cast<const core::FieldAccessTypedExpr*>(expr)) {
    auto* input = field->inputs().empty() ? nullptr : field->inputs()[0].get();
    bool isLeaf =
      !input || dynamic_cast<const core::InputTypedExpr*>(input) != nullptr;
    if (isLeaf) {
      for (auto i = 0; i < sources.size(); ++i) {
        auto maybeIdx = context[i]->getChildIdxIfExists(field->name());
        if (maybeIdx.has_value())
          if (maybeIdx.has_value()) {
            auto source = sources[i];
            markFieldAccessed(source,  maybeIdx.value(), steps, isControl);
            return;
          }
      }
      VELOX_FAIL("Field not found {}", field->name());
    }
    steps.push_back(
        Step{.kind = StepKind::kField, .field = toName(field->name())});
    markSubfields(input, steps, isControl, context, sources);
    steps.pop_back();
    return;
    if (auto* call = dynamic_cast<const core::CallTypedExpr*>(expr)) {
      auto& name = call->name();
      if (name == "cardinality") {
        steps.push_back(Step{.kind = StepKind::kCardinality});
        markSubfields(
            call->inputs()[0].get(), steps, isControl, context, sources);
        steps.pop_back();
        return;
      }
      if (name == "subscript") {
        auto constant = foldConstant(call->inputs()[1]);
        if (!constant) {
	  std::vector<Step> subSteps;
          markSubfields(call->inputs()[1].get(), subSteps, isControl, context, sources);
          steps.push_back(
			  Step{.kind = StepKind::kSubscript, .allFields = true});
	  markSubfields(
			call->inputs()[0].get(), steps, isControl, context, sources);
          steps.pop_back();
          return;
        }
        auto value = constant->value();
        if (value->type()->kind() == TypeKind::VARCHAR) {
          std::string str = value->as<ConstantVector<StringView>>()->valueAt(0);
          steps.push_back(
              Step{.kind = StepKind::kSubscript, .field = toName(str)});
          markSubfields(
              call->inputs()[0].get(), steps, isControl, context, sources);
          steps.pop_back();
          return;
        }
        auto id = integerValue(constant->value().get());
        steps.push_back(Step{.kind = StepKind::kSubscript, .id = id});
        markSubfields(
            call->inputs()[0].get(), steps, isControl, context, sources);
        steps.pop_back();
      }
      return;
    }
  }
}

  void Optimization::markTopOutputs(const RowType* type, const core::PlanNode* node) {
    ContextSource source = {.planNode = node};
    for (auto i = 0; i < type->size(); ++i) {
    std::vector<Step> steps;
      markFieldAccessed(source, i, steps, false);
  }
}

} // namespace facebook::velox::optimizer
