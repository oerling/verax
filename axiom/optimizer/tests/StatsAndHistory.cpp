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

#include "axiom/optimizer/tests/HiveQueriesTestBase.h"
#include "velox/connectors/hive/TableHandle.h"
#include "velox/dwio/parquet/RegisterParquetWriter.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
#include "velox/expression/ExprToSubfieldFilter.h"
#include "velox/type/tests/SubfieldFiltersBuilder.h"

#include <folly/Hash.h>
#include <folly/init/Init.h>

namespace facebook::axiom::optimizer::test {

using namespace facebook::velox;
using namespace facebook::velox::common::test;

class StatsAndHistoryTest : public HiveQueriesTestBase {
 protected:
  void SetUp() override {
    HiveQueriesTestBase::SetUp();
    parquet::registerParquetWriterFactory();
    makePartitions();
  }

  void TearDown() override {
    hiveMetadata().dropTableIfExists("history_data");
    parquet::unregisterParquetWriterFactory();
    HiveQueriesTestBase::TearDown();
  }

  void makePartitions() {
    // Create table structure and manually populate partition metadata
    auto tableType = ROW(
        {{"sequence", BIGINT()},
         {"user_id", BIGINT()},
         {"event_date", DATE()},
         {"event_time", BIGINT()},
         {"ds", VARCHAR()},
         {"ts", VARCHAR()},
         {"product", VARCHAR()}});

    folly::F14FastMap<std::string, velox::Variant> options = {
        {connector::hive::HiveWriteOptions::kPartitionedBy,
         velox::Variant::array({"ds", "ts", "product"})},
        {connector::hive::HiveWriteOptions::kFileFormat, "parquet"}};

    // Use createEmptyTable to properly register the table
    createEmptyTable("history_data", tableType, options);

    // Now get the table and manually add partition metadata for testing
    auto table = hiveMetadata().findTable("history_data");
    auto* layout = dynamic_cast<connector::hive::LocalHiveTableLayout*>(
        const_cast<connector::TableLayout*>(table->layouts()[0]));

    std::vector<std::shared_ptr<const connector::hive::LocalHivePartition>>
        partitions;

    // Create partitions for 5 days * 24 hours * 3 products = 360 partitions
    for (int day = 0; day < 5; day++) {
      std::string ds = fmt::format("2025-10-{:02d}", 10 + day);
      for (int hour = 0; hour < 24; hour++) {
        std::string ts = fmt::format("{}_{:02d}", ds, hour);
        for (const auto& product : {"p1", "p2", "p3"}) {
          auto partition =
              std::make_shared<connector::hive::LocalHivePartition>();
          partition->name =
              fmt::format("ds={}/ts={}/product={}", ds, ts, product);
          partition->path = fmt::format(
              "{}/{}",
              hiveMetadata().tablePath("history_data"),
              partition->name);
          partition->partitionKeys = {
              {"ds", ds}, {"ts", ts}, {"product", std::string(product)}};
          partition->stats.numRows = 100; // Dummy row count
          partitions.push_back(partition);
        }
      }
    }

    layout->setPartitions(std::move(partitions));
  }
};

TEST_F(StatsAndHistoryTest, partitions) {
  // Find the history_data table
  auto table = hiveMetadata().findTable("history_data");
  ASSERT_TRUE(table != nullptr);

  // Get the table layout
  auto& layouts = table->layouts();
  ASSERT_EQ(layouts.size(), 1);
  auto* layout =
      dynamic_cast<const connector::hive::LocalHiveTableLayout*>(layouts[0]);
  ASSERT_TRUE(layout != nullptr);

  // Get split manager
  auto* splitManager = hiveMetadata().splitManager();
  ASSERT_NE(splitManager, nullptr);

  // Test 1: List all partitions (no filters)
  {
    common::SubfieldFilters emptyFilters;
    auto tableHandle =
        std::make_shared<velox::connector::hive::HiveTableHandle>(
            exec::test::kHiveConnectorId,
            "history_data",
            true, // filterPushdownEnabled
            std::move(emptyFilters),
            nullptr); // remainingFilter

    auto partitions = splitManager->listPartitions(nullptr, tableHandle);

    // Expected: 5 days * 24 hours * 3 products = 360 partitions
    EXPECT_EQ(partitions.size(), 5 * 24 * 3);
  }

  // Test 2: List filtered partitions
  // ds between '2025-10-11' and '2025-10-13' AND product in ('p2', 'p3')
  {
    auto filters =
        SubfieldFiltersBuilder()
            .add("ds", velox::exec::between("2025-10-11", "2025-10-13"))
            .add("product", velox::exec::in({"p2", "p3"}))
            .build();

    auto tableHandle =
        std::make_shared<velox::connector::hive::HiveTableHandle>(
            exec::test::kHiveConnectorId,
            "history_data",
            true, // filterPushdownEnabled
            std::move(filters),
            nullptr); // remainingFilter

    auto partitions = splitManager->listPartitions(nullptr, tableHandle);

    // Expected: 3 days * 24 hours * 2 products = 144 partitions
    EXPECT_EQ(partitions.size(), 3 * 24 * 2);
  }
}

} // namespace facebook::axiom::optimizer::test

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::Init init{&argc, &argv, false};
  return RUN_ALL_TESTS();
}
