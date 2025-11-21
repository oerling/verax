void h30DefineCheckers5() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(5, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4*H  (orders t3*H  (customer t2*H  (nation t6*H  (region t7  Build ) project 2 columns   Build ) project 4 columns   Build ) project 4 columns   Build )*H  (supplier t5  Build ) project 2 columns  PARTIAL agg FINAL agg order by 1 columns  project 2 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("region", common::test::SubfieldFiltersBuilder()
        .add("r_name", exec::in(std::vector<std::string>{std::string("ASIA")}))
        .build()).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("nation", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("customer", {}).hashJoin(rightMatcher1, velox::core::JoinType::kInner).build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderdate", exec::greaterThanOrEqual(8766LL))
        .build(), "lt(cast(\"o_orderdate\" as DATE),1995-01-01)").hashJoin(rightMatcher2, velox::core::JoinType::kInner).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("supplier", {}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher3, velox::core::JoinType::kInner).hashJoin(rightMatcher4, velox::core::JoinType::kInner).project({"\"n_name\" AS n_name", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p92"}).partialAggregation({"\"n_name\""}, {"sum(\"dt1___p92\") AS revenue"}).localPartition().finalAggregation({"\"n_name\""}, {"sum(\"revenue\") AS revenue"}).orderBy({"\"revenue\" DESC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
