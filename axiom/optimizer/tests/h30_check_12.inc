void h30DefineCheckers12() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(12, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // orders t2*H  (lineitem t3  Build ) project 3 columns  PARTIAL agg FINAL agg order by 1 columns  project 3 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_receiptdate", exec::greaterThanOrEqual(8766LL))
        .add("l_shipmode", exec::in(std::vector<std::string>{std::string("MAIL"), std::string("SHIP")}))
        .build(), "\"and\"(lt(cast(\"l_receiptdate\" as DATE),1995-01-01),lt(\"l_commitdate\",\"l_receiptdate\"),lt(\"l_shipdate\",\"l_commitdate\"))").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("orders", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).project({"\"l_shipmode\" AS l_shipmode", "switch(\"or\"(eq(\"o_orderpriority\",'1-URGENT'),eq(\"o_orderpriority\",'2-HIGH')),1,0) AS dt1___p58", "switch(\"and\"(neq(\"o_orderpriority\",'1-URGENT'),neq(\"o_orderpriority\",'2-HIGH')),1,0) AS dt1___p65"}).partialAggregation({"\"l_shipmode\""}, {"sum(\"dt1___p58\") AS high_line_count", "sum(\"dt1___p65\") AS low_line_count"}).localPartition().finalAggregation({"\"l_shipmode\""}, {"sum(\"high_line_count\") AS high_line_count", "sum(\"low_line_count\") AS low_line_count"}).orderBy({"\"l_shipmode\" ASC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
