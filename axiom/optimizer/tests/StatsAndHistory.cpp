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

#include "axiom/common/Session.h"
#include "axiom/connectors/SchemaResolver.h"
#include "axiom/connectors/hive/LocalHiveConnectorMetadata.h"
#include "axiom/logical_plan/PlanBuilder.h"
#include "axiom/optimizer/FunctionRegistry.h"
#include "axiom/optimizer/Optimization.h"
#include "axiom/optimizer/QueryGraphContext.h"
#include "axiom/optimizer/VeloxHistory.h"
#include "axiom/optimizer/tests/HiveQueriesTestBase.h"
#include "axiom/runner/LocalRunner.h"
#include "velox/common/config/Config.h"
#include "velox/common/file/FileSystems.h"
#include "velox/common/memory/HashStringAllocator.h"
#include "velox/connectors/hive/HiveConfig.h"
#include "velox/connectors/hive/HiveConnector.h"
#include "velox/connectors/hive/TableHandle.h"
#include "velox/dwio/parquet/RegisterParquetReader.h"
#include "velox/dwio/parquet/RegisterParquetWriter.h"
#include "velox/exec/tests/utils/AssertQueryBuilder.h"
#include "velox/exec/tests/utils/PlanBuilder.h"
#include "velox/expression/Expr.h"
#include "velox/expression/ExprToSubfieldFilter.h"
#include "velox/type/tests/SubfieldFiltersBuilder.h"

#include <folly/Hash.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/init/Init.h>
#include <algorithm>

