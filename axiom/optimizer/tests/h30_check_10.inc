void h30DefineCheckers10() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(10, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4*H  (orders t3  Build )*H  (customer t2  Build )*H  (nation t5  Build ) project 8 columns  PARTIAL agg FINAL agg order by 1 columns  project 8 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderdate", exec::greaterThanOrEqual(8674LL))
        .build(), "lt(cast(\"o_orderdate\" as DATE),1994-01-01)").build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("customer", {}).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("nation", {}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_returnflag", exec::in(std::vector<std::string>{std::string("R")}))
        .build()).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher1, velox::core::JoinType::kInner).hashJoin(rightMatcher2, velox::core::JoinType::kInner).project({"\"c_custkey\" AS c_custkey", "\"c_name\" AS c_name", "\"c_acctbal\" AS c_acctbal", "\"c_phone\" AS c_phone", "\"n_name\" AS n_name", "\"c_address\" AS c_address", "\"c_comment\" AS c_comment", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p77"}).partialAggregation({"\"c_custkey\"", "\"c_name\"", "\"c_acctbal\"", "\"c_phone\"", "\"n_name\"", "\"c_address\"", "\"c_comment\""}, {"sum(\"dt1___p77\") AS revenue"}).localPartition().finalAggregation({"\"c_custkey\"", "\"c_name\"", "\"c_acctbal\"", "\"c_phone\"", "\"n_name\"", "\"c_address\"", "\"c_comment\""}, {"sum(\"revenue\") AS revenue"}).topN(20).localMerge().finalLimit(0, 20).project({"\"c_custkey\" AS c_custkey", "\"c_name\" AS c_name", "\"revenue\" AS revenue", "\"c_acctbal\" AS c_acctbal", "\"n_name\" AS n_name", "\"c_address\" AS c_address", "\"c_phone\" AS c_phone", "\"c_comment\" AS c_comment"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
