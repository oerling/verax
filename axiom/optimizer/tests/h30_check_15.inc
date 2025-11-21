void h30DefineCheckers15() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(15, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // supplier t2*H  (lineitem t4 project 2 columns  PARTIAL agg FINAL agg project 2 columns   Build )*H  (lineitem t6 project 2 columns  PARTIAL agg FINAL agg project 1 columns  PARTIAL agg FINAL agg project 1 columns   Build ) order by 1 columns  project 5 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::greaterThanOrEqual(9496LL))
        .build(), "lt(cast(\"l_shipdate\" as DATE),1996-04-01)").project({"\"l_suppkey\" AS l_suppkey", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt3___p49"}).partialAggregation({"\"l_suppkey\""}, {"sum(\"dt3___p49\") AS total_revenue"}).localPartition().finalAggregation({"\"l_suppkey\""}, {"sum(\"total_revenue\") AS total_revenue"}).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::greaterThanOrEqual(9496LL))
        .build(), "lt(cast(\"l_shipdate\" as DATE),1996-04-01)").project({"\"l_suppkey_2\" AS l_suppkey_2", "multiply(\"l_extendedprice_5\",minus(1,\"l_discount_6\")) AS dt5___p71"}).partialAggregation({"\"l_suppkey_2\""}, {"sum(\"dt5___p71\") AS total_revenue_17"}).localPartition().finalAggregation({"\"l_suppkey_2\""}, {"sum(\"total_revenue_17\") AS total_revenue_17"}).project({"\"total_revenue_17\" AS total_revenue_19"}).partialAggregation({}, {"max(\"total_revenue_19\") AS max"}).localPartition().finalAggregation({}, {"max(\"max\") AS max"}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher1, velox::core::JoinType::kInner).orderBy({"\"s_suppkey\" ASC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
