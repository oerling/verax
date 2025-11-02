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

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>
#include <folly/init/Init.h>
#include <gflags/gflags.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <iostream>
#include "axiom/connectors/SchemaResolver.h"
#include "axiom/connectors/hive/LocalHiveConnectorMetadata.h"
#include "axiom/connectors/tpch/TpchConnectorMetadata.h"
#include "axiom/logical_plan/PlanPrinter.h"
#include "axiom/optimizer/ConstantExprEvaluator.h"
#include "axiom/optimizer/DerivedTablePrinter.h"
#include "axiom/optimizer/Optimization.h"
#include "axiom/optimizer/Plan.h"
#include "axiom/optimizer/RelationOpPrinter.h"
#include "axiom/optimizer/VeloxHistory.h"
#include "axiom/optimizer/tests/linenoise/linenoise.h"
#include "axiom/runner/LocalRunner.h"
#include "axiom/sql/presto/PrestoParser.h"
#include "velox/common/file/FileSystems.h"
#include "velox/connectors/hive/HiveConnector.h"
#include "velox/dwio/dwrf/RegisterDwrfReader.h"
#include "velox/dwio/dwrf/RegisterDwrfWriter.h"
#include "velox/dwio/parquet/RegisterParquetReader.h"
#include "velox/dwio/parquet/RegisterParquetWriter.h"
#include "velox/exec/PlanNodeStats.h"
#include "velox/exec/tests/utils/LocalExchangeSource.h"
#include "velox/expression/Expr.h"
#include "velox/functions/prestosql/aggregates/RegisterAggregateFunctions.h"
#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/parse/TypeResolver.h"
#include "velox/serializers/PrestoSerializer.h"

DEFINE_string(
    data_path,
    "",
    "Root path of data. Data layout must follow Hive-style partitioning. ");
DEFINE_string(data_format, "parquet", "Data format: parquet or dwrf.");

DEFINE_uint32(optimizer_trace, 0, "Optimizer trace level");

DEFINE_bool(
    enable_reducing_existences,
    true,
    "Enable adding reducing semijoins into hash builds,a aggregations etc.");

DEFINE_int32(max_rows, 100, "Max number of printed result rows");

DEFINE_int32(num_workers, 4, "Number of in-process workers");
DEFINE_int32(num_drivers, 4, "Number of drivers per worker");

DEFINE_int64(split_target_bytes, 16 << 20, "Approx bytes covered by one split");

DEFINE_string(
    query,
    "",
    "Text of query. If empty, reads ';' separated queries from standard input");

using namespace facebook::velox;

namespace facebook::axiom {

const char* helpText =
    "Axiom Interactive SQL\n"
    "\n"
    "Type SQL and end with ';'.\n"
    "To set a flag, type 'flag <gflag_name> = <value>;' Leave a space on either side of '='.\n"
    "\n"
    "Useful flags:\n"
    "\n"
    "num_workers - Make a distributed plan for this many workers. Runs it in-process with remote exchanges with serialization and passing data in memory. If num_workers is 1, makes single node plans without remote exchanges.\n"
    "\n"
    "num_drivers - Specifies the parallelism for workers. This many threads per pipeline per worker.\n"
    "\n";

static const std::string kHiveConnectorId = "hive";

class VeloxRunner {
 public:
  void initialize() {
    memory::MemoryManager::testingSetInstance(memory::MemoryManager::Options{});

    rootPool_ = memory::memoryManager()->addRootPool("axiom_sql");
    optimizerPool_ = rootPool_->addLeafChild("optimizer");

    functions::prestosql::registerAllScalarFunctions();
    aggregate::prestosql::registerAllAggregateFunctions();
    parse::registerTypeResolver();

    optimizer::FunctionRegistry::registerPrestoFunctions();

    filesystems::registerLocalFileSystem();
    dwio::common::registerFileSinks();
    parquet::registerParquetReaderFactory();
    parquet::registerParquetWriterFactory();
    dwrf::registerDwrfReaderFactory();
    dwrf::registerDwrfWriterFactory();
    exec::ExchangeSource::registerFactory(
        exec::test::createLocalExchangeSource);
    serializer::presto::PrestoVectorSerde::registerVectorSerde();
    if (!isRegisteredNamedVectorSerde(VectorSerde::Kind::kPresto)) {
      serializer::presto::PrestoVectorSerde::registerNamedVectorSerde();
    }

    if (!FLAGS_data_path.empty()) {
      connector_ = registerHiveConnector(FLAGS_data_path);
    } else {
      connector_ = registerTpchConnector();
    }

    schema_ = std::make_shared<connector::SchemaResolver>();

    prestoParser_ = std::make_unique<::axiom::sql::presto::PrestoParser>(
        connector_->connectorId(), optimizerPool_.get());

    history_ = std::make_unique<optimizer::VeloxHistory>();

    if (!FLAGS_data_path.empty()) {
      history_->updateFromFile(FLAGS_data_path + "/.history");
    }

    executor_ =
        std::make_shared<folly::CPUThreadPoolExecutor>(std::max<int32_t>(
            std::thread::hardware_concurrency() * 2,
            FLAGS_num_workers * FLAGS_num_drivers * 2 + 2));
    spillExecutor_ = std::make_shared<folly::IOThreadPoolExecutor>(4);
  }

