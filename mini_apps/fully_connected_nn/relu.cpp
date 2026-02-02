//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <math.h>
#include <ripple-test-suite/ripple-test-suite.h>

#include <ripple.h>
#include <ripple_math.h>

namespace {

void relu(unsigned N, const float X[N], float Y[N]) {
  auto BS = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(float));

  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0), i = 0;

  for (; i + nv0 <= N; i += RIPPLE_BLOCK_SIZE(float))
    Y[i + v0] = fmaxf(X[i + v0], 0);

  if (i + v0 < N)
    Y[i + v0] = fmaxf(X[i + v0], 0);
}

void relu_backward(unsigned N, const float X[N], const float dReluX[N],
                   float dX[N]) {
  auto BS = ripple_set_block_shape(0, RIPPLE_BLOCK_SIZE(float));

  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0), i = 0;

  for (; i + nv0 <= N; i += RIPPLE_BLOCK_SIZE(float))
    dX[i + v0] = (X[i + v0] >= 0) ? 0 : dReluX[i + v0];

  if (i + v0 < N)
    dX[i + v0] = (X[i + v0] >= 0) ? 0 : dReluX[i + v0];
}

void relu_ref(unsigned N, const float X[N], float Y[N]) {
  for (int i = 0; i < N; i++)
    Y[i] = fmaxf(X[i], 0);
}

void relu_backward_ref(unsigned N, const float X[N], const float dReluX[N],
                       float dX[N]) {
  for (int i = 0; i < N; i++)
    dX[i] = (X[i] >= 0) ? 0 : dReluX[i];
}

using namespace ripple_test_suite;

class ReLUTest : public Test {
  static constexpr int N = 1729;
  float X[N], ReluXRef[N], ReluX[N];

public:
  ReLUTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++) {
      X[i] = -32 * randn();
    }
    relu_ref(N, X, ReluXRef);
  }

  void run(unsigned) override { relu(N, X, ReluX); }

  bool verify() const override { return equal(1e-5, ReluXRef, ReluX); }
};

DefineTest<ReLUTest> T("ReLU");

class BackReLUTest : public Test {
  static constexpr int N = 1729;
  float X[N], DReluX[N], DXRef[N], DX[N];

public:
  BackReLUTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++) {
      X[i] = -32 * randn();
      DReluX[i] = -32 * randn();
    }
    relu_backward_ref(N, X, DReluX, DXRef);
  }

  void run(unsigned) override { relu_backward(N, X, DReluX, DX); }

  bool verify() const override { return equal(1e-5, DXRef, DX); }
};

DefineTest<BackReLUTest> TB("ReLU,backward");

} // namespace
