



namespace facebook::velox::optimizer::test {

  struct FeatureOptions {
  int32_t numFloat{10};
  int32_t numInt{10};
  int32_t numIdList{10};
  int32_t idListMaxCard{1000};
  int32_t idListMaxDistinct{1000};
  numIdListScore{5};
  
};


std::vector<RowVectorPtr> makeFeatures(int32_t numBatches, int32_t batchSize, const FeatureOptions& opts, memory::MemoryPool* pool);
    
}