  std::shared_ptr<velox::connector::Connector> registerTpchConnector() {
    auto emptyConfig = std::make_shared<config::ConfigBase>(
        std::unordered_map<std::string, std::string>{});

    velox::connector::tpch::TpchConnectorFactory factory;
    auto connector = factory.newConnector("tpch", emptyConfig);
    velox::connector::registerConnector(connector);

    connector::ConnectorMetadata::registerMetadata(
        connector->connectorId(),
        std::make_shared<connector::tpch::TpchConnectorMetadata>(
            dynamic_cast<velox::connector::tpch::TpchConnector*>(
                connector.get())));

    return connector;
  }

  std::shared_ptr<velox::connector::Connector> registerHiveConnector(
      const std::string& dataPath) {
    ioExecutor_ = std::make_unique<folly::IOThreadPoolExecutor>(8);

    std::unordered_map<std::string, std::string> connectorConfig = {
        {velox::connector::hive::HiveConfig::kLocalDataPath, dataPath},
        {velox::connector::hive::HiveConfig::kLocalFileFormat,
         FLAGS_data_format},
    };

    auto config =
        std::make_shared<config::ConfigBase>(std::move(connectorConfig));

    velox::connector::hive::HiveConnectorFactory factory;
    auto connector =
        factory.newConnector(kHiveConnectorId, config, ioExecutor_.get());
    velox::connector::registerConnector(connector);

    connector::ConnectorMetadata::registerMetadata(
        kHiveConnectorId,
        std::make_shared<connector::hive::LocalHiveConnectorMetadata>(
            dynamic_cast<velox::connector::hive::HiveConnector*>(
                connector.get())));

    return connector;
  }

  struct Timing {
    uint64_t micros{0};
    uint64_t userNanos{0};
    uint64_t systemNanos{0};

    std::string toString() const {
      std::stringstream out;
      out << succinctNanos(micros * 1000) << " / " << succinctNanos(userNanos)
          << " user / " << succinctNanos(systemNanos) << " system ("
          << (100 * (userNanos + systemNanos) / (micros * 1000)) << "%)";
      return out.str();
    }
  };

  std::vector<RowVectorPtr> runInner(
      runner::LocalRunner& runner,
      Timing& timing) {
    std::vector<RowVectorPtr> results;

    {
      struct rusage start;
      getrusage(RUSAGE_SELF, &start);
      MicrosecondTimer timer(&timing.micros);

      while (auto rows = runner.next()) {
        results.push_back(rows);
      }

      struct rusage final;
      getrusage(RUSAGE_SELF, &final);
      auto tvNanos = [](struct timeval tv) {
        return tv.tv_sec * 1000000000 + tv.tv_usec * 1000;
      };
      timing.userNanos = tvNanos(final.ru_utime) - tvNanos(start.ru_utime);
      timing.systemNanos = tvNanos(final.ru_stime) - tvNanos(start.ru_stime);
    }

    return results;
  }

