void h30DefineCheckers6() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(6, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t2 project 1 columns  PARTIAL agg FINAL agg project 1 columns 

  // Fragment 0
  {
    auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_quantity", exec::lessThanDouble(24))
        .add("l_discount", exec::betweenDouble(0.05, 0.07))
        .add("l_shipdate", exec::greaterThanOrEqual(8766LL))
        .build(), "lt(cast(\"l_shipdate\" as DATE),1995-01-01)").project({"multiply(\"l_extendedprice\",\"l_discount\") AS dt1___p48"}).partialAggregation({}, {"sum(\"dt1___p48\") AS revenue"}).localPartition().finalAggregation({}, {"sum(\"revenue\") AS revenue"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
