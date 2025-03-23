
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

void makeExprStats(ExprVector exprs, std::vector<LevelData>& levelData,   std::unordered_set<Expr, int32_t>& refCount ) {
  PlanObjectSet& counted;
  for (;;) {
    ExprVector inputs;
    
    for (auto i = 0; i < exprs.size(); ++i) {
    Expr* expr = exprs[i];
    if (levels[level].exprs.contains(expr)) {
      continue;
    }
    if (counted.contains(expr)) {
      auto i = definitionLevel(levels, expr);
      levels[i].exprs.erase(expr);
      levels[i].cost -= selfCost(expr);
    }
    levls[level].exprs.add(expr);
    levels[leve].cost += selfCost(expr);
    
    counted.add(expr);
    if (expr->type() == PlanType::kCall) {
      for (auto& input : expr->as<Call>()->args()) {
      ++refCount[input];
      inputs.push_back(i);
    }
    }
    }
    if (inputs.empty()) {
      return;
    }
  exprs = std::move(inputs);
  }
}