  connector::TablePtr createTable(
      const ::axiom::sql::presto::CreateTableAsSelectStatement& statement) {
    auto metadata =
        connector::ConnectorMetadata::metadata(connector_->connectorId());

    folly::F14FastMap<std::string, velox::Variant> options;
    for (const auto& [key, value] : statement.properties()) {
      options[key] =
          optimizer::ConstantExprEvaluator::evaluateConstantExpr(*value);
    }

    auto session = std::make_shared<connector::ConnectorSession>("test");
    return metadata->createTable(
        session, statement.tableName(), statement.tableSchema(), options);
  }

  void dropTable(const ::axiom::sql::presto::DropTableStatement& statement) {
    auto metadata =
        connector::ConnectorMetadata::metadata(connector_->connectorId());

    const auto& tableName = statement.tableName();

    auto session = std::make_shared<connector::ConnectorSession>("test");
    const bool dropped =
        metadata->dropTable(session, tableName, statement.ifExists());

    if (dropped) {
      std::cout << "Dropped table: " << tableName << std::endl;
    } else {
      std::cout << "Table doesn't exist: " << tableName << std::endl;
    }
    return;
  }

  void run(std::string_view sql) {
    ::axiom::sql::presto::SqlStatementPtr sqlStatement;
    try {
      sqlStatement = prestoParser_->parse(sql);
    } catch (std::exception& e) {
      std::cerr << "Failed to parse SQL: " << e.what() << std::endl;
      return;
    }

    if (sqlStatement->isExplain()) {
      auto* explain =
          sqlStatement->as<::axiom::sql::presto::ExplainStatement>();

      CHECK(explain->statement()->isSelect());
      auto* select =
          explain->statement()->as<::axiom::sql::presto::SelectStatement>();
      if (explain->isAnalyze()) {
        runExplainAnalyze(*select);
      } else {
        runExplain(*select, explain->type());
      }
      return;
    }

    if (sqlStatement->isCreateTableAsSelect()) {
      const auto* ctas =
          sqlStatement
              ->as<::axiom::sql::presto::CreateTableAsSelectStatement>();

      auto table = createTable(*ctas);

      auto originalSchemaResolver = schema_;
      SCOPE_EXIT {
        schema_ = originalSchemaResolver;
      };

      schema_ = std::make_shared<connector::SchemaResolver>();
      schema_->setTargetTable(connector_->connectorId(), table);

      runSql(ctas->plan());
      return;
    }

    if (sqlStatement->isInsert()) {
      const auto* insert =
          sqlStatement->as<::axiom::sql::presto::InsertStatement>();
      runSql(insert->plan());
      return;
    }

    if (sqlStatement->isDropTable()) {
      const auto* drop =
          sqlStatement->as<::axiom::sql::presto::DropTableStatement>();

      try {
        dropTable(*drop);
      } catch (std::exception& e) {
        std::cerr << "DROP TABLE failed: " << e.what() << std::endl;
        return;
      }
      return;
    }

    CHECK(sqlStatement->isSelect());

    const auto logicalPlan =
        sqlStatement->as<::axiom::sql::presto::SelectStatement>()->plan();

    runSql(logicalPlan);
  }

  std::shared_ptr<core::QueryCtx> newQuery() {
    ++queryCounter_;

    return core::QueryCtx::create(
        executor_.get(),
        core::QueryConfig(config_),
        {},
        cache::AsyncDataCache::getInstance(),
        rootPool_->shared_from_this(),
        spillExecutor_.get(),
        fmt::format("query_{}", queryCounter_));
  }

