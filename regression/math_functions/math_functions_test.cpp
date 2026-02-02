//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include "math_functions.h"
#include <ripple.h>
#include <stdio.h>

#include <limits>
#include <vector>

namespace {
using namespace ripple_test_suite;

// Reference implementations
template <typename T> T hypot_ref(T x, T y) {
  return static_cast<T>(sqrt(x * x + y * y));
}
template <typename T> T rsqrt_ref(T x) { return static_cast<T>(1.0) / sqrt(x); }

template <typename T, T (*OP)(T)>
void ref_unary_math(size_t N, T *output, const T *input) {
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input[i]);
}

template <typename T, T (*OP)(T, T)>
void ref_binary_math(size_t N, T *output, const T *input1, const T *input2) {
  for (size_t i = 0; i < N; i++)
    output[i] = OP(input1[i], input2[i]);
}

template <typename T> class TestBase : public Test {
protected:
  const size_t N;
  std::vector<T> Input1, Input2, Output;
  std::vector<T> RefOutput;

public:
  TestBase(TestFramework &TF, size_t N)
      : Test(TF), N(N), Input1(N), Output(N), Input2(N), RefOutput(N) {}

  void prepare(unsigned) override {
    // Following input generator needs to
    // be fixed based on the function's input
    // range
    for (size_t i = 0; i < N; ++i) {
      Input1[i] = static_cast<T>(0.1 * i);
      Input2[i] = static_cast<T>(0.1 * i);
    }
  }

  bool verify() const override {
    return isclose(N, &RefOutput[0], &Output[0], 1e-5, 5 * 1e-2);
  }
};

enum { N = 129 };

// Scalar math functions tests
template <typename T, T (*OP)(T)>
class ScalarUnaryMathTest : public TestBase<T> {
public:
  ScalarUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    TestBase<T>::prepare(0);
    ref_unary_math<T, OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    // Run scalar version (reference implementation)
    ref_unary_math<T, OP>(this->N, &this->Output[0], &this->Input1[0]);
  }
};

template <typename T, T (*OP)(T, T)>
class ScalarBinaryMathTest : public TestBase<T> {
public:
  ScalarBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    TestBase<T>::prepare(0);
    ref_binary_math<T, OP>(this->N, &this->RefOutput[0], &this->Input1[0],
                           &this->Input2[0]);
  }

  void run(unsigned) override {
    // Run scalar version (reference implementation)
    ref_binary_math<T, OP>(this->N, &this->Output[0], &this->Input1[0],
                           &this->Input2[0]);
  }
};

// Ripple math functions tests
template <typename T, T (*REF_OP)(T), T (*RIPPLE_OP)(T)>
class RippleUnaryMathTest : public TestBase<T> {
public:
  RippleUnaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}

  void prepare(unsigned) override {
    TestBase<T>::prepare(0);
    ref_unary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0]);
  }

  void run(unsigned) override {
    Ripple_unary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                    &this->Input1[0]);
  }
};

template <typename T, T (*REF_OP)(T, T), T (*RIPPLE_OP)(T, T)>
class RippleBinaryMathTest : public TestBase<T> {
public:
  RippleBinaryMathTest(TestFramework &TF) : TestBase<T>(TF, N) {}
  void prepare(unsigned) override {
    TestBase<T>::prepare(0);
    ref_binary_math<T, REF_OP>(this->N, &this->RefOutput[0], &this->Input1[0],
                               &this->Input2[0]);
  }
  void run(unsigned) override {
    Ripple_binary_math<T, RIPPLE_OP>(this->N, &this->Output[0],
                                     &this->Input1[0], &this->Input2[0]);
  }
};

// Unary float
DefineTest<ScalarUnaryMathTest<float, sinf_fn>>
    ScalarFloatSin("ScalarFloatSin");
DefineTest<RippleUnaryMathTest<float, sinf_fn, sinf_fn>>
    RippleFloatSin("RippleFloatSin");

DefineTest<ScalarUnaryMathTest<float, cosf_fn>>
    ScalarFloatCos("ScalarFloatCos");
DefineTest<RippleUnaryMathTest<float, cosf_fn, cosf_fn>>
    RippleFloatCos("RippleFloatCos");

DefineTest<ScalarUnaryMathTest<float, tanf_fn>>
    ScalarFloatTan("ScalarFloatTan");
DefineTest<RippleUnaryMathTest<float, tanf_fn, tanf_fn>>
    RippleFloatTan("RippleFloatTan");

DefineTest<ScalarUnaryMathTest<float, asinf_fn>>
    ScalarFloatAsin("ScalarFloatAsin");
DefineTest<RippleUnaryMathTest<float, asinf_fn, asinf_fn>>
    RippleFloatAsin("RippleFloatAsin");

DefineTest<ScalarUnaryMathTest<float, acosf_fn>>
    ScalarFloatAcos("ScalarFloatAcos");
DefineTest<RippleUnaryMathTest<float, acosf_fn, acosf_fn>>
    RippleFloatAcos("RippleFloatAcos");

