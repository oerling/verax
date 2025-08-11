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

#include "axiom/optimizer/connectors/hive/LocalHiveConnectorMetadata.h"
#include "axiom/optimizer/connectors/ConnectorSplitSource.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/DistributedPlanBuilder.h"
#include "velox/exec/tests/utils/LocalRunnerTestBase.h"
#include "velox/exec/tests/utils/QueryAssertions.h"

using namespace facebook::velox;
using namespace facebook::velox::exec;
using namespace facebook::velox::exec::test;
using namespace facebook::velox::connector;

class HiveConnectorMetadataTest : public LocalRunnerTestBase {
 protected:
  static constexpr int32_t kNumFiles = 5;
  static constexpr int32_t kNumVectors = 5;
  static constexpr int32_t kRowsPerVector = 10000;
  static constexpr int32_t kNumRows = kNumFiles * kNumVectors * kRowsPerVector;

  static void SetUpTestCase() {
    // The lambdas will be run after this scope returns, so make captures
    // static.
    static int32_t counter1;
    // Clear 'counter1' so that --gtest_repeat runs get the same data.
    counter1 = 0;
    auto customize1 = [&](const RowVectorPtr& rows) {
      makeAscending(rows, counter1);
    };

    rowType_ = ROW({"c0"}, {BIGINT()});
    testTables_ = {TableSpec{
        .name = "T",
        .columns = rowType_,
        .rowsPerVector = kRowsPerVector,
        .numVectorsPerFile = kNumVectors,
        .numFiles = kNumFiles,
        .customizeData = customize1}};

    // Creates the data and schema from 'testTables_'. These are created on the
    // first test fixture initialization.
    LocalRunnerTestBase::SetUpTestCase();
  }

  static void makeAscending(const RowVectorPtr& rows, int32_t& counter) {
    auto ints = rows->childAt(0)->as<FlatVector<int64_t>>();
    for (auto i = 0; i < ints->size(); ++i) {
      ints->set(i, counter + i);
    }
    counter += ints->size();
  }

  inline static RowTypePtr rowType_;
};