  void runExplain(
      const ::axiom::sql::presto::SelectStatement& statement,
      ::axiom::sql::presto::ExplainStatement::Type type) {
    switch (type) {
      case ::axiom::sql::presto::ExplainStatement::Type::kLogical:
        std::cout << logical_plan::PlanPrinter::toText(*statement.plan())
                  << std::endl;
        break;

      case ::axiom::sql::presto::ExplainStatement::Type::kGraph:
        optimize(statement.plan(), newQuery(), [](const auto& dt) {
          std::cout << optimizer::DerivedTablePrinter::toText(dt) << std::endl;
          return false; // Stop optimization.
        });
        break;

      case ::axiom::sql::presto::ExplainStatement::Type::kOptimized:
        optimize(statement.plan(), newQuery(), nullptr, [](const auto& plan) {
          std::cout << optimizer::RelationOpPrinter::toText(plan) << std::endl;
          return false; // Stop optimization.
        });
        break;

      case ::axiom::sql::presto::ExplainStatement::Type::kExecutable:
        std::cout << optimize(statement.plan(), newQuery()).toString()
                  << std::endl;
        break;
    }
  }

  void runExplainAnalyze(
      const ::axiom::sql::presto::SelectStatement& statement) {
    auto queryCtx = newQuery();
    auto planAndStats = optimize(statement.plan(), queryCtx);

    auto runner = makeRunner(planAndStats, queryCtx);
    SCOPE_EXIT {
      waitForCompletion(runner);
    };

    Timing timing;
    auto results = runInner(*runner, timing);

    printPlanWithStats(*runner, planAndStats.prediction);

    std::cout << "(" << countResults(results) << " rows in " << results.size()
              << " batches) " << timing.toString() << std::endl
              << std::endl;
  }

  // Optimizes provided logical plan.
  // @param checkDerivedTable Optional lambda to call after to-graph stage of
  // optimization. If returns 'false', the optimization stops and returns an
  // empty result.
  // @param checkBestPlan Optional lambda to call towards the end of
  // optimization after best plan is found. If returns 'false', the optimization
  // stops and returns an empty result.
  optimizer::PlanAndStats optimize(
      const logical_plan::LogicalPlanNodePtr& logicalPlan,
      const std::shared_ptr<core::QueryCtx>& queryCtx,
      const std::function<bool(const optimizer::DerivedTable&)>&
          checkDerivedTable = nullptr,
      const std::function<bool(const optimizer::RelationOp&)>& checkBestPlan =
          nullptr) {
    runner::MultiFragmentPlan::Options opts;
    opts.numWorkers = FLAGS_num_workers;
    opts.numDrivers = FLAGS_num_drivers;
    auto allocator =
        std::make_unique<HashStringAllocator>(optimizerPool_.get());
    auto context = std::make_unique<optimizer::QueryGraphContext>(*allocator);

    optimizer::queryCtx() = context.get();
    SCOPE_EXIT {
      optimizer::queryCtx() = nullptr;
    };

    exec::SimpleExpressionEvaluator evaluator(
        queryCtx.get(), optimizerPool_.get());

    auto session = std::make_shared<Session>(queryCtx->queryId());

    axiom::optimizer::OptimizerOptions optimizerOptions;
    optimizerOptions.traceFlags = FLAGS_optimizer_trace;
    optimizerOptions.enableReducingExistences = FLAGS_enable_reducing_existences;
    
    optimizer::Optimization optimization(
        session,
        *logicalPlan,
        *schema_,
        *history_,
        queryCtx,
        evaluator,
        optimizerOptions,
        opts);

    if (checkDerivedTable && !checkDerivedTable(*optimization.rootDt())) {
      return {};
    }

    auto best = optimization.bestPlan();
    if (checkBestPlan && !checkBestPlan(*best->op)) {
      return {};
    }

    return optimization.toVeloxPlan(best->op);
  }

