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
#include "velox/common/base/Exceptions.h"

#include <gtest/gtest.h>
#include <sstream>

using namespace facebook::velox;
using namespace facebook::velox::optimizer;

class ModelTest : public testing::Test {
 protected:
  static float qs(Model* m, const char* chars) {
    std::vector<float> point;
    std::string str(chars);
    std::istringstream in(str);
    float f;
    while (in >> f) {
      point.push_back(f);
    }
    if (point.size() != m->rank()) {
      std::cout << "Expect " << m->rank() << " dims.\n";
      return -1;
    }
    f = m->query(point);
    std::cout << f << " at {";
    for (auto c : point) {
      std::cout << c << " ";
    }
    std::cout << "}\n";
    return f;
  }
};

static float absv(float x) {
  return x > 0 ? x : -x;
}

static float weighted(float x1, float x2, float m1, float m2, float x) {
  float w1 = 1.0 / absv(x1 - x);
  float w2 = 1.0 / absv(x2 - x);
  return m1 * w1 + m2 * w2 / (w1 + w2);
}

TEST_F(ModelTest, dim1) {
  Model m1(1);
  m1.insert({10}, 10);
  m1.insert({20}, 20);
  m1.insert({30}, 40);
  m1.precompute();
  EXPECT_THROW(m1.query({5}), VeloxException);
  EXPECT_EQ(15, m1.query({15}));
  EXPECT_EQ(20, m1.query({20}));
  EXPECT_EQ(30, m1.query({25}));
  EXPECT_EQ(50, m1.query({35}));
}

TEST_F(ModelTest, dim2) {
  Model m(2);
  m.insert({10, 1}, 10);
  m.insert({20, 1}, 20);
  m.insert({30, 1}, 40);
  m.insert({40, 1}, 100);

  m.insert({10, 2}, 12);
  m.insert({10, 3}, 14);
  m.insert({10, 5}, 20);
  m.precompute();
  EXPECT_EQ(5, m.query({5, 1}));
  EXPECT_EQ(15, m.query({15, 1}));
  EXPECT_EQ(20, m.query({20, 1}));
  EXPECT_EQ(30, m.query({25}));
  EXPECT_EQ(50, m.query({35}));
}

TEST_F(ModelTest, dim3) {
  Model m(3);
  for (auto i = 0; i < 4; ++i) {
    for (auto j = 0; j < 4; ++j) {
      for (auto k = 0; k < 4; ++k) {
        m.insert({i, j, k}, (i + 1) + (j + 1) * 2 + (k + 1) * 3);
      }
    }
  }
  m.precompute();
}
