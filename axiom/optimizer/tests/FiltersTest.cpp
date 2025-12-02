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

#include "axiom/optimizer/Filters.h"
#include <folly/init/Init.h>
#include <gtest/gtest.h>
#include "axiom/optimizer/Optimization.h"
#include "axiom/optimizer/Plan.h"
#include "axiom/optimizer/QueryGraph.h"
#include "axiom/optimizer/tests/HiveQueriesTestBase.h"
#include "axiom/sql/presto/PrestoParser.h"
#include "velox/expression/Expr.h"

namespace facebook::axiom::optimizer::test {

using namespace facebook::velox;

class FiltersTest : public HiveQueriesTestBase {
 protected:
  static void SetUpTestCase() {
    HiveQueriesTestBase::SetUpTestCase();
  }

  static void TearDownTestCase() {
    HiveQueriesTestBase::TearDownTestCase();
  }

  void SetUp() override {
    HiveQueriesTestBase::SetUp();
  }

  void TearDown() override {
    // Reset optimization and dependencies
    optimization_.reset();
    session_.reset();
    allocator_.reset();
    context_.reset();

    // Clean up queryCtx
    optimizer::queryCtx() = nullptr;
    queryCtx_ = nullptr;

    HiveQueriesTestBase::TearDown();
  }

  /// Parses SQL and creates an Optimization that stays live for the TEST_F.
  void optimize(std::string_view sql) {
    // Clean up any previous optimization state
    if (queryCtx_) {
      optimizer::queryCtx() = nullptr;
      queryCtx_ = nullptr;
    }
    optimization_.reset();
    session_.reset();
    allocator_.reset();
    context_.reset();

    // Parse the SQL query
    auto logicalPlan = parseSelect(sql);
    VELOX_CHECK_NOT_NULL(logicalPlan);

    // Create the optimizer context and dependencies
    auto& veloxQueryCtx = getQueryCtx();

    allocator_ = std::make_unique<HashStringAllocator>(optimizerPool_.get());
    context_ = std::make_unique<optimizer::QueryGraphContext>(*allocator_);
    queryCtx_ = context_.get();

    // Set the thread-local queryCtx
    optimizer::queryCtx() = queryCtx_;

    exec::SimpleExpressionEvaluator evaluator(
        veloxQueryCtx.get(), optimizerPool_.get());

    session_ = std::make_shared<Session>(veloxQueryCtx->queryId());

    connector::SchemaResolver schemaResolver;

    runner::MultiFragmentPlan::Options options;
    options.numWorkers = 1;
    options.numDrivers = 1;

    // Create the Optimization and keep it alive
    optimization_ = std::make_unique<optimizer::Optimization>(
        session_,
        *logicalPlan,
        schemaResolver,
        *history_,
        veloxQueryCtx,
        evaluator,
        optimizerOptions_,
        options);
  }

  // Members that stay alive for the duration of TEST_F
  std::unique_ptr<HashStringAllocator> allocator_;
  std::unique_ptr<optimizer::QueryGraphContext> context_;
  optimizer::QueryGraphContext* queryCtx_{nullptr};
  std::shared_ptr<Session> session_;
  std::unique_ptr<optimizer::Optimization> optimization_;

