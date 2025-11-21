void h30DefineCheckers2() {
// Configuration: numWorkers=1, numDrivers=4
setChecker(2, 1, 4, [](const PlanAndStats& planAndStats) {
  // Plan:
  // partsupp t4*H  (part t2  Build )*H  (supplier t3*H  (nation t5*H  (region t6  Build ) project 2 columns   Build ) project 7 columns   Build )*H left (partsupp t8*H exists (part t2  Build )*H  (supplier t9*H  (nation t10*H  (region t11  Build ) project 1 columns   Build ) project 1 columns   Build ) PARTIAL agg FINAL agg project 1 columns   Build ) order by 4 columns  project 8 columns 

  // Fragment 0
  {
    auto rightMatcher = core::PlanMatcherBuilder().hiveScan("part", {}, "\"and\"(eq(cast(\"p_size\" as BIGINT),15),\"like\"(\"p_type\",'%BRASS'))").build();

auto rightMatcher1 = core::PlanMatcherBuilder().hiveScan("region", common::test::SubfieldFiltersBuilder()
        .add("r_name", exec::in(std::vector<std::string>{std::string("EUROPE")}))
        .build()).build();

auto rightMatcher2 = core::PlanMatcherBuilder().hiveScan("nation", {}).hashJoin(rightMatcher1, velox::core::JoinType::kInner).build();

auto rightMatcher3 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher2, velox::core::JoinType::kInner).build();

auto rightMatcher4 = core::PlanMatcherBuilder().hiveScan("part", {}, "\"and\"(eq(cast(\"p_size\" as BIGINT),15),\"like\"(\"p_type\",'%BRASS'))").build();

auto rightMatcher5 = core::PlanMatcherBuilder().hiveScan("region", common::test::SubfieldFiltersBuilder()
        .add("r_name", exec::in(std::vector<std::string>{std::string("EUROPE")}))
        .build()).build();

auto rightMatcher6 = core::PlanMatcherBuilder().hiveScan("nation", {}).hashJoin(rightMatcher5, velox::core::JoinType::kInner).build();

auto rightMatcher7 = core::PlanMatcherBuilder().hiveScan("supplier", {}).hashJoin(rightMatcher6, velox::core::JoinType::kInner).build();

auto rightMatcher8 = core::PlanMatcherBuilder().hiveScan("partsupp", {}).hashJoin(rightMatcher4, velox::core::JoinType::kLeftSemiFilter).hashJoin(rightMatcher7, velox::core::JoinType::kInner).partialAggregation({"\"ps_partkey_0\""}, {"min(\"ps_supplycost_3\") AS min"}).localPartition().finalAggregation({"\"ps_partkey_0\""}, {"min(\"min\") AS min"}).project({"\"ps_partkey_0\" AS dt7___gk12"}).build();

auto matcher = core::PlanMatcherBuilder().hiveScan("partsupp", {}).hashJoin(rightMatcher, velox::core::JoinType::kInner).hashJoin(rightMatcher3, velox::core::JoinType::kInner).hashJoin(rightMatcher8, velox::core::JoinType::kLeft).topN(100).localMerge().finalLimit(0, 100).project({"\"s_acctbal\" AS s_acctbal", "\"s_name\" AS s_name", "\"n_name\" AS n_name", "\"p_partkey\" AS p_partkey", "\"p_mfgr\" AS p_mfgr", "\"s_address\" AS s_address", "\"s_phone\" AS s_phone", "\"s_comment\" AS s_comment"}).build();
;
    EXPECT_TRUE(matcher->match(planAndStats.plan->fragments()[0].fragment.planNode));
  }
});

}