DefineTest<ScalarUnaryMathTest<float, atanf_fn>>
    ScalarFloatAtan("ScalarFloatAtan");
DefineTest<RippleUnaryMathTest<float, atanf_fn, atanf_fn>>
    RippleFloatAtan("RippleFloatAtan");

DefineTest<ScalarUnaryMathTest<float, sinhf_fn>>
    ScalarFloatSinh("ScalarFloatSinh");
DefineTest<RippleUnaryMathTest<float, sinhf_fn, sinhf_fn>>
    RippleFloatSinh("RippleFloatSinh");

DefineTest<ScalarUnaryMathTest<float, coshf_fn>>
    ScalarFloatCosh("ScalarFloatCosh");
DefineTest<RippleUnaryMathTest<float, coshf_fn, coshf_fn>>
    RippleFloatCosh("RippleFloatCosh");

DefineTest<ScalarUnaryMathTest<float, tanhf_fn>>
    ScalarFloatTanh("ScalarFloatTanh");
DefineTest<RippleUnaryMathTest<float, tanhf_fn, tanhf_fn>>
    RippleFloatTanh("RippleFloatTanh");

DefineTest<ScalarUnaryMathTest<float, expf_fn>>
    ScalarFloatExp("ScalarFloatExp");
DefineTest<RippleUnaryMathTest<float, expf_fn, expf_fn>>
    RippleFloatExp("RippleFloatExp");

DefineTest<ScalarUnaryMathTest<float, logf_fn>>
    ScalarFloatLog("ScalarFloatLog");
DefineTest<RippleUnaryMathTest<float, logf_fn, logf_fn>>
    RippleFloatLog("RippleFloatLog");

DefineTest<ScalarUnaryMathTest<float, sqrtf_fn>>
    ScalarFloatSqrt("ScalarFloatSqrt");
DefineTest<RippleUnaryMathTest<float, sqrtf_fn, sqrtf_fn>>
    RippleFloatSqrt("RippleFloatSqrt");

DefineTest<ScalarUnaryMathTest<float, rsqrt_ref>>
    ScalarFloatRsqrt("ScalarFloatRsqrt");
DefineTest<RippleUnaryMathTest<float, rsqrt_ref, rsqrtf_fn>>
    RippleFloatRsqrt("RippleFloatRsqrt");

DefineTest<ScalarUnaryMathTest<float, truncf_fn>>
    ScalarFloatTrunc("ScalarFloatTrunc");
DefineTest<RippleUnaryMathTest<float, truncf_fn, truncf_fn>>
    RippleFloatTrunc("RippleFloatTrunc");

DefineTest<ScalarUnaryMathTest<float, roundf_fn>>
    ScalarFloatRound("ScalarFloatRound");
DefineTest<RippleUnaryMathTest<float, roundf_fn, roundf_fn>>
    RippleFloatRound("RippleFloatRound");

DefineTest<ScalarUnaryMathTest<float, floorf_fn>>
    ScalarFloatFloor("ScalarFloatFloor");
DefineTest<RippleUnaryMathTest<float, floorf_fn, floorf_fn>>
    RippleFloatFloor("RippleFloatFloor");

DefineTest<ScalarUnaryMathTest<float, ceilf_fn>>
    ScalarFloatCeil("ScalarFloatCeil");
DefineTest<RippleUnaryMathTest<float, ceilf_fn, ceilf_fn>>
    RippleFloatCeil("RippleFloatCeil");

// Unary _Float16
DefineTest<ScalarUnaryMathTest<_Float16, sinf16_fn>>
    ScalarHalfSin("ScalarHalfSin");
DefineTest<RippleUnaryMathTest<_Float16, sinf16_fn, sinf16_fn>>
    RippleHalfSin("RippleHalfSin");

DefineTest<ScalarUnaryMathTest<_Float16, cosf16_fn>>
    ScalarHalfCos("ScalarHalfCos");
DefineTest<RippleUnaryMathTest<_Float16, cosf16_fn, cosf16_fn>>
    RippleHalfCos("RippleHalfCos");

DefineTest<ScalarUnaryMathTest<_Float16, tanf16_fn>>
    ScalarHalfTan("ScalarHalfTan");
DefineTest<RippleUnaryMathTest<_Float16, tanf16_fn, tanf16_fn>>
    RippleHalfTan("RippleHalfTan");

DefineTest<ScalarUnaryMathTest<_Float16, asinf16_fn>>
    ScalarHalfAsin("ScalarHalfAsin");
DefineTest<RippleUnaryMathTest<_Float16, asinf16_fn, asinf16_fn>>
    RippleHalfAsin("RippleHalfAsin");

DefineTest<ScalarUnaryMathTest<_Float16, acosf16_fn>>
    ScalarHalfAcos("ScalarHalfAcos");
DefineTest<RippleUnaryMathTest<_Float16, acosf16_fn, acosf16_fn>>
    RippleHalfAcos("RippleHalfAcos");

