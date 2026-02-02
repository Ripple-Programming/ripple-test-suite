//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>

#include <ripple-test-suite/ripple-test-suite.h>

namespace {

template <unsigned N>
static __attribute__((always_inline)) void
sum_along_columns(unsigned M, const float (*X)[N], float Y[N]) {
  auto BS = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(float));
  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0), j = 0;

  for (j = 0; j + nv0 <= N; j += nv0) {
    float acc = 0;
    for (int i = 0; i < M; i++)
      acc += X[i][j + v0];
    Y[j + v0] = acc;
  }
  if (j + v0 < N) {
    float acc = 0;
    for (int i = 0; i < M; i++)
      acc += X[i][j + v0];
    Y[j + v0] = acc;
  }
}

template <unsigned N>
void sum_along_columns_ref(unsigned M, const float (*X)[N], float Y[N]) {
  for (int j = 0; j < N; j++) {
    float acc = 0;
    for (int i = 0; i < M; i++) {
      acc += X[i][j];
    }
    Y[j] = acc;
  }
}

using namespace ripple_test_suite;

template <unsigned N> class SumAlongColumnsTest : public Test {
  static constexpr int M = 1729;
  float X[M][N], ColumnSumRef[N], ColumnSum[N];

public:
  SumAlongColumnsTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < M; i++) {
      for (int j = 0; j < N; j++)
        X[i][j] = -32 * randn();
    }
    sum_along_columns_ref<N>(M, X, ColumnSumRef);
  }

  void run(unsigned) override { sum_along_columns<N>(M, X, ColumnSum); }

  bool verify() const override { return equal(1e-5, ColumnSum, ColumnSumRef); }
};

DefineTest<SumAlongColumnsTest<10>> T_10("sum_along_columns[10]");
DefineTest<SumAlongColumnsTest<300>> T_300("sum_along_columns[300]");

} // namespace
