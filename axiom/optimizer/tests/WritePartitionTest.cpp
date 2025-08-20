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

namespace lp = facebook::velox::logical_plan;

namespace facebook::velox::optimizer {
namespace {

class WritePartitionTest : public test::HiveQueriesTestBase {
  void SetUp() override {
    connector_ = getConnector(velox::exec::test::kHiveConnectorId);
    metadata_ = dynamic_cast<connector::hive::HiveConnectorMetadata*>(
      connector->metadata());
  }

  connector::Connector* connector_;
  connector::ConnectorMetadata* metadata_;
  connector::ConnectorSessionPtr session{std:make_shared<connector::hive::HiveConnectorSession>()};

};

TEST_F(WritePartitionTest, copartition) {
  lp::PlanBuilder::Context context(exec::test::kHiveConnectorId);

    constexpr int32_t kTestSize = 20480;

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

  

  auto data = makeRowVector({
      makeFlatVector<int64_t>(kTestSize, [](auto row) { return row; }),
      makeFlatVector<int32_t>(kTestSize, [](auto row) { return row % 10; }),
      makeFlatVector<int64_t>(kTestSize, [](auto row) { return row + 2; }),
      makeFlatVector<StringView>(
          kTestSize,
          [](auto row) { return row % 2 == 0 ? "2022-09-01" : "2025-09-02"; }),
  });

  auto write1 = lp::PlanBuilder(ctx)
    .values({data})
    .tableWrite("test", {"key1", "key2", "data", "ds"})
    .build();
  
  

}
  
