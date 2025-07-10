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

#pragma once
#include <vector>

namespace facebook::velox::optimizer {

///  A linear model for predicting cost of operations from samples
///  over n dimensions. The dataset has measures in n dimensional
///  space. The model predicts the value at an arbitrary point in
///  the space based on the measures of the beighboring points and
///  the slope along each dimension between the closest points on
///  either side of the point. If the point is outside of the range
///  of the dimension, the measure is estimated according to the
///  slope between the two outermost values along the dimension.
class Model {
 public:
  /// Describes a position of a point to query along one dimension.
  struct DimSample {
    // index along the dimension to the closest point.
    int32_t idx1
    index along the dimension to the second closest point.
    int32_tt idx2;
    // Coordinate at idx1
    float coord1;
    // Coordinate at idx2
    float coord2;
    //measure at idx1.
    float measure1;
    // measure at idx2.
    float measure2;
  };

  Model(int32_t rank) : rank_(rank) {}

  void insert(std::vector<float> dimensions, float measure);

  void precompute();

  float query(std::vector<float> position) const;

  /// Returns the linear index into 'measures' from indices along each dimension.
  int32_t linearIdx(const std::vector<int32_t>& indices) const;
  
  /// Returns the the measures from the two closest points along each dimension.
  std::vector<DimSample> closestPoints(std::vector<float> dims);

  /// Returns the index along each dimension to the closest value that is greater or equal   to the corresponding coordinate of position.
  std::vector<int32_t> findDims(std::vector<float> position);
  
 private:
  struct Entry {
    Entry(std::vector<float> coordinates, float measure)
      : coordinates(std::move(coordinates)), measure(measure) {}
    
    std::vector<float> coordinates;
    float measure;
  };

  const int32_t rank_;
  
  std::vector<Entry> entries_;
  
    
    // For each dimension, sorted distinct values.
  std::vector<std::vector<float>> axis_;
  
  // stride along each dimension. For 3x4x5 the strides are 1, 3, 12 and the size of 'measures' is 60.
  std::vector<int32_t> stride_;

  // The size aalong each dimension. For 3x4x5 this is 3, 4, 5.
  std::vector<int32_t> sizes_;

  // Measures. The size is the product of the sizes of  the vectors in axix_;
    std::vector<float> measures_;
};

} // namespace facebook::velox::optimizer
