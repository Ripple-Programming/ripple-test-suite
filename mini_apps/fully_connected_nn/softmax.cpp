//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include <limits>
#include <math.h>

#include <ripple.h>
#include <ripple_math.h>

namespace {

template <unsigned Nj, unsigned Tj>
void softmax(unsigned N, const float (*X)[Nj], float (*XSoftmax)[Nj]) {
  auto BS = ripple_set_block_shape(0, Tj);
  for (int i = 0; i < N; i++) {
    size_t v0 = ripple_id(BS, 0), j;
    float row_max =
        -std::numeric_limits<float>::max(); // TODO: use lowest() from C++ 11.
    for (j = 0; j + Tj <= Nj; j += Tj)
      row_max = std::max(row_max, ripple_reducemax(0b1, X[i][j + v0]));
    if (j + v0 < Nj)
      row_max = std::max(row_max, ripple_reducemax(0b1, X[i][j + v0]));
    // TODO: use the output array for e_x_scaled?
    float e_x_scaled[Nj];
    float e_x_scaled_sum = 0.0;
    for (j = 0; j + Tj <= Nj; j += Tj) {
      float t = expf(X[i][j + v0] - row_max);
      e_x_scaled[j + v0] = t;
      e_x_scaled_sum += ripple_reduceadd(0b1, t);
    }
    if (j + v0 < Nj) {
      float t = expf(X[i][j + v0] - row_max);
      e_x_scaled[j + v0] = t;
      e_x_scaled_sum += ripple_reduceadd(0b1, t);
    }
    for (j = 0; j + Tj <= Nj; j += Tj)
      XSoftmax[i][j + v0] = e_x_scaled[j + v0] / e_x_scaled_sum;
    if (j + v0 < Nj)
      XSoftmax[i][j + v0] = e_x_scaled[j + v0] / e_x_scaled_sum;
  }
}

using namespace std;

template <unsigned Nj>
void softmax_ref(unsigned N, const float (*X)[Nj], float (*XSoftmax)[Nj]) {
  for (int i = 0; i < N; ++i) {
    float row_max =
        -numeric_limits<float>::max(); // TODO: use lowest() from C++ 11.
    float e_x_scaled[Nj], e_x_scaled_sum = 0.;

    for (int j = 0; j < Nj; ++j) // Compute scaling factor
      row_max = fmaxf(row_max, X[i][j]);

    for (int j = 0; j < Nj; ++j) { // Compute the sum.
      e_x_scaled[j] = expf(X[i][j] - row_max);
      e_x_scaled_sum += e_x_scaled[j];
    }

    for (int j = 0; j < Nj; ++j) { // Write the result.
      XSoftmax[i][j] = e_x_scaled[j] / e_x_scaled_sum;
    }
  }
}

using namespace ripple_test_suite;

template <unsigned Nj, unsigned Tj> class SoftmaxTest : public Test {
  static constexpr int Nimage = 32;
  float XRef[Nimage][Nj], XSoftmaxRef[Nimage][Nj], XSoftmax[Nimage][Nj];

public:
  SoftmaxTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < Nimage; i++)
      for (int j = 0; j < Nj; j++) {
        float NormRand = static_cast<double>(rand()) / RAND_MAX;
        XRef[i][j] = 8 * (NormRand - 0.5); // get a random value in (-4, 4)
      }
    softmax_ref<Nj>(Nimage, &XRef[0], &XSoftmaxRef[0]);
  }

  void run(unsigned) override {
    softmax<Nj, Tj>(Nimage, &XRef[0], &XSoftmax[0]);
  }

  bool verify() const override { return equal(1e-5, XSoftmaxRef, XSoftmax); }
};

// TODO: This crashes Hexagon compiler.
#ifndef __hexagon__
DefineTest<SoftmaxTest<10, 10>> T("softmax[10],tile=10");
#endif
} // namespace
