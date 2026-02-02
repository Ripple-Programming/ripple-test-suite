//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include <ripple.h>
#include <ripple_math.h>

namespace {

/**
 * @brief Returns the Loss as per cross entropy function
 * <https://en.wikipedia.org/wiki/Cross-entropy>
 *
 * @param N Number of batches.
 * @param Yref The reference (golden) probablistic distribution.
 * @param Ypred The predected probablistic distribution.
 * @return float
 */
float categorical_cross_entropy_loss(unsigned N, const float Yref[N][10],
                                     const float Ypred[N][10]) {
  const float epsilon = 1e-12; // To avoid overflows.
  auto BS = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(float));
  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0), i = 0;

  const float *Yrefptr = &Yref[0][0], *Ypredptr = &Ypred[0][0];

  float acc = 0.0;

  for (i = 0; i + nv0 <= N * 10; i += RIPPLE_BLOCK_SIZE(float))
    acc += Yrefptr[i + v0] * logf(Ypredptr[i + v0] + epsilon);

  if (i + v0 < N * 10)
    acc += Yrefptr[i + v0] * logf(Ypredptr[i + v0] + epsilon);

  return -ripple_reduceadd(0b1, acc) / N;
}

float categorical_cross_entropy_loss_ref(unsigned N, const float Yref[N][10],
                                         const float Ypred[N][10]) {
  const float epsilon = 1e-12; // To avoid overflows.
  const float *Yrefptr = &Yref[0][0], *Ypredptr = &Ypred[0][0];

  float acc = 0.0;

  for (int i = 0; i < N * 10; i++) {
    acc += (Yrefptr[i] * logf(Ypredptr[i] + epsilon));
  }
  return -acc / N;
}

using namespace ripple_test_suite;

class CrossEntropyLossTest : public Test {
  static constexpr int N = 1729;
  float Yref[N][10], Ypred[N][10];
  float LossRef, Loss;

public:
  CrossEntropyLossTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++)
      for (int j = 0; j < 10; ++j) {
        Yref[i][j] = randn();
        Ypred[i][j] = randn();
      }
    LossRef = categorical_cross_entropy_loss_ref(N, Yref, Ypred);
  }

  void run(unsigned) override {
    Loss = categorical_cross_entropy_loss(N, Yref, Ypred);
  }

  bool verify() const override { return equal(1e-5, Loss, LossRef); }
};

DefineTest<CrossEntropyLossTest> T("cross_entropy_loss");

} // namespace
