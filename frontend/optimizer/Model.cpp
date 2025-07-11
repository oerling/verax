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

#include "optimizer/Model.h" //@manual
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace facebook::velox::optimizer {

void Model::insert(std::vector<float> dimensions, float measure) {
  // VELOX_CHECK_EQ(dimensions.size(), rank_, "Bad umber of dimensions");
  entries_.emplace_back(std::move(dimensions), measure);
}
int32_t numCells = 1;
void Model::precompute() {
  for (auto dim = 0; dim < rank_; ++dim) {
    std::unordered_set<float> set;
    for (auto& e : entries_) {
      set.insert(e.coordinates[dim]);
    }
    std::vector<float> values;
    // VELOX_CHECK_GT(set.size(), 1, "A dimension must have more than one
    // values: dim={}", i);
    for (auto v : set) {
      values.push_back(v);
    }
    std::sort(
        values.begin(), values.end(), [](float l, float r) { return l < r; });
    stride_.push_back(numCells);
    sizes_.push_back(values.size());
    numCells *= values.size();
    axis_.push_back(std::move(values));
  }
  measures_.resize(numCells, std::nan(""));
  for (auto& e : entries_) {
    auto dims = findDims(e.coordinates);
    auto linIdx = linearIdx(dims);
    measures_[linIdx] = e.measure;
  }
  entries_.clear();
}

int32_t Model::linearIdx(const std::vector<int32_t>& indices) const {
  int32_t idx = 0;
  for (auto i = 0; i < indices.size(); ++i) {
    idx += indices[i] * stride_[i];
  }
  return idx;
}

std::vector<int32_t> Model::findDims(const std::vector<float>& point) const {
  std::vector<int32_t> result;
  for (auto i = 0; i < rank_; ++i) {
    auto it = std::lower_bound(axis_[i].begin(), axis_[i].end(), point[i]);
    result.push_back(
        it == axis_[i].end() ? axis_[i].size() - 1 : it - axis_[i].begin());
  }
  return result;
}

std::vector<Model::DimSample> Model::slopes(
    const std::vector<int32_t>& point,
    const std::vector<float>& coords) const {
  int32_t pointIdx = linearIdx(point);
  float measureAtPoint = measures_[pointIdx];
  std::vector<DimSample> result;
  for (auto i = 0; i < rank_; ++i) {
    DimSample sample;
    float coord = coords[i];
    int32_t idx = point[i];
    if (idx == sizes_[i] - 1) {
      --idx;
      pointIdx -= stride_[i];
    }
    sample.idx1 = idx;
    sample.idx2 = idx + 1;
    float mhigh = measures_[pointIdx + stride_[i]];
    float mlow = measures_[pointIdx];
    float k = (mhigh - mlow) / (axis_[i][idx + 1] - axis_[i][idx]);
    sample.multiplier = (coord - mlow) * k;
    result.push_back(sample);
  }
  return result;
}

float Model::query(const std::vector<float>& coords) const {
  auto point = findDims(coords);
  auto samples = slopes(point, coords);
  float sum = 0;
  for (auto i = 0; i < rank_; ++i) {
    auto& slope = samples[i];
    float k = (slope.measure2 - slope.measure1) / (slope.coord2 - slope.coord1);
    if (i == 0) {
      sum = slope.measure1;
    } else {
      sum *= slope.multiplier;
    }
  }
  return sum;
}

} // namespace facebook::velox::optimizer
