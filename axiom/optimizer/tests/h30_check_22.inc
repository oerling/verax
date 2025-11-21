void h30DefineCheckers22() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(22, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // orders t6 project 1 columns *H right exists-flag (customer t2*M  (customer t4 PARTIAL agg FINAL agg project 1 columns ) filter 1 exprs   Build ) filter 1 exprs  project 2 columns  PARTIAL agg FINAL agg order by 1 columns  project 3 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("customer", common::test::SubfieldFiltersBuilder()
        .add("c_acctbal", exec::greaterThanDouble(0))
        .build(), "\"in\"(substr(\"c_phone\",1,2),array['13', '31', '23', '29', '30', '18', '17'])").partialAggregation({}, {"avg(\"c_acctbal_5\") AS avg"}).localPartition().finalAggregation({}, {"avg(\"avg\") AS avg"}).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("customer", {}, "\"in\"(substr(\"c_phone\",1,2),array['13', '31', '23', '29', '30', '18', '17'])").nestedLoopJoin(rightMatcher, velox::core::JoinType::kInner).filter("gt(\"c_acctbal\",\"avg\")").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("orders", {}).hashJoin(rightMatcher1, velox::core::JoinType::kRightSemiProject).filter("\"not\"(\"dt1___mark0\")").project({"substr(\"c_phone\",1,2) AS cntrycode", "\"c_acctbal\" AS c_acctbal"}).partialAggregation({"\"cntrycode\""}, {"count() AS numcust", "sum(\"c_acctbal\") AS totacctbal"}).localPartition().finalAggregation({"\"cntrycode\""}, {"count(\"numcust\") AS numcust", "sum(\"totacctbal\") AS totacctbal"}).orderBy({"\"cntrycode\" ASC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
