//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include "scatter_gather.h"

#include <ripple.h>
#include <ripple/HVX_Scatter_Gather.h>

#include <ripple-test-suite/ripple-test-suite.h>

// Handle restrictions on the block size vs data type.
// HVX block size must be 128 for most combinations of element and index types
// but are twice as large for some others.
template <typename T> struct VectorSizeChooser {
  static const size_t VectorSize = 128;
};

template <> struct VectorSizeChooser<int64_t> {
  static const size_t VectorSize = 256;
};

template <typename T, typename IT>
void Ripple_gather(size_t N, const IT *indexes, T *destination,
                   size_t SparseLen, const T *source) {
  constexpr size_t B = VectorSizeChooser<T>::VectorSize / sizeof(T);
  auto BS = ripple_set_block_shape(0, B);
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    hvx_gather(destination + ripple_parallel_idx(BS, 0) * B, source, indexes[i],
               SparseLen);
}

template <typename T, typename IT>
void Ripple_scatter(size_t N, const IT *indexes, const T *source,
                    size_t SparseLen, T *destination) {
  auto BS =
      ripple_set_block_shape(0, VectorSizeChooser<T>::VectorSize / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    hvx_scatter(destination, indexes[i], source[i], SparseLen);
}

template void Ripple_gather(size_t N, const int16_t *indexes,
                            int8_t *destination, size_t source_len,
                            const int8_t *source);

template void Ripple_gather(size_t N, const int *indexes, int8_t *destination,
                            size_t source_len, const int8_t *source);

template void Ripple_gather(size_t N, const int16_t *indexes,
                            int16_t *destination, size_t source_len,
                            const int16_t *source);

template void Ripple_gather(size_t N, const int *indexes, int16_t *destination,
                            size_t source_len, const int16_t *source);

template void Ripple_gather(size_t N, const int *indexes, int32_t *destination,
                            size_t source_len, const int32_t *source);

template void Ripple_gather(size_t N, const int *indexes, int64_t *destination,
                            size_t source_len, const int64_t *source);

template void Ripple_scatter(size_t N, const int16_t *indexes,
                             const int8_t *source, size_t destination_len,
                             int8_t *destination);

template void Ripple_scatter(size_t N, const int *indexes, const int8_t *source,
                             size_t destination_len, int8_t *destination);

template void Ripple_scatter(size_t N, const int16_t *indexes,
                             const int16_t *source, size_t destination_len,
                             int16_t *destination);

template void Ripple_scatter(size_t N, const int *indexes,
                             const int16_t *source, size_t destination_len,
                             int16_t *destination);

template void Ripple_scatter(size_t N, const int *indexes,
                             const int32_t *source, size_t destination_len,
                             int32_t *destination);

template void Ripple_scatter(size_t N, const int *indexes,
                             const int64_t *source, size_t destination_len,
                             int64_t *destination);
