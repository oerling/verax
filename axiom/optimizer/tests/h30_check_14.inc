void h30DefineCheckers14() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(14, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // part t3*H  (lineitem t2  Build ) project 2 columns  PARTIAL agg FINAL agg project 1 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipdate", exec::greaterThanOrEqual(9374LL))
        .build(), "lt(cast(\"l_shipdate\" as DATE),1995-10-01)").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("part", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).project({"switch(\"like\"(\"p_type\",'PROMO%'),multiply(\"l_extendedprice\",minus(1,\"l_discount\")),0) AS dt1___p53", "multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p51"}).partialAggregation({}, {"sum(\"dt1___p53\") AS sum", "sum(\"dt1___p51\") AS sum_0"}).localPartition().finalAggregation({}, {"sum(\"sum\") AS sum", "sum(\"sum_0\") AS sum_0"}).project({"divide(multiply(\"sum\",100),\"sum_0\") AS promo_revenue"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
