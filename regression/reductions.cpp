//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

// TODO: Must be included before ripple_math.h.
#include <ripple-test-suite/ripple-test-suite.h>

#include <limits>
#include <ripple.h>
#include <ripple_math.h>

enum ReduceOp { ADD, MIN, MAX };

template <ReduceOp Op, typename T> struct Reduce;

template <typename T> struct Reduce<ADD, T> {
  // TODO: Could be constexpr variable.
  static T neutral_element() { return 0; }
  template <uint64_t mask> static T op(const T &x) {
    return ripple_reduceadd(mask, x);
  }
  static T update(T acc, T x) { return acc + x; }
};

template <typename T> struct Reduce<MIN, T> {
  static T neutral_element() {
    return std::numeric_limits<T>::has_infinity
               ? std::numeric_limits<T>::infinity()
               : std::numeric_limits<T>::max();
  }
  template <uint64_t mask> static T op(const T &x) {
    return ripple_reducemin(mask, x);
  }
  static T update(T acc, T x) { return std::min(acc, x); }
};

template <typename T> struct Reduce<MAX, T> {
  static T neutral_element() {
    return std::numeric_limits<T>::has_infinity
               ? -std::numeric_limits<T>::infinity()
               : std::numeric_limits<T>::min();
  }
  template <uint64_t mask> static T op(const T &x) {
    return ripple_reducemax(mask, x);
  }
  static T update(T acc, T x) { return std::max(acc, x); }
};

template <typename T, ReduceOp Op> T neutral_element() {
  return Reduce<Op, T>::neutral_element();
}

template <uint64_t mask, ReduceOp Op, typename T> T reduce_op(const T &x) {
  return Reduce<Op, T>::template op<mask>(x);
}

template <typename T, ReduceOp Op> T reduce_update(const T &acc, const T &x) {
  return Reduce<Op, T>::update(acc, x);
}

