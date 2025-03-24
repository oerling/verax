
struct LevelData {
  int32_t exprCount{0};
  float levelCost{0};
  PlanObjectSet exprs;

};

int32_t definitionLevel(std::vector<PlanObjectSet>& levels, ExprCP expr) {
  for (auto i = 0; i < levels.size(); ++i) {
    if (levels[i].exprs.contains(expr)) {
      return i;
    }
  }
  VELOX_UNREACHABLE();
}


float selfCost(ExprCP expr) {
  switch (expr->type()) {
  case PlanType::kColumn: {
    auto kind = expr->value().type->kind();
    if (kind == TypeKind::ARRAY || kind == TypeKind::NAP) {
      return 200;
    }
    return 10;
  }
  default:
    return 5;
  }
}


void makeExprStats(PlanObjectSet exprs, std::vector<LevelData>& levelData,   std::unordered_set<Expr, int32_t>& refCount ) {
  PlanObjectSet& counted;
  for (;;) {
    PlanObjectSet  inputs;
    levelData.emplace_back();
    int32_t level = levelData.size() - 1;
    exprs.forEach([&](PlanObjectCP o) {
      auto* expr = o->as<Expr>();
      float self = selfCost(expr);
    if (counted.contains(expr)) {
      auto i = definitionLevel(levels, expr);
      levels[i].exprs.erase(expr);
      levels[i].cost -= selfCost;
    }
    levls[level].exprs.add(expr);
    levels[level].cost += selfCost;
    
    counted.add(expr);
    if (expr->type() == PlanType::kCall) {
      for (auto& input : expr->as<Call>()->args()) {
      ++refCount[input];
      inputs.add(input);
    }
    }
    });
      if (inputs.empty()) {
      return;
    }
  exprs = std::move(inputs);
  }
}







