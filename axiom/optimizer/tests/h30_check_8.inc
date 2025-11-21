void h30DefineCheckers8() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(8, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t4*H  (part t2  Build )*H  (orders t5*H  (customer t6*H  (nation t7*H  (region t9  Build ) project 1 columns   Build ) project 1 columns   Build ) project 2 columns   Build )*H  (supplier t3  Build )*H  (nation t8  Build ) project 3 columns  PARTIAL agg FINAL agg order by 1 columns  project 2 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", common::test::SubfieldFiltersBuilder()
        .add("p_type", exec::in(std::vector<std::string>{std::string("ECONOMY ANODIZED STEEL")}))
        .build()).build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("region", common::test::SubfieldFiltersBuilder()
        .add("r_name", exec::in(std::vector<std::string>{std::string("AMERICA")}))
        .build()).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("nation", {}).hashJoin(rightMatcher1, velox::core::JoinType::kInner).build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("customer", {}).hashJoin(rightMatcher2, velox::core::JoinType::kInner).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("orders", common::test::SubfieldFiltersBuilder()
        .add("o_orderdate", exec::between(9131LL, 9861LL))
        .build()).hashJoin(rightMatcher3, velox::core::JoinType::kInner).build();

auto rightMatcher5 = core::PlanMatcherBuilder().hiveScan("supplier", {}).build();

auto rightMatcher6 = core::PlanMatcherBuilder().hiveScan("nation", {}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher4, velox::core::JoinType::kInner).hashJoin(rightMatcher5, velox::core::JoinType::kInner).hashJoin(rightMatcher6, velox::core::JoinType::kInner).project({"year(\"o_orderdate\") AS o_year", "switch(eq(\"n_name_1\",'BRAZIL'),multiply(\"l_extendedprice\",minus(1,\"l_discount\")),0) AS dt1___p113", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p108"}).partialAggregation({"\"o_year\""}, {"sum(\"dt1___p113\") AS sum", "sum(\"dt1___p108\") AS sum_4"}).localPartition().finalAggregation({"\"o_year\""}, {"sum(\"sum\") AS sum", "sum(\"sum_4\") AS sum_4"}).orderBy({"\"o_year\" ASC NULLS LAST"}).localMerge().project({"\"o_year\" AS o_year", "divide(\"sum\",\"sum_4\") AS mkt_share"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
