void h30DefineCheckers16() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(16, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // partsupp t2*H  (part t3  Build )*H exists-flag (supplier t5 project 1 columns   Build ) filter 1 exprs  PARTIAL agg FINAL agg order by 4 columns  project 4 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", common::test::SubfieldFiltersBuilder()
        .add("p_brand", exec::orFilter(exec::lessThan(std::string("Brand#45")), exec::greaterThan(std::string("Brand#45"))))
        .build(), "\"and\"(\"not\"(\"like\"(\"p_type\",'MEDIUM POLISHED%')),\"in\"(cast(\"p_size\" as BIGINT),array[49, 14, 23, 45, 19, 3, 36, 9]))").build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("supplier", {}, "\"like\"(\"s_comment\",'%Customer%Complaints%')").build();

auto matcher = core::PlanMatcherBuilder().hiveScan("partsupp", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher1, velox::core::JoinType::kLeftSemiProject).filter("\"not\"(\"dt1___mark0\")").partialAggregation({"\"p_brand\"", "\"p_type\"", "\"p_size\""}, {"count(\"ps_suppkey\") AS supplier_cnt"}).localPartition().finalAggregation({"\"p_brand\"", "\"p_type\"", "\"p_size\""}, {"count(\"supplier_cnt\") AS supplier_cnt"}).orderBy({"\"supplier_cnt\" DESC NULLS LAST", "\"p_brand\" ASC NULLS LAST", "\"p_type\" ASC NULLS LAST", "\"p_size\" ASC NULLS LAST"}).localMerge().build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
