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

<<<<<<< HEAD:frontend/optimizer/tests/Genies.h
#include "optimizer/FunctionRegistry.h" //@manual
#include "optimizer/tests/FeatureGen.h" //@manual

#include "optimizer/tests/QueryTestBase.h" //@manual
=======
#include "axiom/optimizer/FunctionRegistry.h"
#include "axiom/optimizer/tests/FeatureGen.h"
#include "axiom/optimizer/tests/QueryTestBase.h"
>>>>>>> main:axiom/optimizer/tests/Genies.h
#include "velox/common/base/tests/GTestUtils.h"

namespace facebook::velox::optimizer::test {
TypePtr makeGenieType();
void registerGenieUdfs();
} // namespace facebook::velox::optimizer::test
