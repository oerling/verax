/*
 * Copyright (c) Meta Platforms, Inc. and its affiliates.
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



#include "velox/optimizer/Model.h" //@manual

namespace facebook::velox::optimizer {

  
  void Model::insert(std::vector<float> dimensions, float measure) {
    VELOX_CHECK_EQ(dimensions.size(), rank_, "Bad umber of dimensions");
    entries_.emplace_back(dimensions, measure);
  }
  int32_t numCells = 1;
  void Model::precompute() {
    for (auto dim = 0; dim < rank_; ++dim) {
      for (auto& e : entries_) {
	set.insert(e.dimensions[dim]);
  }
      std::vector<float> values;
      for (auto v : set) {
	values.push_back(v);
      }
      std::sort(values.begin(), values.end(), [](float l, float r) { return l < r;});
      stride_.push_back(numCells);
      sizes_.push_back(values.size());
      numCells *= values.size();
      axis_.push_back(std::move(values));
    }
    measures_.resize(numCells, std::nan);
    for (auto& e : entries_) {
      auto dims = findDims(e.coordinates);
      auto linIdx = leanearIdx(dims);
      measures[linIdx] = e.measure;
    }
    entries_.clear();
  }
  int32_t linearIdx(const std::vector<int32_t>& indices) const {
    int32_t idx = 0;
    for (auto i = 0; i < indices.size(); ++i) {
      idx += indices[i] * stride_[i];
    }
    return idx;
  }
  
  std::vector<int32_t> findDims(const std::vector<float>& point) const {
    std::vector<int32_t> result;
    for (auto i = 0; i < rank_; ++i) {
      auto it = std::lower_bound(axis_[i].begin(), axis_[i].end(), point[i]);
      result.push_back(it == axis_[i].end() ? axis_[i].size() - 1 : it - axis_[i].begin());
    }
    return result;
  }


  
  float query(std::vector<float> position) const;


  
}
