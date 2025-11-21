void h30DefineCheckers4() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(4, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4 project 1 columns *H right exists-flag (orders t2  Build ) filter 1 exprs  PARTIAL agg FINAL agg order by 1 columns  project 2 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderdate", exec::greaterThanOrEqual(8582LL))
        .build(), "lt(cast(\"o_orderdate\" as DATE),1993-10-01)").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}, "lt(\"l_commitdate\",\"l_receiptdate\")").hashJoin(rightMatcher, velox::core::JoinType::kRightSemiProject).filter("\"dt1___mark0\"").partialAggregation({"\"o_orderpriority\""}, {"count() AS order_count"}).localPartition().finalAggregation({"\"o_orderpriority\""}, {"count(\"order_count\") AS order_count"}).orderBy({"\"o_orderpriority\" ASC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