  /// Helper to find a BaseTable by name in a DerivedTable.
  /// Fails the test if the table is not found.
  const BaseTable* findBaseTable(
      DerivedTableCP dt,
      std::string_view tableName) {
    for (auto* table : dt->tables) {
      if (table->is(PlanType::kTableNode)) {
        auto* bt = table->as<BaseTable>();
        if (bt->schemaTable && bt->schemaTable->name() == tableName) {
          return bt;
        }
      }
    }
    ADD_FAILURE() << "Table '" << tableName << "' not found in DerivedTable";
    return nullptr;
  }
};

TEST_F(FiltersTest, basic) {
  // SQL query with filters on lineitem
  optimize(
      "select l_shipdate, l_partkey, l_suppkey "
      "from lineitem "
      "where l_shipdate between cast('1995-01-01' as date) and cast('1995-06-01' as date) "
      "and l_partkey < 10000 and l_suppkey > 10");

  // Access the root derived table
  auto* rootDt = optimization_->rootDt();

  const BaseTable* lineitemTable = findBaseTable(rootDt, "lineitem");
  ASSERT_NE(lineitemTable, nullptr);

  // Check that we have filters
  bool hasFilters =
      !lineitemTable->columnFilters.empty() || !lineitemTable->filter.empty();
  ASSERT_TRUE(hasFilters) << "Expected filters on lineitem table";

  // Create a temporary PlanState
  PlanState state(*optimization_, rootDt);

  // Combine all filters to test
  ExprVector allFilters = lineitemTable->columnFilters;
  ASSERT_EQ(4, allFilters.size());

  ConstraintMap constraints;

  float selectivity =
      conjunctsSelectivity(state, allFilters, true, constraints);

  // Selectivity should be in valid range [0, 1]
  EXPECT_GE(selectivity, 0.0f);
  EXPECT_LE(selectivity, 1.0f);
}

TEST_F(FiltersTest, combineRanges) {
  // SQL query with overlapping between clauses on nation table
  optimize(
      "select * from nation "
      "where n_nationkey between 2 and 15 "
      "and n_nationkey between 5 and 20");

  // Access the root derived table
  auto* rootDt = optimization_->rootDt();
  ASSERT_NE(rootDt, nullptr);
  ASSERT_FALSE(rootDt->tables.empty());

  // Find the BaseTable for nation using helper
  const BaseTable* nationTable = findBaseTable(rootDt, "nation");
  ASSERT_NE(nationTable, nullptr);

  // Check that we have filters
  bool hasFilters =
      !nationTable->columnFilters.empty() || !nationTable->filter.empty();
  ASSERT_TRUE(hasFilters) << "Expected filters on nation table";

  // Create a temporary PlanState
  PlanState state(*optimization_, rootDt);

  // Combine all filters to test
  ExprVector allFilters;
  allFilters.insert(
      allFilters.end(),
      nationTable->columnFilters.begin(),
      nationTable->columnFilters.end());
  allFilters.insert(
      allFilters.end(), nationTable->filter.begin(), nationTable->filter.end());

  ASSERT_FALSE(allFilters.empty()) << "Expected at least one filter expression";

  // Test conjunctsSelectivity on combined filters with constraint tightening
  ConstraintMap constraints;
  float selectivity =
      conjunctsSelectivity(state, allFilters, true, constraints);

  // Selectivity should be 20/25 = 0.8 within 1%
  float expectedSelectivity = 20.0f / 25.0f;
  EXPECT_NEAR(selectivity, expectedSelectivity, 0.01f)
      << "Expected selectivity around " << expectedSelectivity << " but got "
      << selectivity;

  // Check that there is exactly one constraint for n_nationkey
  EXPECT_EQ(constraints.size(), 1)
      << "Expected exactly one constraint for n_nationkey";

  if (!constraints.empty()) {
    // Get the constraint value
    const auto& constraintValue = constraints.begin()->second;

    // Check min = 5
    ASSERT_NE(constraintValue.min, nullptr) << "Expected min to be set";
    auto minValue = constraintValue.min->value<velox::TypeKind::BIGINT>();
    EXPECT_EQ(minValue, 5) << "Expected min to be 5, got " << minValue;

    // Check max = 15
    ASSERT_NE(constraintValue.max, nullptr) << "Expected max to be set";
    auto maxValue = constraintValue.max->value<velox::TypeKind::BIGINT>();
    EXPECT_EQ(maxValue, 15) << "Expected max to be 15, got " << maxValue;

    // Check cardinality = 11
    EXPECT_EQ(constraintValue.cardinality, 11.0f)
        << "Expected cardinality to be 11, got " << constraintValue.cardinality;

    LOG(INFO) << "Constraint: min=" << minValue << ", max=" << maxValue
              << ", cardinality=" << constraintValue.cardinality;
  }
}

} // namespace facebook::axiom::optimizer::test

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::Init init(&argc, &argv, false);
  return RUN_ALL_TESTS();
}
