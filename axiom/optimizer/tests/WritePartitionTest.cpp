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

#include "axiom/logical_plan/PlanBuilder.h"
#include "axiom/optimizer/tests/HiveQueriesTestBase.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
#include "axiom/optimizer/connectors/hive/HiveConnectorMetadata.h"

namespace lp = facebook::velox::logical_plan;

namespace facebook::velox::optimizer {
namespace {

class WritePartitionTest : public test::HiveQueriesTestBase {
protected:
  void SetUp() override {
    connector_ = connector::getConnector(velox::exec::test::kHiveConnectorId);
    metadata_ = dynamic_cast<connector::hive::HiveConnectorMetadata*>(
								      connector_->metadata());
  }

  std::vector<RowVectorPtr> makeTestData(int32_t numBatches, int32_t batchSize, int32_t dayOffset = 0) {
    std::vector<RowVectorPtr> data;
    for (auto i = 0; i < numBatches; ++i) {
      auto start = i * batchSize;
      data .push_back(makeRowVector({
      makeFlatVector<int64_t>(batchSize, [](auto row) { return row + start; }),
      makeFlatVector<int32_t>(batchSize, [](auto row) { return (row + start)% 19; }),
      makeFlatVector<int64_t>(batchSize, [](auto row) { return row + start + 2; }),
      makeFlatVector<StringView>(
          kTestSize,
          [](auto row) { return fmt::format("2025-09-{}", dayOffset + ((row + start) % 2)); }
	  })));
    }
    return data;
  }
  
  std::shared_ptr<connector::Connector> connector_;
  connector::ConnectorMetadata* metadata_;
  connector::ConnectorSessionPtr session{std::make_shared<connector::hive::HiveConnectorSession>()};

};

TEST_F(WritePartitionTest, copartition) {
  lp::PlanBuilder::Context context(exec::test::kHiveConnectorId);

    constexpr int32_t kTestBatchSize = 2048;

  auto tableType = ROW(
      {{"key1", BIGINT()},
       {"key2", INTEGER()},
       {"data", BIGINT()},
       {"ds", VARCHAR()}});

  std::unordered_map<std::string, std::string> options = {
      {"bucketed_by", "key1,key2"},
      {"bucket_count", "16"},
      {"partitioned_by", "ds"},
      {"file_format", "parquet"},
      {"compression_kind", "snappy"}};


  metadata_->createTableWithOptions("test", tableType, options, session, false);

  auto data = makeTestData(10, kTestBatchSize);


  auto write1 = lp::PlanBuilder(context)
    .values({data})
    .tableWrite(kHiveConnectorId, "test", lp::WriteKind::kInsert, {"key1", "key2", "data", "ds"})
    .build();
  runVelox(write1);
  
  

}
  