TEST_F(HiveConnectorMetadataTest, basic) {
  auto connector = getConnector(kHiveConnectorId);
  auto metadata = connector->metadata();
  ASSERT_TRUE(metadata != nullptr);
  auto table = metadata->findTable("T");
  ASSERT_TRUE(table != nullptr);
  auto column = table->findColumn("c0");
  ASSERT_TRUE(column != nullptr);
  EXPECT_EQ(250'000, table->numRows());
  auto* layout = table->layouts()[0];
  auto columnHandle = metadata->createColumnHandle(*layout, "c0");
  std::vector<ColumnHandlePtr> columns = {columnHandle};
  std::vector<core::TypedExprPtr> filters;
  std::vector<core::TypedExprPtr> rejectedFilters;
  auto ctx = dynamic_cast<hive::LocalHiveConnectorMetadata*>(metadata)
                 ->connectorQueryCtx();

  auto tableHandle = metadata->createTableHandle(
      *layout, columns, *ctx->expressionEvaluator(), filters, rejectedFilters);
  EXPECT_TRUE(rejectedFilters.empty());
  std::vector<ColumnStatistics> stats;
  std::vector<common::Subfield> fields;
  auto c0 = common::Subfield::create("c0");
  fields.push_back(std::move(*c0));
  HashStringAllocator allocator(pool_.get());
  auto pair = layout->sample(
      tableHandle, 100, {}, layout->rowType(), fields, &allocator, &stats);
  EXPECT_EQ(250'000, pair.first);
  EXPECT_EQ(250'000, pair.second);
}

TEST_F(HiveConnectorMetadataTest, createTable) {
  constexpr int32_t kTestSize = 2048;
  auto connector = getConnector(kHiveConnectorId);
  auto metadata = dynamic_cast<connector::hive::HiveConnectorMetadata*>(
      connector->metadata());
  ASSERT_TRUE(metadata != nullptr);

  auto tableType = ROW(
      {{"key1", BIGINT()},
       {"key2", INTEGER()},
       {"data", BIGINT()},
       {"ds", VARCHAR()}});

  std::unordered_map<std::string, std::string> options = {
      {"bucketed_by", "key1"},
      {"sorted_by", "key1, key2"},
      {"bucket_count", "4"},
      {"partitioned_by", "ds"}};

  auto session = std::make_shared<connector::hive::HiveConnectorSession>();

  metadata->createTable("test", tableType, options, session, false);

  auto table = metadata->findTable("test");
  auto& layouts = table->layouts();
  ASSERT_EQ(1, layouts.size());
  auto* layout =
      dynamic_cast<const connector::hive::HiveTableLayout*>(layouts[0]);
  ASSERT_TRUE(layout != nullptr);
  auto& columns = layout->columns();
  ASSERT_EQ(4, columns.size());

  auto buckets = layout->partitionColumns();
  ASSERT_EQ(1, buckets.size());
  EXPECT_EQ(columns[0], buckets[0]);
  auto numBuckets = layout->numBuckets();
  EXPECT_EQ(4, numBuckets.value());

  auto sorting = layout->orderColumns();
  ASSERT_EQ(2, sorting.size());
  EXPECT_EQ(columns[0], sorting[0]);
  EXPECT_EQ(columns[1], sorting[1]);

  auto partition = layout->hivePartitionColumns();
  ASSERT_EQ(1, partition.size());
  EXPECT_EQ(columns[3], partition[0]);

  auto data = makeRowVector({
      makeFlatVector<int64_t>(kTestSize, [](auto row) { return row; }),
      makeFlatVector<int64_t>(kTestSize, [](auto row) { return row % 10; }),
      makeFlatVector<int64_t>(kTestSize, [](auto row) { return row + 2; }),
      makeFlatVector<StringView>(
          kTestSize,
          [](auto row) { return row % 2 == 0 ? "2022-09-01" : "2025-09-02"; }),
  });

  auto connectorHandle = metadata->createInsertTableHandle(
      *layouts[0], tableType, {}, WriteKind::kInsert, session);

  auto handle = std::make_shared<core::InsertTableHandle>(
      kHiveConnectorId, connectorHandle);

  std::vector<std::string> output = {
      "numWrittenRows", "fragment", "tableCommitContext"};
  std::vector<TypePtr> types = {BIGINT(), VARBINARY(), VARBINARY()};
  std::vector<core::FieldAccessTypedExprPtr> groupingKeys;
  // 2. partition columns
  for (auto i = 0; i < partition.size(); i++) {
    groupingKeys.emplace_back(
        std::make_shared<const core::FieldAccessTypedExpr>(
            partition[i]->type(), partition[i]->name()));
    output.emplace_back(partition[i]->name());
    types.emplace_back(partition[i]->type());
  }

  auto resultType = ROW(std::move(output), std::move(types));

  auto idGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  auto builder = exec::test::PlanBuilder(idGenerator).values({data});

  auto plan = std::make_shared<core::TableWriteNode>(
      idGenerator->next(),
      tableType,
      tableType->names(),
      nullptr,
      handle,
      false,
      resultType,
      connector::CommitStrategy::kNoCommit,
      builder.planNode());
  auto result = exec::test::AssertQueryBuilder(plan).copyResults(pool());
  metadata->finishWrite(*layout, connectorHandle, {result}, WriteKind::kInsert, session);

  std::string id = "readQ";
  runner::MultiFragmentPlan::Options runnerOptions = {
    .queryId = id, .numWorkers = 1, .numDrivers = 1};

  DistributedPlanBuilder rootBuilder(runnerOptions, idGenerator, pool_.get());
  rootBuilder.tableScan("test", tableType);
  auto readPlan = std::make_shared<runner::MultiFragmentPlan>(
							      rootBuilder.fragments(), std::move(runnerOptions));
  auto rootPool = memory::memoryManager()->addRootPool(
						       "readQ");

  auto splitSourceFactory = std::make_shared<connector::ConnectorSplitSourceFactory>();
  auto localRunner = std::make_shared<runner::LocalRunner>(
							   std::move(readPlan), makeQueryCtx(id, rootPool.get()), splitSourceFactory);
  auto results = readCursor(localRunner);
  exec::test::assertEqualResults({data}, results);
}
