void h30DefineCheckers18() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(18, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4*H  (orders t3  Build )*H exists-flag (lineitem t6 PARTIAL agg FINAL agg filter 1 exprs  project 1 columns   Build ) filter 1 exprs *H  (customer t2  Build ) PARTIAL agg FINAL agg order by 2 columns  project 6 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("orders", {}).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("lineitem", {}).partialAggregation({"\"l_orderkey_0\""}, {"sum(\"l_quantity_4\") AS sum"}).localPartition().finalAggregation({"\"l_orderkey_0\""}, {"sum(\"sum\") AS sum"}).filter("gt(\"sum\",300)").project({"\"l_orderkey_0\" AS l_orderkey_17"}).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("customer", {}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher1, velox::core::JoinType::kLeftSemiProject).filter("\"dt1___mark0\"").hashJoin(rightMatcher2, velox::core::JoinType::kInner).partialAggregation({"\"c_name\"", "\"c_custkey\"", "\"o_orderkey\"", "\"o_orderdate\"", "\"o_totalprice\""}, {"sum(\"l_quantity\") AS sum_18"}).localPartition().finalAggregation({"\"c_name\"", "\"c_custkey\"", "\"o_orderkey\"", "\"o_orderdate\"", "\"o_totalprice\""}, {"sum(\"sum_18\") AS sum_18"}).topN(100).localMerge().finalLimit(0, 100).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