template <typename T, unsigned K, ReduceOp O>
void reduce_1d(int N, const T (*X)[K], T *Y) {
  auto BS = ripple_set_block_shape(0, K);
  size_t v0 = ripple_id(BS, 0);
  for (int i = 0; i < N; i++)
    Y[i] = reduce_op<0b1, O>(X[i][v0]);
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_0(int N, const T (*X)[K][L], T *Y) {
  auto BS = ripple_set_block_shape(0, L, K);
  size_t v0 = ripple_id(BS, 0), v1 = ripple_id(BS, 1);
  for (int i = 0; i < N; i++)
    Y[i] = reduce_op<0b11, O>(X[i][v1][v0]);
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_1(int N, const T (*X)[K][L], T (*Y)[K]) {
  auto BS = ripple_set_block_shape(0, L, K);
  size_t v0 = ripple_id(BS, 0), v1 = ripple_id(BS, 1);
  for (int i = 0; i < N; i++)
    Y[i][v1] = reduce_op<0b1, O>(X[i][v1][v0]);
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_2(int N, const T (*X)[K][L], T (*Y)[L]) {
  auto BS = ripple_set_block_shape(0, L, K);
  size_t v0 = ripple_id(BS, 0), v1 = ripple_id(BS, 1);
  for (int i = 0; i < N; i++)
    Y[i][v0] = reduce_op<0b10, O>(X[i][v1][v0]);
}

template <typename T, unsigned K, ReduceOp O>
void reduce_1d_ref(int N, const T (*X)[K], T *Y) {
  for (int i = 0; i < N; ++i) {
    T acc = neutral_element<T, O>();
    for (int k = 0; k < K; ++k) {
      acc = reduce_update<T, O>(acc, X[i][k]);
    }
    Y[i] = acc;
  }
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_0_ref(int N, const T (*X)[K][L], T *Y) {
  for (int i = 0; i < N; i++) {
    T acc = neutral_element<T, O>();
    for (int k = 0; k < K; ++k) {
      for (int l = 0; l < L; ++l) {
        acc = reduce_update<T, O>(acc, X[i][k][l]);
      }
    }
    Y[i] = acc;
  }
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_1_ref(int N, const T (*X)[K][L], T (*Y)[K]) {
  for (int i = 0; i < N; i++) {
    for (int k = 0; k < K; ++k) {
      T acc = neutral_element<T, O>();
      for (int l = 0; l < L; ++l) {
        acc = reduce_update<T, O>(acc, X[i][k][l]);
      }
      Y[i][k] = acc;
    }
  }
}

template <typename T, unsigned K, unsigned L, ReduceOp O>
void reduce_2d_2_ref(int N, const T (*X)[K][L], T (*Y)[L]) {
  for (int i = 0; i < N; i++) {
    for (int l = 0; l < L; ++l) {
      T acc = neutral_element<T, O>();
      for (int k = 0; k < K; ++k) {
        acc = reduce_update<T, O>(acc, X[i][k][l]);
      }
      Y[i][l] = acc;
    }
  }
}

using namespace ripple_test_suite;

template <typename T, ReduceOp O, unsigned SigDigits = 10, unsigned K = 23>
class Reduce1DTest : public Test {
  static constexpr int N = 128;
  T X[N][K], XRed[N], XRedRef[N];

public:
  Reduce1DTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++) {
      for (int k = 0; k < K; k++) {
        X[i][k] = std::numeric_limits<T>::max() *
                  (randn() - (O == ReduceOp::ADD ? 0 : 0.5));
      }
    }
    reduce_1d_ref<T, K, O>(N, X, XRedRef);
  }

  void run(unsigned) override { reduce_1d<T, K, O>(N, X, XRed); }

  bool verify() const override {
    return isclose(N, &(XRed[0]), &(XRedRef[0]), pow(0.1, SigDigits));
  }
};

template <typename T, ReduceOp O, unsigned SigDigits = 10, unsigned K = 23,
          unsigned L = 23>
class Reduce2DTest : public Test {
  static constexpr int N = 128;
  T X[N][K][L], XRed0[N], XRed1[N][K], XRed2[N][L], XRed0Ref[N], XRed1Ref[N][K],
      XRed2Ref[N][L];

public:
  Reduce2DTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; i++) {
      for (int k = 0; k < K; k++) {
        for (int l = 0; l < L; l++) {
          X[i][k][l] = std::numeric_limits<T>::max() *
                       (randn() - (O == ReduceOp::ADD ? 0 : 0.5));
        }
      }
    }
    reduce_2d_0_ref<T, K, L, O>(N, X, XRed0Ref);
    reduce_2d_1_ref<T, K, L, O>(N, X, XRed1Ref);
    reduce_2d_2_ref<T, K, L, O>(N, X, XRed2Ref);
  }

  void run(unsigned) override {
    reduce_2d_0<T, K, L, O>(N, X, XRed0);
    reduce_2d_1<T, K, L, O>(N, X, XRed1);
    reduce_2d_2<T, K, L, O>(N, X, XRed2);
  }

  bool verify() const override {
    return (
        isclose(N, &(XRed0[0]), &(XRed0Ref[0]), pow(0.1, SigDigits)) &&
        isclose(N * K, &(XRed1[0][0]), &(XRed1Ref[0][0]),
                pow(0.1, SigDigits)) &&
        isclose(N * L, &(XRed2[0][0]), &(XRed2Ref[0][0]), pow(0.1, SigDigits)));
  }
};

DefineTest<Reduce1DTest<float, ReduceOp::ADD, 2>>
    TF32ReduceAdd1D("ReduceTest1D<+,f32>");
DefineTest<Reduce2DTest<float, ReduceOp::ADD, 2>>
    TF32ReduceAdd2D("ReduceTest2D<+,f32>");
DefineTest<Reduce1DTest<double, ReduceOp::ADD, 4>>
    TF64ReduceAdd1D("ReduceTest1D<+,f64>");
DefineTest<Reduce2DTest<double, ReduceOp::ADD, 4>>
    TF64ReduceAdd2D("ReduceTest2D<+,f64>");

DefineTest<Reduce1DTest<float, ReduceOp::MAX>>
    TF32ReduceMax1D("ReduceTest1D<max,f32>");
DefineTest<Reduce2DTest<float, ReduceOp::MAX>>
    TF32ReduceMax2D("ReduceTest2D<max,f32>");
DefineTest<Reduce1DTest<double, ReduceOp::MAX>>
    TF64ReduceMax1D("ReduceTest1D<max,f64>");
DefineTest<Reduce2DTest<double, ReduceOp::MAX>>
    TF64ReduceMax2D("ReduceTest2D<max,f64>");

DefineTest<Reduce1DTest<float, ReduceOp::MIN>>
    TF32ReduceMin1D("ReduceTest1D<min,f32>");
DefineTest<Reduce2DTest<float, ReduceOp::MIN>>
    TF32ReduceMin2D("ReduceTest2D<min,f32>");
DefineTest<Reduce1DTest<double, ReduceOp::MIN>>
    TF64ReduceMin1D("ReduceTest1D<min,f64>");
DefineTest<Reduce2DTest<double, ReduceOp::MIN>>
    TF64ReduceMin2D("ReduceTest2D<min,f64>");

#if __has_bf16__
DefineTest<Reduce1DTest<__bf16, ReduceOp::ADD, 2>>
    TBF16ReduceAdd1D("ReduceTest1D<+,bf16>");
DefineTest<Reduce2DTest<__bf16, ReduceOp::ADD, 2>>
    TBF16ReduceAdd2D("ReduceTest2D<+,bf16>");
DefineTest<Reduce1DTest<__bf16, ReduceOp::MAX>>
    TBF16ReduceMax1D("ReduceTest1D<max,bf16>");
DefineTest<Reduce2DTest<__bf16, ReduceOp::MAX>>
    TBF16ReduceMax2D("ReduceTest2D<max,bf16>");
DefineTest<Reduce1DTest<__bf16, ReduceOp::MIN>>
    TBF16ReduceMin1D("ReduceTest1D<min,bf16>");
DefineTest<Reduce2DTest<__bf16, ReduceOp::MIN>>
    TBF16ReduceMin2D("ReduceTest2D<min,bf16>");
#endif

// TODO: Test special floating point numbers. nan, inf, subnormal, etc.
