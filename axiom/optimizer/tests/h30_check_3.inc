void h30DefineCheckers3() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(3, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4*H  (orders t3*H  (customer t2  Build ) project 3 columns   Build ) project 4 columns  PARTIAL agg FINAL agg order by 2 columns  project 4 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("customer", common::test::SubfieldFiltersBuilder()
        .add("c_mktsegment", exec::in(std::vector<std::string>{std::string("BUILDING")}))
        .build()).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderdate", exec::lessThanOrEqual(9203LL))
        .build()).hashJoin(rightMatcher, velox::core::JoinType::kInner).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::greaterThanOrEqual(9205LL))
        .build()).hashJoin(rightMatcher1, velox::core::JoinType::kInner).project({"\"l_orderkey\" AS l_orderkey", "\"o_orderdate\" AS o_orderdate", "\"o_shippriority\" AS o_shippriority", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p61"}).partialAggregation({"\"l_orderkey\"", "\"o_orderdate\"", "\"o_shippriority\""}, {"sum(\"dt1___p61\") AS revenue"}).localPartition().finalAggregation({"\"l_orderkey\"", "\"o_orderdate\"", "\"o_shippriority\""}, {"sum(\"revenue\") AS revenue"}).topN(10).localMerge().finalLimit(0, 10).project({"\"l_orderkey\" AS l_orderkey", "\"revenue\" AS revenue", "\"o_orderdate\" AS o_orderdate", "\"o_shippriority\" AS o_shippriority"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
