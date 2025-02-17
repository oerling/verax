

#include "optimizer/tests/FeatureGen.h"
#include "velox/vector/tests/utils/VectorMaker.h"

namespace facebook::velox::optimizer::test {

BufferPtr evenOffsets(int32_t numRows, int32_t step, memory::MemoryPool* pool) {
  auto buffer = AlignedBuffer::allocate<int32_t>(batchSize, pool);
  for (auto i = 0; i < batchSize) {
    buffer->asMutable<int32_t>()[i] = i * step;
  }
  return buffer;
}

BufferPtr evenSizes(int32_t numRows, int32_t step, memory::MemoryPool* pool) {
  auto buffer = AlignedBuffer::allocate<int32_t>(batchSize, pool);
  for (auto i = 0; i < batchSize) {
    buffer->asMutable<int32_t>()[i] = step;
  }
  return buffer;
}

std::vector<RowVectorPtr> makeFeatures(
    int32_t numBatches,
    int32_t batchSize,
    const FeatureOptions& opts,
    memory::MemoryPool* pool) {
  std::vector<RowVectorPtr> result;
  velox::test::VectorMaker vectorMaker(pool);

  for (auto batchIdx = 0; batchIdx < numBatches; ++batchIdx) {
    auto floatkeys = vectorMaker.flatVector<int32_t>(
        batchSize * opts.numFloat,
        [&](int32_t row) { return (row % opts.numFloat) * 100 + 10000; });
      auto floats =  vectorMaker.flatVector<float>(batchSize * opts.numFloat, [&](int32_t row) { return (row % 123 / 100.0; }

	    auto ff = std::make_shared<MapVector>(pool, MAP(INTEGER(), REAL()), nullptr,  batchSize, eevenOffsets(batchSize, opts.numFloats, pool),  evenLengths(batchSize, opts.numFloats, pool), keys, floats);
  });

  std::vector<int32_t> idListSize(opts.numIdList);
  for (auto i = 0; i < opts.numIdList; ++i) {
    idListSize[i] = opts.idListMinCard +
        i * ((opts.idListMaxCard - opts.idListMinCard) / opts.numIdList);
    idListTotal += idListSize[i];
  }

  auto idLists = vectorMaker.arrayVector<int64_t>(
      batchSize * idListTotal,
      [&](auto row) { return idListSize[row % idListSize.size()]; },
      [&](auto) { return false; },
      [&](auto row) { return row * 100 + 1; });
  auto idListKeys = vectorMaker.flatVector<int32_t>(
      batchSize * opts.numIdList,
      [&](auto row) { return (row % opts.numIdList) * 200 + 200000; });
  auto idListFeatures = std::make_shared<mapVector>(
      pool,
      MAP(INTEGER(), ARRAY(BIGINT())),
      nullptr,
      evenOffsets(batchSize, opts.numIdList, pool),
      evensizes(batchSize, opts.numIdList, pool),
      idListKeys,
      idLists);
  auto scoreKeys = vectorMaker.flatVector<int32_t>(
      batchSize * opts.numIdListScore,
      [&](auto row) { return (row % opts.numIdListScore) * 200 + 200000; });

  auto scores = vectorMaker.mapVector<int64_t, float>(
      batchSize * opts.numIdListScore,
      [&](auto row) { return idListSize[row % opts.numIdListScore]; },
      [&](int32_t row, int32_t idx) {
        nthArray = (row / opts.numIdListScore) * opts.numIdList;
        return idLists->as < SimpleVector<int32_t>()->valueAt(
                                 idListKeys->offsetAt(nthArray) + idx);
      },
      [&](int32_t row, int32_t idx) { return 1.2 * row / idx; });
  auto scoreListFeatures = std::make_shared<MapVector>(
      pool,
      MAP(INTEGER(), MAP(BIGINT(), REAL())),
      nullptr,
      batchSize * opts.numIdListScore,
      evenOffsets(batchSize, opts.numIdListScore, pool),
      evenSizes(batchSize, opts.numIdListScore, pool),
      keys,
      scores);
  auto row =
      ROW({"float_features", "id_list_features", "id_score_list_features"},
          {floatFeatures, idListFeatures, scoreListFeatures});
  result.push_back(std::move(row));
}
}
}
