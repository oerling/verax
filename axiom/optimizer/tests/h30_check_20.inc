void h30DefineCheckers20() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(20, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t7 PARTIAL agg FINAL agg project 3 columns *H right (partsupp t5*H exists-flag (part t11 project 1 columns   Build ) filter 1 exprs *H exists (supplier t2*H  (nation t3  Build ) project 1 columns   Build )  Build ) filter 1 exprs  project 1 columns *H right exists-flag (supplier t2*H  (nation t3  Build )  Build ) filter 1 exprs  order by 1 columns  project 2 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", {}, "\"like\"(\"p_name\",'forest%')").build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("nation", common::test::SubfieldFiltersBuilder()
        .add("n_name", exec::in(std::vector<std::string>{std::string("CANADA")}))
        .build()).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher1, velox::core::JoinType::kInner).project({"\"s_suppkey\" AS edt14_edt14_t2_s_suppkey"}).build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("partsupp", {}).hashJoin(rightMatcher, velox::core::JoinType::kLeftSemiProject).filter("\"dt4___mark0\"").hashJoin(rightMatcher2, velox::core::JoinType::kLeftSemiFilter).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("nation", common::test::SubfieldFiltersBuilder()
        .add("n_name", exec::in(std::vector<std::string>{std::string("CANADA")}))
        .build()).build();

auto rightMatcher5 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher4, velox::core::JoinType::kInner).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::greaterThanOrEqual(8766LL))
        .build(), "lt(cast(\"l_shipdate\" as DATE),1995-01-01)").partialAggregation({"\"l_partkey\"", "\"l_suppkey\""}, {"sum(\"l_quantity\") AS sum"}).localPartition().finalAggregation({"\"l_partkey\"", "\"l_suppkey\""}, {"sum(\"sum\") AS sum"}).project({"\"l_partkey\" AS dt6___gk8", "\"l_suppkey\" AS dt6___gk9", "multiply(\"sum\",0.5) AS expr"}).hashJoin(rightMatcher3, velox::core::JoinType::kRight).filter("lt(\"expr\",cast(\"ps_availqty\" as DOUBLE))").project({"\"ps_suppkey\" AS ps_suppkey"}).hashJoin(rightMatcher5, velox::core::JoinType::kRightSemiProject).filter("\"dt1___mark1\"").orderBy({"\"s_name\" ASC NULLS LAST"}).localMerge().project({"\"s_name\" AS s_name", "\"s_address\" AS s_address"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