  static void printPlanWithStats(
      runner::LocalRunner& runner,
      const optimizer::NodePredictionMap& estimates) {

    // Calculate predicted CPU total
    float predictedCpu = 0;
    for (auto& pair : estimates) {
      predictedCpu += pair.second.cpu;
    }

    // Get actual CPU timings from TaskStats
    folly::F14FastMap<std::string, int64_t> nodeCpuNanos;
    int64_t cpuNanos = 0;

    // Get TaskStats from runner and convert to PlanNodeStats
    auto taskStats = runner.stats();
    for (const auto& taskStat : taskStats) {
      auto planStats = velox::exec::toPlanStats(taskStat);

      // For each plan node, sum up CPU from addInput, getOutput, and finish
      for (const auto& [nodeId, stats] : planStats) {
        int64_t nodeCpu = stats.addInputTiming.cpuNanos +
                          stats.getOutputTiming.cpuNanos +
                          stats.finishTiming.cpuNanos;

        // Accumulate (PlanNodeIds may not be unique across tasks)
        nodeCpuNanos[nodeId] += nodeCpu;
        cpuNanos += nodeCpu;
      }
    }

    std::cout << runner.printPlanWithStats([&](const core::PlanNodeId& nodeId,
                                               std::string_view indentation,
                                               std::ostream& out) {
      auto it = estimates.find(nodeId);
      if (it != estimates.end()) {
        out << indentation << "Estimate: " << it->second.cardinality
            << " rows, " << succinctBytes(it->second.peakMemory)
            << " peak memory, predicted cpu=" << std::fixed
            << std::setprecision(2)
            << (it->second.cpu * 100.0f / predictedCpu) << "%";

        // Add actual CPU percentage
        auto cpuIt = nodeCpuNanos.find(nodeId);
        if (cpuIt != nodeCpuNanos.end() && cpuNanos > 0) {
          out << ", actual cpu=" << std::fixed << std::setprecision(2)
              << (static_cast<float>(cpuIt->second) * 100.0f / cpuNanos) << "%";
        }

        out << std::endl;
      }
    });
  }

  std::shared_ptr<runner::LocalRunner> makeRunner(
      optimizer::PlanAndStats& planAndStats,
      const std::shared_ptr<core::QueryCtx>& queryCtx) {
    connector::SplitOptions splitOptions{
        .targetSplitCount =
            static_cast<int32_t>(FLAGS_num_workers * FLAGS_num_drivers * 2),
        .fileBytesPerSplit = static_cast<uint64_t>(FLAGS_split_target_bytes),
    };

    return std::make_shared<runner::LocalRunner>(
        planAndStats.plan,
        std::move(planAndStats.finishWrite),
        queryCtx,
        std::make_shared<runner::ConnectorSplitSourceFactory>(splitOptions),
        optimizerPool_);
  }

  /// Runs a query and returns the result as a single vector in *resultVector,
  /// the plan text in *planString and the error message in *errorString.
  /// *errorString is not set if no error. Any of these may be nullptr.
  std::shared_ptr<runner::LocalRunner> runSql(
      const logical_plan::LogicalPlanNodePtr& logicalPlan) {
    auto queryCtx = newQuery();
    optimizer::PlanAndStats planAndStats;
    try {
      planAndStats = optimize(logicalPlan, queryCtx);
    } catch (const std::exception& e) {
      std::cerr << "Failed to optimize: " << e.what() << std::endl;
      return nullptr;
    }

    try {
      auto runner = makeRunner(planAndStats, queryCtx);
      SCOPE_EXIT {
        waitForCompletion(runner);
      };

      Timing timing;
      auto results = runInner(*runner, timing);

      const auto stats = runner->stats();

      const int numRows = printResults(results);

      history_->recordVeloxExecution(planAndStats, stats);
      std::cout << numRows << " rows, " << timing.toString() << std::endl;

      return runner;
    } catch (const std::exception& e) {
      std::cerr << "Query terminated with: " << e.what() << std::endl;
      return nullptr;
    }
  }

  void waitForCompletion(const std::shared_ptr<runner::LocalRunner>& runner) {
    if (runner) {
      try {
        runner->waitForCompletion(500000);
      } catch (const std::exception&) {
      }
    }
  }

  std::unordered_map<std::string, std::string>& sessionConfig() {
    return config_;
  }

  std::set<std::string>& modifiedFlags() {
    return modifiedFlags_;
  }

  void saveHistory() {
    history_->saveToFile(FLAGS_data_path + "/.history");
  }

