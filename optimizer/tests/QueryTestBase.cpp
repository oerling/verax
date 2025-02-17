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

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/init/Init.h>
#include <gflags/gflags.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "optimizer/connectors/hive/LocalHiveConnectorMetadata.h" //@manual
#include "velox/common/base/SuccinctPrinter.h"
#include "velox/common/file/FileSystems.h"
#include "velox/common/memory/MmapAllocator.h"
#include "velox/connectors/hive/HiveConnector.h"
#include "velox/dwio/common/Options.h"
#include "velox/dwio/dwrf/RegisterDwrfReader.h"
#include "velox/dwio/dwrf/reader/DwrfReader.h"
#include "velox/dwio/parquet/RegisterParquetReader.h"
#include "velox/exec/Exchange.h"

#include "optimizer/Plan.h" //@manual
#include "optimizer/SchemaResolver.h" //@manual
#include "optimizer/VeloxHistory.h" //@manual
#include "optimizer/connectors/ConnectorSplitSource.h" //@manual
#include "velox/exec/PlanNodeStats.h"
#include "velox/exec/Split.h"
#include "velox/exec/tests/utils/HiveConnectorTestBase.h"
#include "velox/exec/tests/utils/LocalExchangeSource.h"
#include "velox/expression/Expr.h"
#include "velox/functions/prestosql/aggregates/RegisterAggregateFunctions.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/parse/QueryPlanner.h"
#include "velox/parse/TypeResolver.h"
#include "velox/runner/LocalRunner.h"
#include "velox/serializers/PrestoSerializer.h"
#include "velox/vector/VectorSaver.h"

DEFINE_string(
    data_path,
    "",
    "Root path of data. Data layout must follow Hive-style partitioning. ");


DEFINE_int32(optimizer_trace, 0, "Optimizer trace level");

DEFINE_bool(print_plan, false, "Print optimizer results");

DEFINE_int32(num_drivers, 4, "Number of drivers");
DEFINE_int32(num_workers, 4, "Number of in-process workers");

DEFINE_string(data_format, "parquet", "Data format");


namespace facebook::velox::optimizer {

void QueryTestBase::SetUp() {
  memory::MemoryManager::testingSetInstance({});

    rootPool_ = memory::memoryManager()->addRootPool("velox_sql");

    optimizerPool_ = rootPool_->addLeafChild("optimizer");
    schemaPool_ = rootPool_->addLeafChild("schema");
    checkPool_ = rootPool_->addLeafChild("check");

    functions::prestosql::registerAllScalarFunctions();
    aggregate::prestosql::registerAllAggregateFunctions();
    parse::registerTypeResolver();
    filesystems::registerLocalFileSystem();
    parquet::registerParquetReaderFactory();
    dwrf::registerDwrfReaderFactory();
    exec::ExchangeSource::registerFactory(
        exec::test::createLocalExchangeSource);
    serializer::presto::PrestoVectorSerde::registerVectorSerde();
    if (!isRegisteredNamedVectorSerde(VectorSerde::Kind::kPresto)) {
      serializer::presto::PrestoVectorSerde::registerNamedVectorSerde();
    }
    ioExecutor_ = std::make_unique<folly::IOThreadPoolExecutor>(8);
    std::unordered_map<std::string, std::string> connectorConfig;
    connectorConfig[connector::hive::HiveConfig::kLocalDataPath] =
        FLAGS_data_path;
    connectorConfig[connector::hive::HiveConfig::kLocalFileFormat] =
        FLAGS_data_format;
    auto config =
        std::make_shared<config::ConfigBase>(std::move(connectorConfig));
    connector::registerConnectorFactory(
        std::make_shared<connector::hive::HiveConnectorFactory>());
    connector_ =
        connector::getConnectorFactory(
            connector::hive::HiveConnectorFactory::kHiveConnectorName)
            ->newConnector(kHiveConnectorId, config, ioExecutor_.get());
    connector::registerConnector(connector_);

    std::unordered_map<std::string, std::shared_ptr<config::ConfigBase>>
        connectorConfigs;
    auto copy = hiveConfig_;
    connectorConfigs[kHiveConnectorId] =
        std::make_shared<config::ConfigBase>(std::move(copy));

    schemaQueryCtx_ = core::QueryCtx::create(
        executor_.get(),
        core::QueryConfig(config_),
        std::move(connectorConfigs),
        cache::AsyncDataCache::getInstance(),
        rootPool_->shared_from_this(),
        spillExecutor_.get(),
        "schema");
    common::SpillConfig spillConfig;
    common::PrefixSortConfig prefixSortConfig;

    schemaRootPool_ = rootPool_->addAggregateChild("schemaRoot");
    connectorQueryCtx_ = std::make_shared<connector::ConnectorQueryCtx>(
        schemaPool_.get(),
        schemaRootPool_.get(),
        schemaQueryCtx_->connectorSessionProperties(kHiveConnectorId),
        &spillConfig,
        prefixSortConfig,
        std::make_unique<exec::SimpleExpressionEvaluator>(
            schemaQueryCtx_.get(), schemaPool_.get()),
        schemaQueryCtx_->cache(),
        "scan_for_schema",
        "schema",
        "N/a",
        0,
        schemaQueryCtx_->queryConfig().sessionTimezone());

    schema_ = std::make_shared<facebook::velox::optimizer::SchemaResolver>(
        connector_, "");

    planner_ = std::make_unique<core::DuckDbQueryPlanner>(optimizerPool_.get());
    auto& tables = dynamic_cast<connector::hive::LocalHiveConnectorMetadata*>(
                       connector_->metadata())
                       ->tables();
    for (auto& pair : tables) {
      planner_->registerTable(pair.first, pair.second->rowType());
    }
    planner_->registerTableScan(
        [this](
            const std::string& id,
            const std::string& name,
            const RowTypePtr& rowType,
            const std::vector<std::string>& columnNames) {
          return toTableScan(id, name, rowType, columnNames);
        });
    history_ = std::make_unique<facebook::velox::optimizer::VeloxHistory>();
    executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(
        FLAGS_num_drivers * 2 + 2);
    spillExecutor_ = std::make_shared<folly::IOThreadPoolExecutor>(4);
  }
}
