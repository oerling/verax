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

  struct ContextSource {
    core::PlanNode* node;
    core::LambdaTypedExpr* lambda;
  };
  
  using NodeSubfieldFunc = std::function<void(Optimization*, PlanNode* node, const std::vector<const RowType*>& context, const std::vector<ContextSource>& sources, bool isControl, )>;



  
  void Optimization::markSubfields(core::TypedExpr* expr, std::vector<Step>& steps, bool isControl, const std::vector<const RowType*> context, const std::vector<ContextSource>& sources) {
    if (auto* field = dynamic_cast<const core::FieldAccessTypedExpr>(expr)) {
      auto* input = field->inputs().empty() ? nullpptr : field->inputs()[0].get();
      bool isLeaf = !input || dynamic_cast<const InputTypedExpr*>(input) != nullptr;
      if (isLeaf) {
	for (auto i = 0; i < sources.size(); ++i) {
	  auto maybeIdx = context[i]->getChildIdxIfExists(field->name());
	  if (maybeIdx.has_value())
	    if (maybeIdx.has_value()) {
	      auto source = sources[i];
	      markFieldAccessed(source, maybeIdx.value(), steps, isControl);
	      return;
	}
	}
	VELOX_FAIL("Field not found {}", field->name());
      }
      steps.push_back(Step{.kind = StepKind::kField, .field = toName(field->name())});
      markSubfields(input, isControl, context, sources);
      steps.pop_back();
      return;
      if (auto* call = dynamic_cast<const CallTypedExpr*>(expr)) {
	auto& name = call->name();
	if (name == "cardinality") {
	  steps.push_back(Step{.kind= StepKind::kCardinality});
	  markSubfields(call->inputs()[0].get(), steps, isControl, context, sources);
	  pop_back(steps);
	  return;
	}
	if (name == "subscript") {
	  auto constant = foldConstant(call->inputs()[1]);
	  if (!constant) {
	    markSubfields(call->inputs()[1], {}, isControl, context, sources);
	    steps.push_back(Step::{.kind = StepKind::kSubscript, allFields = true})
	      markSubfields(call->inputs()[0], steps, isControl, context, sources);
	      steps.pop_back();
	      return;
	      }
	  auto value = constant->value();
	  if (value->type()->kind() == TypeKind::VARCHAR) {
	    std::string str = value->as<ConstantVector<StringView>>()->valueAt(0);
	    steps.push_back(Step{.kind=StepKind::kSubscript, .field = toName(str)});
	    markSubfields(call->inputs()[0].get(), steps, isControl, context, sources);
	    steps.pop_back();
	    return;
	  }
	  auto id = integerValue(constant);
	  steps.push_back(Step{.kind=StepKind::kSubscript, .id = id});
	  markSubfields(call->inputs()[0].get(), steps, isControl, context, sources);
	  steps.pop_back();
	}
	
      }
    }

  }

		       