namespace facebook::axiom::optimizer::test {

using namespace facebook::velox;
using namespace facebook::velox::common::test;
namespace lp = facebook::axiom::logical_plan;

namespace {

// Create the history_data table in the specified directory.
// Similar to ParquetTpchTest::createTables, this is a standalone function
// that creates necessary objects directly without inheriting from test base.
void createHistoryDataTable(std::string_view path) {
  SCOPE_EXIT {
    connector::ConnectorMetadata::unregisterMetadata(
        exec::test::PlanBuilder::kHiveDefaultConnectorId);
    velox::connector::unregisterConnector(
        std::string(exec::test::PlanBuilder::kHiveDefaultConnectorId));
    parquet::unregisterParquetReaderFactory();
    parquet::unregisterParquetWriterFactory();
  };

  filesystems::registerLocalFileSystem();
  dwio::common::registerFileSinks();

  parquet::registerParquetReaderFactory();
  parquet::registerParquetWriterFactory();

  // Register Hive connector with data path
  std::unordered_map<std::string, std::string> configs;
  configs[velox::connector::hive::HiveConfig::kLocalDataPath] =
      std::string(path);
  configs[velox::connector::hive::HiveConfig::kLocalFileFormat] =
      velox::dwio::common::toString(velox::dwio::common::FileFormat::PARQUET);

  auto hiveConfig = std::make_shared<config::ConfigBase>(std::move(configs));

  velox::connector::hive::HiveConnectorFactory hiveConnectorFactory;
  auto hiveConnector = hiveConnectorFactory.newConnector(
      std::string(exec::test::PlanBuilder::kHiveDefaultConnectorId),
      hiveConfig);
  velox::connector::registerConnector(std::move(hiveConnector));

  // Register LocalHiveConnectorMetadata
  auto connector = velox::connector::getConnector(
      std::string(exec::test::PlanBuilder::kHiveDefaultConnectorId));
  connector::ConnectorMetadata::registerMetadata(
      exec::test::PlanBuilder::kHiveDefaultConnectorId,
      std::make_shared<connector::hive::LocalHiveConnectorMetadata>(
          dynamic_cast<velox::connector::hive::HiveConnector*>(
              connector.get())));

  // Create memory pools
  auto rootPool = memory::memoryManager()->addRootPool();
  auto pool = rootPool->addLeafChild("leaf");

  // Create table structure
  auto tableType = ROW(
      {{"sequence", BIGINT()},
       {"user_id", BIGINT()},
       {"event_date", BIGINT()},
       {"event_time", BIGINT()},
       {"ds", VARCHAR()},
       {"ts", VARCHAR()},
       {"product", VARCHAR()}});

  // Create dataset with 50,000 rows
  constexpr int64_t kTotalRows = 50000;
  constexpr int64_t kRowsPerDay = 10000;

  // Unix timestamp for 2025-10-10 00:00:00 UTC
  constexpr int64_t kBaseUnixTime = 1760140800;

  // Create vectors for all columns
  std::vector<int64_t> sequenceData;
  std::vector<int64_t> userIdData;
  std::vector<int64_t> eventDateData;
  std::vector<int64_t> eventTimeData;
  std::vector<std::string> dsData;
  std::vector<std::string> tsData;
  std::vector<std::string> productData;

  sequenceData.reserve(kTotalRows);
  userIdData.reserve(kTotalRows);
  eventDateData.reserve(kTotalRows);
  eventTimeData.reserve(kTotalRows);
  dsData.reserve(kTotalRows);
  tsData.reserve(kTotalRows);
  productData.reserve(kTotalRows);

  for (int64_t seq = 0; seq < kTotalRows; ++seq) {
    // sequence is the row number
    sequenceData.push_back(seq);

    // user_id: generate values spread between 1000 and 950000
    // Use a hash-like distribution
    int64_t userId = 1000 + (seq * 997) % 949000;
    userIdData.push_back(userId);

    // ds: '2025-10-10' for first 10000 rows, incrementing day for next 10000
    int day = seq / kRowsPerDay;
    std::string ds = fmt::format("2025-10-{:02d}", 10 + day);
    dsData.push_back(ds);

    // ts: ds_XX where XX = (sequence % 10000) / 24 / 6, 4 6 hour ts per day.
    int64_t hour = (seq % kRowsPerDay) / (kRowsPerDay / 24) / 6;
    std::string ts = fmt::format("{}_{:02d}", ds, std::min<int32_t>(3, hour));
    tsData.push_back(ts);

    // product: 'p1' for seq % 10 <= 6, 'p2' for 7-8, 'p3' for 9
    int mod10 = seq % 10;
    std::string product;
    if (mod10 <= 6) {
      product = "p1";
    } else if (mod10 <= 8) {
      product = "p2";
    } else {
      product = "p3";
    }
    productData.push_back(product);

    // event_date: ds as integer date (YYYYMMDD)
    int64_t eventDate = 20251010 + day;
    eventDateData.push_back(eventDate);

    // event_time: base unix time for the day + seconds within the day
    // seconds = (seq % 10000) * (24 * 60 * 60 / 10000)
    double secondsInDay =
        (seq % kRowsPerDay) * (24.0 * 60.0 * 60.0 / kRowsPerDay);
    int64_t eventTime = kBaseUnixTime + (day * 24 * 60 * 60) +
        static_cast<int64_t>(secondsInDay);
    eventTimeData.push_back(eventTime);
  }

  // Create row vector with the data using BaseVector::create
  auto sequenceVector = BaseVector::create(BIGINT(), kTotalRows, pool.get());
  auto userIdVector = BaseVector::create(BIGINT(), kTotalRows, pool.get());
  auto eventDateVector = BaseVector::create(BIGINT(), kTotalRows, pool.get());
  auto eventTimeVector = BaseVector::create(BIGINT(), kTotalRows, pool.get());
  auto dsVector = BaseVector::create(VARCHAR(), kTotalRows, pool.get());
  auto tsVector = BaseVector::create(VARCHAR(), kTotalRows, pool.get());
  auto productVector = BaseVector::create(VARCHAR(), kTotalRows, pool.get());

  auto* sequenceFlat = sequenceVector->asFlatVector<int64_t>();
  auto* userIdFlat = userIdVector->asFlatVector<int64_t>();
  auto* eventDateFlat = eventDateVector->asFlatVector<int64_t>();
  auto* eventTimeFlat = eventTimeVector->asFlatVector<int64_t>();
  auto* dsFlat = dsVector->asFlatVector<StringView>();
  auto* tsFlat = tsVector->asFlatVector<StringView>();
  auto* productFlat = productVector->asFlatVector<StringView>();

  for (int64_t i = 0; i < kTotalRows; ++i) {
    sequenceFlat->set(i, sequenceData[i]);
    userIdFlat->set(i, userIdData[i]);
    eventDateFlat->set(i, eventDateData[i]);
    eventTimeFlat->set(i, eventTimeData[i]);
    dsFlat->set(i, StringView(dsData[i]));
    tsFlat->set(i, StringView(tsData[i]));
    productFlat->set(i, StringView(productData[i]));
  }

  auto data = std::make_shared<RowVector>(
      pool.get(),
      tableType,
      BufferPtr(nullptr),
      kTotalRows,
      std::vector<VectorPtr>{
          sequenceVector,
          userIdVector,
          eventDateVector,
          eventTimeVector,
          dsVector,
          tsVector,
          productVector});

  // Write data to table using logical plan and optimizer
  const auto tableDirectory = fmt::format("{}/history_data", std::string(path));

  // Create table in metadata with partition properties
  auto connectorId =
      std::string(exec::test::PlanBuilder::kHiveDefaultConnectorId);
  auto metadata = connector::ConnectorMetadata::metadata(connectorId);
  auto* hiveMetadata =
      dynamic_cast<connector::hive::LocalHiveConnectorMetadata*>(metadata);
  VELOX_CHECK_NOT_NULL(hiveMetadata, "Expected LocalHiveConnectorMetadata");

  auto session = std::make_shared<connector::ConnectorSession>("test");

  // Drop table if exists and create new one
  hiveMetadata->dropTableIfExists("history_data");

  folly::F14FastMap<std::string, velox::Variant> tableOptions;
  tableOptions["partitioned_by"] = velox::Variant::array(
      {velox::variant("ds"), velox::variant("ts"), velox::variant("product")});

  auto table = hiveMetadata->createTable(
      session, "history_data", tableType, tableOptions);

  // Begin write operation
  auto writeHandle =
      hiveMetadata->beginWrite(session, table, connector::WriteKind::kCreate);

  // Build logical plan for table write
  lp::PlanBuilder::Context context(connectorId);
  auto logicalPlan = lp::PlanBuilder(context)
                         .values({data})
                         .tableWrite(
                             "history_data",
                             lp::WriteKind::kCreate,
                             {"sequence",
                              "user_id",
                              "event_date",
                              "event_time",
                              "ds",
                              "ts",
                              "product"})
                         .build();

  // Create optimizer context objects (not inheriting from QueryTestBase)
  auto optimizerPool = rootPool->addLeafChild("optimizer");
  auto allocator =
      std::make_unique<velox::HashStringAllocator>(optimizerPool.get());
  auto context_graph =
      std::make_unique<optimizer::QueryGraphContext>(*allocator);
  optimizer::queryCtx() = context_graph.get();
  SCOPE_EXIT {
    optimizer::queryCtx() = nullptr;
  };

  // Register Presto functions
  optimizer::FunctionRegistry::registerPrestoFunctions();

  // Create executor for QueryCtx (required by LocalRunner)
  auto executor = std::make_unique<folly::CPUThreadPoolExecutor>(4);

  // Create QueryCtx with executor
  auto queryCtx = velox::core::QueryCtx::create(executor.get());

  // Create expression evaluator
  velox::exec::SimpleExpressionEvaluator evaluator(
      queryCtx.get(), optimizerPool.get());

  // Create session
  auto axiomSession = std::make_shared<axiom::Session>("history_data_creation");

  // Create history object
  optimizer::VeloxHistory history;

  // Create schema resolver and set target table
  connector::SchemaResolver schemaResolver;
  schemaResolver.setTargetTable(connectorId, table);

  // Create optimization
  optimizer::OptimizerOptions optimizerOptions;
  runner::MultiFragmentPlan::Options runnerOptions;
  runnerOptions.numWorkers = 1;
  runnerOptions.numDrivers = 1;
  runnerOptions.queryId = "create_history_data";

  optimizer::Optimization opt(
      axiomSession,
      *logicalPlan,
      schemaResolver,
      history,
      queryCtx,
      evaluator,
      optimizerOptions,
      runnerOptions);

  // Get optimized plan
  auto best = opt.bestPlan();
  auto planAndStats = opt.toVeloxPlan(best->op);

  // Execute the write plan using LocalRunner
  auto runner = std::make_shared<runner::LocalRunner>(
      planAndStats.plan,
      std::move(planAndStats.finishWrite),
      queryCtx,
      std::make_shared<runner::ConnectorSplitSourceFactory>(),
      optimizerPool);

  // Read results
  std::vector<RowVectorPtr> results;
  while (auto result = runner->next()) {
    results.push_back(result);
  }
  runner->waitForCompletion(50000);

  // Verify that all rows were written
  ASSERT_EQ(1, results.size());
  ASSERT_EQ(1, results[0]->size());
  const auto& child = results[0]->childAt(0);
  ASSERT_TRUE(child);
  ASSERT_EQ(1, child->size());
  const auto value = child->as<SimpleVector<int64_t>>()->valueAt(0);
  ASSERT_EQ(kTotalRows, value);

  // Finish write operation
  hiveMetadata->finishWrite(session, writeHandle, {}).get();

  LOG(INFO) << "Created history_data table with " << kTotalRows << " rows in "
            << tableDirectory;
}

} // namespace

class StatsAndHistoryTest : public HiveQueriesTestBase {
 protected:
  static void SetUpTestCase() {
    HiveQueriesTestBase::SetUpTestCase();

    // Create the history_data table in the shared temp directory
    // The directory was created and TPCH tables were populated by
    // HiveQueriesTestBase::SetUpTestCase()
    createHistoryDataTable(LocalRunnerTestBase::localDataPath_);
  }

