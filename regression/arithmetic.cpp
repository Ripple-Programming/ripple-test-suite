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
 * @brief Adds the vectors "a", "b" and populates "apb" with the results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void add_ripple(size_t N, const T *a, const T *b, T *apb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    apb[i] = a[i] + b[i];
}

/**
 * @brief Subtracts the vectors "b" from "a" and populates "asb" with the
 * results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void sub_ripple(size_t N, const T *a, const T *b, T *asb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    asb[i] = a[i] - b[i];
}

/**
 * @brief Multiplies the vectors "a" and "b" and populates "atb" with the
 * results.
 *
 * @tparam T
 * @param N
 * @param a
 * @param b
 */
template <typename T>
void mult_ripple(size_t N, const T *a, const T *b, T *atb) {
  auto BS = ripple_set_block_shape(0, 128 / sizeof(T));
  ripple_parallel(BS, 0);
  for (size_t i = 0; i < N; i++)
    atb[i] = a[i] * b[i];
}

using namespace ripple_test_suite;

enum Op { ADD, SUB, MULT };

template <typename T, Op O, unsigned SigDigits> class OpTest : public Test {
  static constexpr int N = 1729;
  std::vector<T> X, Y, XopY, XopYRef;

public:
  OpTest(TestFramework &TestFramework)
      : Test(TestFramework), X(N, 0), Y(N, 0), XopY(N, 0), XopYRef(N, 0) {
    for (int i = 0; i < N; i++) {
      X[i] = static_cast<T>(42 * (randn() - 0.5));
      Y[i] = static_cast<T>(42 * (randn() - 0.5));
      if (O == Op::ADD)
        XopYRef[i] = X[i] + Y[i];
      if (O == Op::SUB)
        XopYRef[i] = X[i] - Y[i];
      if (O == Op::MULT)
        XopYRef[i] = X[i] * Y[i];
    }
  }

  void run(unsigned) override {
    if (O == Op::ADD)
      add_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
    if (O == Op::SUB)
      sub_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
    if (O == Op::MULT)
      mult_ripple(N, &(X[0]), &(Y[0]), &(XopY[0]));
  }

  bool verify() const override {
    return isclose(N, &(XopY[0]), &(XopYRef[0]), pow(0.1, SigDigits));
  }
};

// #if __has_Float16__
// DefineTest<OpTest<_Float16, Op::ADD, 3>> TF16Add("OpTest<+,f16>");
// DefineTest<OpTest<_Float16, Op::SUB, 3>> TF16Sub("OpTest<-,f16>");
// DefineTest<OpTest<_Float16, Op::MULT, 3>> TF16Mult("OpTest<*,f16>");
// #endif

DefineTest<OpTest<float, Op::ADD, 4>> TF32Add("OpTest<+,f32>");
DefineTest<OpTest<float, Op::SUB, 4>> TF32Sub("OpTest<-,f32>");
DefineTest<OpTest<float, Op::MULT, 4>> TF32Mult("OpTest<*,f32>");

DefineTest<OpTest<double, Op::ADD, 6>> TF64Add("OpTest<+,f64>");
DefineTest<OpTest<double, Op::SUB, 6>> TF64Sub("OpTest<-,f64>");
DefineTest<OpTest<double, Op::MULT, 6>> TF64Mult("OpTest<*,f64>");

#ifdef __hexagon__
// TODO: Fix this for x86.
#if __has_bf16__
DefineTest<OpTest<__bf16, Op::ADD, 3>> TBF16Add("OpTest<+,bf16>");
DefineTest<OpTest<__bf16, Op::SUB, 3>> TBF16Sub("OpTest<-,bf16>");
DefineTest<OpTest<__bf16, Op::MULT, 3>> TBF16Mult("OpTest<*,bf16>");
#endif
#endif

// TODO: Test special floating point numbers. nan, inf, subnormal, etc.

} // namespace