  void clearHistory() {
    history_ = std::make_unique<optimizer::VeloxHistory>();
  }

 private:
  static int64_t countResults(const std::vector<RowVectorPtr>& results) {
    int64_t numRows = 0;
    for (const auto& result : results) {
      numRows += result->size();
    }
    return numRows;
  }

  static int32_t printResults(const std::vector<RowVectorPtr>& results) {
    const auto numRows = countResults(results);

    auto printFooter = [&]() {
      std::cout << "(" << numRows << " rows in " << results.size()
                << " batches)" << std::endl
                << std::endl;
    };

    if (numRows == 0) {
      printFooter();
      return 0;
    }

    const auto type = results.front()->rowType();
    std::cout << type->toString() << std::endl;

    const auto numColumns = type->size();

    std::vector<std::vector<std::string>> data;
    std::vector<size_t> widths(numColumns, 0);
    std::vector<bool> alignLeft(numColumns);

    for (auto i = 0; i < numColumns; ++i) {
      widths[i] = type->nameOf(i).size();
      alignLeft[i] = type->childAt(i)->isVarchar();
    }

    auto printSeparator = [&]() {
      std::cout << std::setfill('-');
      for (auto i = 0; i < numColumns; ++i) {
        if (i > 0) {
          std::cout << "-+-";
        }
        std::cout << std::setw(widths[i]) << "";
      }
      std::cout << std::endl;
      std::cout << std::setfill(' ');
    };

    auto printRow = [&](const auto& row) {
      for (auto i = 0; i < numColumns; ++i) {
        if (i > 0) {
          std::cout << " | ";
        }
        std::cout << std::setw(widths[i]);
        if (alignLeft[i]) {
          std::cout << std::left;
        } else {
          std::cout << std::right;
        }
        std::cout << row[i];
      }
      std::cout << std::endl;
    };

    int32_t numPrinted = 0;

    auto doPrint = [&]() {
      printSeparator();
      printRow(type->names());
      printSeparator();

      for (const auto& row : data) {
        printRow(row);
      }

      if (numPrinted < numRows) {
        std::cout << std::endl;
        std::cout << "..." << (numRows - numPrinted) << " more rows."
                  << std::endl;
      }

      printFooter();
    };

    for (const auto& result : results) {
      for (auto row = 0; row < result->size(); ++row) {
        data.emplace_back();

        auto& rowData = data.back();
        rowData.resize(numColumns);
        for (auto column = 0; column < numColumns; ++column) {
          rowData[column] = result->childAt(column)->toString(row);
          widths[column] = std::max(widths[column], rowData[column].size());
        }

        ++numPrinted;
        if (numPrinted >= FLAGS_max_rows) {
          doPrint();
          return numRows;
        }
      }
    }

    doPrint();

    return numRows;
  }

  std::shared_ptr<cache::AsyncDataCache> cache_;
  std::shared_ptr<memory::MemoryPool> rootPool_;
  std::shared_ptr<memory::MemoryPool> optimizerPool_;
  std::unique_ptr<folly::IOThreadPoolExecutor> ioExecutor_;
  std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
  std::shared_ptr<folly::IOThreadPoolExecutor> spillExecutor_;
  std::shared_ptr<velox::connector::Connector> connector_;
  std::shared_ptr<connector::SchemaResolver> schema_;
  std::unordered_map<std::string, std::string> config_;
  std::unique_ptr<optimizer::VeloxHistory> history_;
  std::unique_ptr<::axiom::sql::presto::PrestoParser> prestoParser_;