  void SetUp() override {
    HiveQueriesTestBase::SetUp();
    parquet::registerParquetReaderFactory();
    parquet::registerParquetWriterFactory();
  }

  void TearDown() override {
    parquet::unregisterParquetReaderFactory();
    parquet::unregisterParquetWriterFactory();
    HiveQueriesTestBase::TearDown();
  }

  static void TearDownTestCase() {
    HiveQueriesTestBase::TearDownTestCase();
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

    // Expected: 5 days * 4 hours * 3 products = 60 partitions
    EXPECT_EQ(partitions.size(), 5 * 4 * 3);
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

    // Expected: 3 days * 4 hours * 2 products = 24 partitions
    EXPECT_EQ(partitions.size(), 3 * 4 * 2);
  }
}

TEST_F(StatsAndHistoryTest, partitionStats) {
  // Find the history_data table
  auto table = hiveMetadata().findTable("history_data");
  ASSERT_TRUE(table != nullptr);

  // Get split manager
  auto* splitManager = hiveMetadata().splitManager();
  ASSERT_NE(splitManager, nullptr);

  // List all partitions
  common::SubfieldFilters emptyFilters;
  auto tableHandle = std::make_shared<velox::connector::hive::HiveTableHandle>(
      exec::test::kHiveConnectorId,
      "history_data",
      true, // filterPushdownEnabled
      std::move(emptyFilters),
      nullptr); // remainingFilter

  auto partitions = splitManager->listPartitions(nullptr, tableHandle);
  ASSERT_EQ(partitions.size(), 5 * 4 * 3); // 60 partitions

  // Get partition statistics for all columns
  std::vector<std::string> columns = {
      "sequence", "user_id", "event_date", "event_time"};
  auto partitionStats =
      splitManager->getPartitionStatistics(partitions, columns);
  ASSERT_EQ(partitionStats.size(), partitions.size());

  // Verify statistics for each partition
  for (size_t i = 0; i < partitionStats.size(); ++i) {
    auto& stats = partitionStats[i];
    ASSERT_TRUE(stats != nullptr);
    ASSERT_EQ(stats->columns.size(), 4);
    ASSERT_EQ(stats->columnStatistics.size(), 4);

    // Find column statistics by name
    std::unordered_map<std::string, const connector::ColumnStatistics*>
        colStatsMap;
    for (size_t j = 0; j < stats->columns.size(); ++j) {
      colStatsMap[stats->columns[j]] = &stats->columnStatistics[j];
    }

    // Check user_id: min < 100000 and max > 900000
    auto userIdStats = colStatsMap["user_id"];
    ASSERT_TRUE(userIdStats != nullptr);
    ASSERT_TRUE(userIdStats->min.has_value());
    ASSERT_TRUE(userIdStats->max.has_value());
    EXPECT_LT(userIdStats->min.value().value<int64_t>(), 100000);
    EXPECT_GT(userIdStats->max.value().value<int64_t>(), 900000);

    // Check sequence statistics based on partition's ds value
    auto seqStats = colStatsMap["sequence"];
    ASSERT_TRUE(seqStats != nullptr);
    ASSERT_TRUE(seqStats->min.has_value());
    ASSERT_TRUE(seqStats->max.has_value());

    // Extract partition information to determine expected sequence ranges
    // Partitions are organized as: ds={date}/ts={date}_{hour}/product={product}
    auto* partition = dynamic_cast<const connector::hive::LocalHivePartition*>(
        partitions[i].get());
    ASSERT_TRUE(partition != nullptr);

    auto dsIt = partition->partitionKeys.find("ds");
    ASSERT_TRUE(dsIt != partition->partitionKeys.end());
    std::string ds = dsIt->second;

    // Parse day from ds (format: 2025-10-DD)
    int day = 0;
    if (ds == "2025-10-10")
      day = 0;
    else if (ds == "2025-10-11")
      day = 1;
    else if (ds == "2025-10-12")
      day = 2;
    else if (ds == "2025-10-13")
      day = 3;
    else if (ds == "2025-10-14")
      day = 4;

    // Expected sequence ranges for each day:
    // Day 0: [0, 9999]
    // Day 1: [10000, 19999]
    // Day 2: [20000, 29999]
    // etc.
    int64_t expectedMinBound = day * 10000;
    int64_t expectedMaxLower = day * 10000 + 9000;
    int64_t expectedMaxUpper = day * 10000 + 10000;

    int64_t seqMin = seqStats->min.value().value<int64_t>();
    int64_t seqMax = seqStats->max.value().value<int64_t>();

    EXPECT_LT(seqMin, expectedMinBound + 1000)
        << "Partition: " << partition->name;
    EXPECT_GT(seqMax, expectedMaxLower) << "Partition: " << partition->name;
    EXPECT_LT(seqMax, expectedMaxUpper) << "Partition: " << partition->name;
  }
}

TEST_F(StatsAndHistoryTest, samplePartitions) {
  // Find the history_data table
  auto table = hiveMetadata().findTable("history_data");
  ASSERT_TRUE(table != nullptr);

  // Get the table layout
  auto& layouts = table->layouts();
  ASSERT_EQ(layouts.size(), 1);
  auto* layout =
      dynamic_cast<const connector::hive::LocalHiveTableLayout*>(layouts[0]);
  ASSERT_TRUE(layout != nullptr);

  // Test 1: Sample with no filter - expect total rows between 45000 and 55000
  {
    common::SubfieldFilters emptyFilters;
    auto tableHandle =
        std::make_shared<velox::connector::hive::HiveTableHandle>(
            exec::test::kHiveConnectorId,
            "history_data",
            true, // filterPushdownEnabled
            std::move(emptyFilters),
            nullptr); // remainingFilter

    std::vector<velox::core::TypedExprPtr> extraFilters;
    std::vector<velox::common::Subfield> fields;
    std::vector<connector::ColumnStatistics> stats;

    auto result = layout->sample(
        tableHandle,
        100.0f, // 100% sample
        extraFilters,
        layout->rowType(),
        fields,
        nullptr,
        &stats);

    // Expected: ~50,000 rows (60 partitions * ~833 rows each)
    EXPECT_GE(result.first, 45000);
    EXPECT_LE(result.first, 55000);
    EXPECT_EQ(
        result.first, result.second); // No filter, so all scanned rows pass
  }

  // Test 2: Sample with user_id filter (100000-200000)
  // Expected: ~10% of total rows = 4500-5500 rows
  {
    auto filters = SubfieldFiltersBuilder()
                       .add("user_id", velox::exec::between(100000, 200000))
                       .build();

    auto tableHandle =
        std::make_shared<velox::connector::hive::HiveTableHandle>(
            exec::test::kHiveConnectorId,
            "history_data",
            true, // filterPushdownEnabled
            std::move(filters),
            nullptr); // remainingFilter

    std::vector<velox::core::TypedExprPtr> extraFilters;
    std::vector<velox::common::Subfield> fields;
    std::vector<connector::ColumnStatistics> stats;

    auto result = layout->sample(
        tableHandle,
        100.0f, // 100% sample
        extraFilters,
        layout->rowType(),
        fields,
        nullptr,
        &stats);

    // Expected: ~5,000 rows (10% of total due to user_id filter)
    EXPECT_GE(result.second, 4500);
    EXPECT_LE(result.second, 5500);
  }

  // Test 3: Sample with user_id filter AND product='p3'
  // Expected: ~450-550 rows (1/3 of partitions * 10% of rows)
  {
    auto filters = SubfieldFiltersBuilder()
                       .add("user_id", velox::exec::between(100000, 200000))
                       .add("product", velox::exec::equal("p3"))
                       .build();

    auto tableHandle =
        std::make_shared<velox::connector::hive::HiveTableHandle>(
            exec::test::kHiveConnectorId,
            "history_data",
            true, // filterPushdownEnabled
            std::move(filters),
            nullptr); // remainingFilter

    std::vector<velox::core::TypedExprPtr> extraFilters;
    std::vector<velox::common::Subfield> fields;
    std::vector<connector::ColumnStatistics> stats;

    auto result = layout->sample(
        tableHandle,
        100.0f, // 100% sample
        extraFilters,
        layout->rowType(),
        fields,
        nullptr,
        &stats);

    // Expected: ~500 rows (20 partitions with product='p3' * 10% of rows)
    EXPECT_GE(result.second, 450);
    EXPECT_LE(result.second, 550);
  }
}

} // namespace facebook::axiom::optimizer::test

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::Init init{&argc, &argv, false};
  return RUN_ALL_TESTS();
}
