void h30DefineCheckers19() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(19, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // lineitem t2*H  (part t3  Build ) filter 1 exprs  project 1 columns  PARTIAL agg FINAL agg project 1 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", {}, "\"or\"(\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,15),\"and\"(eq(\"p_brand\",'Brand#34'),\"in\"(\"p_container\",array['LG CASE', 'LG BOX', 'LG PACK', 'LG PKG']))),\"or\"(\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,5),\"and\"(eq(\"p_brand\",'Brand#12'),\"in\"(\"p_container\",array['SM CASE', 'SM BOX', 'SM PACK', 'SM PKG']))),\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,10),\"and\"(eq(\"p_brand\",'Brand#23'),\"in\"(\"p_container\",array['MED BAG', 'MED BOX', 'MED PKG', 'MED PACK'])))))").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("lineitem", common::test::SubfieldFiltersBuilder()
        .add("l_shipinstruct", exec::in(std::vector<std::string>{std::string("DELIVER IN PERSON")}))
        .add("l_shipmode", exec::in(std::vector<std::string>{std::string("AIR REG"), std::string("AIR")}))
        .build(), "\"or\"(\"and\"(gte(\"l_quantity\",20),lte(\"l_quantity\",30)),\"or\"(\"and\"(gte(\"l_quantity\",1),lte(\"l_quantity\",11)),\"and\"(gte(\"l_quantity\",10),lte(\"l_quantity\",20))))").hashJoin(rightMatcher, velox::core::JoinType::kInner).filter("\"or\"(\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,15),\"and\"(lte(\"l_quantity\",30),\"and\"(gte(\"l_quantity\",20),\"and\"(eq(\"p_brand\",'Brand#34'),\"in\"(\"p_container\",array['LG CASE', 'LG BOX', 'LG PACK', 'LG PKG']))))),\"or\"(\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,5),\"and\"(lte(\"l_quantity\",11),\"and\"(gte(\"l_quantity\",1),\"and\"(eq(\"p_brand\",'Brand#12'),\"in\"(\"p_container\",array['SM CASE', 'SM BOX', 'SM PACK', 'SM PKG']))))),\"and\"(\"between\"(cast(\"p_size\" as BIGINT),1,10),\"and\"(lte(\"l_quantity\",20),\"and\"(gte(\"l_quantity\",10),\"and\"(eq(\"p_brand\",'Brand#23'),\"in\"(\"p_container\",array['MED BAG', 'MED BOX', 'MED PKG', 'MED PACK'])))))))").project({"multiply(\"l_extendedprice\",minus(1,\"l_discount\")) AS dt1___p121"}).partialAggregation({}, {"sum(\"dt1___p121\") AS revenue"}).localPartition().finalAggregation({}, {"sum(\"revenue\") AS revenue"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
