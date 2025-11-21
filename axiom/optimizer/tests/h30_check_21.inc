void h30DefineCheckers21() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(21, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t7 project 2 columns *H right exists-flag (lineitem t9 project 2 columns *H right exists-flag (lineitem t3*H  (supplier t2*H  (nation t5  Build ) project 2 columns   Build )*H  (orders t4  Build )  Build ) filter 1 exprs   Build ) filter 1 exprs  PARTIAL agg FINAL agg order by 2 columns  project 2 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("nation", common::test::SubfieldFiltersBuilder()
        .add("n_name", exec::in(std::vector<std::string>{std::string("SAUDI ARABIA")}))
        .build()).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderstatus", exec::in(std::vector<std::string>{std::string("F")}))
        .build()).build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("lineitem", {}, "lt(\"l_commitdate\",\"l_receiptdate\")").hashJoin(rightMatcher1, velox::core::JoinType::kInner).hashJoin(rightMatcher2, velox::core::JoinType::kInner).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("lineitem", {}, "lt(\"l_commitdate\",\"l_receiptdate\")").hashJoin(rightMatcher3, velox::core::JoinType::kRightSemiProject).filter("\"not\"(\"dt1___mark1\")").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher4, velox::core::JoinType::kRightSemiProject).filter("\"dt1___mark0\"").partialAggregation({"\"s_name\""}, {"count() AS numwait"}).localPartition().finalAggregation({"\"s_name\""}, {"count(\"numwait\") AS numwait"}).topN(100).localMerge().finalLimit(0, 100).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
