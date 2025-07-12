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

#include "optimizer/Model.h" //@manual

#include <gtest/gtest.h>

using namespace facebook::velox::optimizer;

class ModelTest : public testing::Test {};

TEST_F(ModelTest, dim1) {
  Model m1(1);
  m1.insert({10}, 10);
  m1.insert({20}, 20);
  m1.insert({30}, 40);
  m1.precompute();
  EXPECT_EQ(5, m1.query({5}));
  EXPECT_EQ(15, m1.query({15}));
  EXPECT_EQ(20, m1.query({20}));
  EXPECT_EQ(30, m1.query({25}));
  EXPECT_EQ(50, m1.query({35}));
}
