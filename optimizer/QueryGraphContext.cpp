/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "optimizer/QueryGraphContext.h" //@manual

namespace facebook::velox::optimizer {

QueryGraphContext*& queryCtx() {
  thread_local QueryGraphContext* context;
  return context;
}

PlanObjectP QueryGraphContext::dedup(PlanObjectP object) {
  auto pair = deduppedObjects_.insert(object);
  return *pair.first;
}

const char* QueryGraphContext::toName(std::string_view str) {
  auto it = names_.find(str);
  if (it != names_.end()) {
    return it->data();
  }
  char* data = allocator_.allocate(str.size() + 1)->begin(); // NOLINT
  memcpy(data, str.data(), str.size());
  data[str.size()] = 0;
  names_.insert(std::string_view(data, str.size()));
  return data;
}

Name toName(std::string_view string) {
  return queryCtx()->toName(string);
}

const Type* QueryGraphContext::toType(const TypePtr& type) {
  return dedupType(type).get();
}
TypePtr QueryGraphContext::dedupType(const TypePtr& type) {
  auto it = deduppedTypes_.find(type);
  if (it != deduppedTypes_.end()) {
    return *it;
  }
  auto size = type->size();
  if (size == 0) {
    deduppedTypes_.insert(type);
    toTypePtr_[type.get()] = type;
    return type;
  }
  std::vector<TypePtr> children;
  for (auto i = 0; i < size; ++i) {
    children.push_back(dedupType(type->childAt(i)));
  }
  TypePtr newType;
  switch (type->kind()) {
    case TypeKind::ROW: {
      std::vector<std::string> names;
      for (auto i = 0; i < size; ++i) {
        names.push_back(type->as<TypeKind::ROW>().nameOf(i));
      }
      newType = ROW(std::move(names), std::move(children));
      break;
    }
    case TypeKind::ARRAY:
      newType = ARRAY(children[0]);
      break;
    case TypeKind::MAP:
      newType = MAP(children[0], children[1]);
      break;
    default:
      VELOX_FAIL("Type has size > 0 and is not row/array/map");
  }
  deduppedTypes_.insert(newType);
  toTypePtr_[newType.get()] = newType;
  return newType;
}

const TypePtr& QueryGraphContext::toTypePtr(const Type* type) {
  auto it = toTypePtr_.find(type);
  if (it != toTypePtr_.end()) {
    return it->second;
  }
  VELOX_FAIL("Cannot translate {} back to TypePtr", type->toString());
}

const Type* toType(const TypePtr& type) {
  return queryCtx()->toType(type);
}

const TypePtr& toTypePtr(const Type* type) {
  return queryCtx()->toTypePtr(type);
}

bool Step::operator==(const Step& other) const {
  return kind == other.kind && field == other.field && id == other.id;
}

size_t Step::hash() const {
  return 1 + static_cast<int32_t>(kind) + reinterpret_cast<size_t>(field) + id;
}


size_t Path::hash() const {
  size_t h = 123;
  for (auto& step : steps_) {
    h = (h + 1921) * step.hash();
  }
  return h;
}

bool Path::operator==(const Path& other) const {
  if (steps_.size() != other.steps_.size()) {
    return false;
  }
  for (auto i = 0; i < steps_.size(); ++i) {
    if (!(steps_[i] == other.steps_[i])) {
      return false;
    }
  }
  return true;
}

std::string Path::toString() const {
  std::stringstream out;
  for (auto& step : steps_) {
    switch (step.kind) {
      case StepKind::kCardinality:
        out << ".cardinality";
        break;
      case StepKind::kField:
        out << "." << step.field;
        break;
      case StepKind::kSubscript:
        if (step.field) {
          out << "[" << step.field << "]";
        } else {
          out << "[" << step.id << "]";
        }
        break;
    }
  }
  return out.str();
}

  PathCP QueryGraphContext::toPath(PathCP path) {
    path->makeImmutable();
    auto pair = deduppedPaths_.insert(path);
	if (path != *pair.first) {
	  delete path;
	}
	return *pair.first;
  }

  
} // namespace facebook::velox::optimizer
