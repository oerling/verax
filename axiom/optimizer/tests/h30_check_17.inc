void h30DefineCheckers17() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(17, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t2*H  (part t3  Build )*H left (lineitem t5*H exists (part t3  Build ) PARTIAL agg FINAL agg project 2 columns   Build ) filter 1 exprs  PARTIAL agg FINAL agg project 1 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", common::test::SubfieldFiltersBuilder()
        .add("p_container", exec::in(std::vector<std::string>{std::string("MED BOX")}))
        .add("p_brand", exec::in(std::vector<std::string>{std::string("Brand#23")}))
        .build()).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("part", common::test::SubfieldFiltersBuilder()
        .add("p_container", exec::in(std::vector<std::string>{std::string("MED BOX")}))
        .add("p_brand", exec::in(std::vector<std::string>{std::string("Brand#23")}))
        .build()).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher1, velox::core::JoinType::kLeftSemiFilter).partialAggregation({"\"l_partkey_1\""}, {"avg(\"l_quantity_4\") AS avg"}).localPartition().finalAggregation({"\"l_partkey_1\""}, {"avg(\"avg\") AS avg"}).project({"\"l_partkey_1\" AS dt4___gk6", "multiply(\"avg\",0.2) AS expr"}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher2, velox::core::JoinType::kLeft).filter("lt(\"l_quantity\",\"expr\")").partialAggregation({}, {"sum(\"l_extendedprice\") AS sum"}).localPartition().finalAggregation({}, {"sum(\"sum\") AS sum"}).project({"divide(\"sum\",7) AS avg_yearly"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
