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

/**
 * @brief Updates "out" as "out[i] = a* x[i] + b * y[i]"
 *
 * @param N The number of elements in the vector.
 * @param a x's scale factor.
 * @param x First array in the linear combination.
 * @param b y's scale factor.
 * @param y Second array in the linear combination.
 * @param out Output vector in which the result of the linear combination is to
 * be stored.
 */
void axpby(unsigned N, float a, const float x[N], float b, const float y[N],
           float out[N]) {
  auto BS = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(float));
  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0), i = 0;

  for (i = 0; i + nv0 <= N; i += RIPPLE_BLOCK_SIZE(float))
    out[i + v0] = a * x[i + v0] + b * y[i + v0];

  if (i + v0 < N)
    out[i + v0] = a * x[i + v0] + b * y[i + v0];
}

} // namespace

namespace {

void axpby_ref(unsigned N, float a, const float x[N], float b, const float y[N],
               float out[N]) {
  for (int i = 0; i < N; i++)
    out[i] = a * x[i] + b * y[i];
}

} // namespace

namespace ripple_test_suite {

class AxpbyTest : public Test {
  static constexpr int N = 1729;
  float Alpha;
  float Beta;
  float X[N], Y[N], Out[N], OutRef[N];

public:
  AxpbyTest(TestFramework &TestFramework)
      : Test(TestFramework), Alpha(-8.0 * randn()), Beta(-8.0 * randn()) {
    for (int i = 0; i < N; i++) {
      X[i] = -32 * randn();
      Y[i] = -32 * randn();
    }

    axpby_ref(N, Alpha, X, Beta, Y, OutRef);
  }

  void run(unsigned) override { axpby(N, Alpha, X, Beta, Y, Out); }

  bool verify() const override { return equal(1e-5, Out, OutRef); }
};

DefineTest<AxpbyTest> AxpbyTestInstance("axpby");

} // namespace ripple_test_suite