DefineTest<ScalarUnaryMathTest<_Float16, atanf16_fn>>
    ScalarHalfAtan("ScalarHalfAtan");
DefineTest<RippleUnaryMathTest<_Float16, atanf16_fn, atanf16_fn>>
    RippleHalfAtan("RippleHalfAtan");

DefineTest<ScalarUnaryMathTest<_Float16, sinhf16_fn>>
    ScalarHalfSinh("ScalarHalfSinh");
DefineTest<RippleUnaryMathTest<_Float16, sinhf16_fn, sinhf16_fn>>
    RippleHalfSinh("RippleHalfSinh");

DefineTest<ScalarUnaryMathTest<_Float16, coshf16_fn>>
    ScalarHalfCosh("ScalarHalfCosh");
DefineTest<RippleUnaryMathTest<_Float16, coshf16_fn, coshf16_fn>>
    RippleHalfCosh("RippleHalfCosh");

DefineTest<ScalarUnaryMathTest<_Float16, tanhf16_fn>>
    ScalarHalfTanh("ScalarHalfTanh");
DefineTest<RippleUnaryMathTest<_Float16, tanhf16_fn, tanhf16_fn>>
    RippleHalfTanh("RippleHalfTanh");

DefineTest<ScalarUnaryMathTest<_Float16, expf16_fn>>
    ScalarHalfExp("ScalarHalfExp");
DefineTest<RippleUnaryMathTest<_Float16, expf16_fn, expf16_fn>>
    RippleHalfExp("RippleHalfExp");

DefineTest<ScalarUnaryMathTest<_Float16, logf16_fn>>
    ScalarHalfLog("ScalarHalfLog");
DefineTest<RippleUnaryMathTest<_Float16, logf16_fn, logf16_fn>>
    RippleHalfLog("RippleHalfLog");

DefineTest<ScalarUnaryMathTest<_Float16, sqrtf16_fn>>
    ScalarHalfSqrt("ScalarHalfSqrt");
DefineTest<RippleUnaryMathTest<_Float16, sqrtf16_fn, sqrtf16_fn>>
    RippleHalfSqrt("RippleHalfSqrt");

DefineTest<ScalarUnaryMathTest<_Float16, rsqrt_ref>>
    ScalarHalfRsqrt("ScalarHalfRsqrt");
DefineTest<RippleUnaryMathTest<_Float16, rsqrt_ref, rsqrtf16_fn>>
    RippleHalfRsqrt("RippleHalfRsqrt");

DefineTest<ScalarUnaryMathTest<_Float16, truncf16_fn>>
    ScalarHalfTrunc("ScalarHalfTrunc");
DefineTest<RippleUnaryMathTest<_Float16, truncf16_fn, truncf16_fn>>
    RippleHalfTrunc("RippleHalfTrunc");

DefineTest<ScalarUnaryMathTest<_Float16, roundf16_fn>>
    ScalarHalfRound("ScalarHalfRound");
DefineTest<RippleUnaryMathTest<_Float16, roundf16_fn, roundf16_fn>>
    RippleHalfRound("RippleHalfRound");

DefineTest<ScalarUnaryMathTest<_Float16, floorf16_fn>>
    ScalarHalfFloor("ScalarHalfFloor");
DefineTest<RippleUnaryMathTest<_Float16, floorf16_fn, floorf16_fn>>
    RippleHalfFloor("RippleHalfFloor");

DefineTest<ScalarUnaryMathTest<_Float16, ceilf16_fn>>
    ScalarHalfCeil("ScalarHalfCeil");
DefineTest<RippleUnaryMathTest<_Float16, ceilf16_fn, ceilf16_fn>>
    RippleHalfCeil("RippleHalfCeil");

// Binary Float
DefineTest<ScalarBinaryMathTest<float, powf_fn>>
    ScalarFloatPow("ScalarFloatPow");
DefineTest<RippleBinaryMathTest<float, powf_fn, powf_fn>>
    RippleFloatPow("RippleFloatPow");

DefineTest<ScalarBinaryMathTest<float, hypot_ref>>
    ScalarFloatHypot("ScalarFloatHypot");
DefineTest<RippleBinaryMathTest<float, hypot_ref, hypotf_fn>>
    RippleFloatHypot("RippleFloatHypot");

// Binary _Float16
DefineTest<ScalarBinaryMathTest<_Float16, powf16_fn>>
    ScalarHalfPow("ScalarHalfPow");
DefineTest<RippleBinaryMathTest<_Float16, powf16_fn, powf16_fn>>
    RippleHalfPow("RippleHalfPow");

DefineTest<ScalarBinaryMathTest<_Float16, hypot_ref>>
    ScalarHalfHypot("ScalarHalfHypot");
DefineTest<RippleBinaryMathTest<_Float16, hypot_ref, hypotf16_fn>>
    RippleHalfHypot("RippleHalfHypot");

} // namespace