  int32_t queryCounter_{0};
  std::set<std::string> modifiedFlags_;
};

// Reads multi-line command from 'in' until encounters ';' followed by zero or
// more whitespaces.
// @return Command text with leading and trailing whitespaces as well as
// trailing ';' removed.
std::string readCommand(const std::string& prompt, bool& atEnd) {
  std::stringstream command;
  atEnd = false;

  bool stripLeadingSpaces = true;

  while (char* rawLine = linenoise(prompt.c_str())) {
    SCOPE_EXIT {
      if (rawLine != nullptr) {
        free(rawLine);
      }
    };

    std::string line(rawLine);

    int64_t startPos = 0;
    if (stripLeadingSpaces) {
      for (; startPos < line.size(); ++startPos) {
        if (std::isspace(line[startPos])) {
          continue;
        }
        break;
      }
    }

    if (startPos == line.size()) {
      continue;
    }

    // Allow spaces after ';'.
    for (int64_t i = line.size() - 1; i >= startPos; --i) {
      if (std::isspace(line[i])) {
        continue;
      }

      if (line[i] == ';') {
        command << line.substr(startPos, i - startPos);
        linenoiseHistoryAdd(fmt::format("{};", command.str()).c_str());
        return command.str();
      }

      break;
    }

    stripLeadingSpaces = false;
    command << line.substr(startPos) << std::endl;
  }
  atEnd = true;
  return "";
}

void readCommands(VeloxRunner& runner, const std::string& prompt) {
  linenoiseSetMultiLine(1);
  linenoiseHistorySetMaxLen(1024);

  for (;;) {
    bool atEnd;
    std::string command = readCommand(prompt, atEnd);
    if (atEnd) {
      break;
    }

    if (command.empty()) {
      continue;
    }

    if (command.starts_with("exit") || command.starts_with("quit")) {
      break;
    }

    if (command.starts_with("help")) {
      std::cout << helpText;
      continue;
    }

    char* flag = nullptr;
    char* value = nullptr;
    if (sscanf(command.c_str(), "flag %ms = %ms", &flag, &value) == 2) {
      auto message = gflags::SetCommandLineOption(flag, value);
      if (!message.empty()) {
        std::cout << message;
        runner.modifiedFlags().insert(std::string(flag));
      } else {
        std::cout << "Failed to set flag '" << flag << "' to '" << value << "'"
                  << std::endl;
      }
      free(flag);
      free(value);
      continue;
    }

    if (sscanf(command.c_str(), "clear %ms", &flag) == 1) {
      gflags::CommandLineFlagInfo info;
      if (!gflags::GetCommandLineFlagInfo(flag, &info)) {
        std::cout << "Failed to clear flag '" << flag << "'" << std::endl;
        continue;
      }
      auto message =
          gflags::SetCommandLineOption(flag, info.default_value.c_str());
      if (!message.empty()) {
        std::cout << message;
      }
      continue;
    }

    if (command.starts_with("flags")) {
      const auto& names = runner.modifiedFlags();
      std::cout << "Modified flags (" << names.size() << "):\n";
      for (const auto& name : names) {
        std::string value;
        if (gflags::GetCommandLineOption(name.c_str(), &value)) {
          std::cout << name << " = " << value << std::endl;
        }
      }
      continue;
    }

    if (sscanf(command.c_str(), "session %ms = %ms", &flag, &value) == 2) {
      std::cout << "Session '" << flag << "' set to '" << value << "'"
                << std::endl;
      runner.sessionConfig()[std::string(flag)] = std::string(value);
      free(flag);
      free(value);
      continue;
    }

    if (command.starts_with("savehistory")) {
      runner.saveHistory();
      continue;
    }

    if (command.starts_with("clearhistory")) {
      runner.clearHistory();
      continue;
    }

    runner.run(command);
  }
}

} // namespace facebook::axiom

int main(int argc, char** argv) {
  gflags::SetUsageMessage(
      "Axiom local SQL command line. "
      "Run 'axiom_sql --help' for available options.\n");

  // Disable logging to stderr.
  FLAGS_logtostderr = false;

  folly::Init init(&argc, &argv, false);

  try {
    facebook::axiom::VeloxRunner runner;
    runner.initialize();

    if (!FLAGS_query.empty()) {
      runner.run(FLAGS_query);
    } else {
      std::cout << "Axiom SQL. Type statement and end with ;.\n"
                   "flag name = value; sets a gflag.\n"
                   "help; prints help text."
                << std::endl;
      readCommands(runner, "SQL> ");
    }
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    exit(-1);
  }

  return 0;
}
