void h30DefineCheckers7() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(7, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t3*H  (supplier t2*H  (nation t6  Build ) project 2 columns   Build )*H  (orders t4*H  (customer t5*H  (nation t7  Build ) project 2 columns   Build ) project 2 columns   Build ) filter 1 exprs  project 4 columns  PARTIAL agg FINAL agg order by 3 columns  project 4 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("nation", {}, "\"or\"(eq(\"n_name\",'FRANCE'),eq(\"n_name\",'GERMANY'))").build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("nation", {}, "\"or\"(eq(\"n_name\",'GERMANY'),eq(\"n_name\",'FRANCE'))").build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("customer", {}).hashJoin(rightMatcher2, velox::core::JoinType::kInner).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("orders", {}).hashJoin(rightMatcher3, velox::core::JoinType::kInner).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::between(9131LL, 9861LL))
        .build()).hashJoin(rightMatcher1, velox::core::JoinType::kInner).hashJoin(rightMatcher4, velox::core::JoinType::kInner).filter("\"or\"(\"and\"(eq(\"n_name\",'FRANCE'),eq(\"n_name_1\",'GERMANY')),\"and\"(eq(\"n_name\",'GERMANY'),eq(\"n_name_1\",'FRANCE')))").project({"\"n_name\" AS n_name", "\"n_name_1\" AS n_name_1", "year(\"l_shipdate\") AS l_year", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p92"}).partialAggregation({"\"n_name\"", "\"n_name_1\"", "\"l_year\""}, {"sum(\"dt1___p92\") AS revenue"}).localPartition().finalAggregation({"\"n_name\"", "\"n_name_1\"", "\"l_year\""}, {"sum(\"revenue\") AS revenue"}).orderBy({"\"n_name\" ASC NULLS LAST", "\"n_name_1\" ASC NULLS LAST", "\"l_year\" ASC NULLS LAST"}).localMerge().project({"\"n_name\" AS supp_nation", "\"n_name_1\" AS cust_nation", "\"l_year\" AS l_year", "\"revenue\" AS revenue"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
