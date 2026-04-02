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

#include <hexagon_protos.h>
#include <hexagon_types.h>

#include "scatter_gather.h"

#include <cassert>
#include <limits>

namespace {

// Reference implementations.
template <typename T, typename IT>
void ref_gather(size_t N, const IT *index, T *destination, size_t source_len,
                const T *source) {
  for (size_t i = 0; i < N; ++i) {
    assert(index[i] >= 0);
    assert(index[i] < source_len);
    destination[i] = source[index[i]];
  }
}

template <typename T, typename IT>
void ref_scatter(size_t N, const IT *index, const T *source,
                 size_t destination_len, T *destination) {
  for (size_t i = 0; i < N; ++i) {
    assert(index[i] >= 0);
    assert(index[i] < destination_len);
    destination[index[i]] = source[i];
  }
}

// A variable-length array that can be accessed as either an array of HVX
// vectors or scalars.
template <typename T> union TypedVector {
  HVX_Vector v[];
  T s[];
};

using namespace ripple_test_suite;

template <typename T, typename IT> class TestBase : public Test {
protected:
  const size_t SparseLen;
  const size_t DenseLen;
  const size_t DenseAllocLen;

  TypedVector<T> *Sparse;   // SparseLen
  TypedVector<T> *Dense;    // DenseLen
  TypedVector<IT> *Indexes; // DenseLen

public:
  TestBase(TestFramework &TestFramework, size_t DenseLen, size_t SparseLen)
      : Test(TestFramework),
        SparseLen(
            std::min(SparseLen, (size_t)std::numeric_limits<IT>::max() + 1)),
        DenseLen(DenseLen),
        DenseAllocLen((DenseLen + sizeof(HVX_Vector) / sizeof(T) - 1) &
                      ~(sizeof(HVX_Vector) / sizeof(T) - 1)) {
    assert(DenseLen < SparseLen);
  }
  void generateIndexes(size_t N, IT *X) {
    // Generate a random permutation.
    std::vector<IT> t(SparseLen);
    for (size_t i = 0; i != SparseLen; ++i)
      t[i] = i;
    for (size_t i = SparseLen - 1; i != 0; --i)
      std::swap(t[i], t[size_t(randn() * (i + 1))]);
    std::copy(t.begin(), t.begin() + N, X);
  }
  void prepareCommon() {
    char *vtcmBase = (char *)F().vtcm();
    char *p = vtcmBase;
    Sparse = (TypedVector<T> *)p;
    p += SparseLen * sizeof(T);
    Dense = (TypedVector<T> *)p;
    p += DenseAllocLen * sizeof(T);
    Indexes = (TypedVector<IT> *)p;
    p += DenseLen * sizeof(IT);
    if (p > vtcmBase + F().vtcmSize())
      std::cerr << "VTCM overflow, available " << F().vtcmSize() << ", need "
                << p - vtcmBase << " bytes." << std::endl;
    generateIndexes(DenseLen, Indexes->s);
  }
};

// Array size, in bytes.
// TODO: Test coverage should be extended to arrays of other sizes. Note that
// the maximum size is limited by the index type range, which is a signed type.
enum { N = 128 + 5, SparseLen = 128 * 1024 };

template <typename T, typename IT>
class GatherTestBase : public TestBase<T, IT> {
  using TestBase<T, IT>::SparseLen;
  using TestBase<T, IT>::DenseLen;
  using TestBase<T, IT>::DenseAllocLen;
  using TestBase<T, IT>::Sparse;
  using TestBase<T, IT>::Dense;
  using TestBase<T, IT>::Indexes;

public:
  GatherTestBase(TestFramework &TF) : TestBase<T, IT>(TF, N, ::SparseLen) {}
  void prepare(unsigned NumIterations) override {
    TestBase<T, IT>::prepareCommon();
    for (size_t i = 0; i != SparseLen; ++i)
      Sparse->s[i] = i;
    for (size_t i = 0; i != DenseAllocLen; ++i)
      Dense->s[i] = -1;
  }
  bool verify() const override {
    std::vector<T> RefOutput(DenseLen);
    ref_gather(DenseLen, Indexes->s, &RefOutput[0], SparseLen, Sparse->s);
    bool ret = true;
    for (size_t i = 0; i != DenseAllocLen; ++i) {
      T Ref = i < DenseLen ? RefOutput[i] : -1;
      if (Dense->s[i] != Ref)
        ret = false;
    }
    return ret;
  }
  void SyncOutput() const {
    const TypedVector<T> *Ptr = Dense;
    size_t Size = DenseAllocLen;
    for (size_t i = 0; i != Size * sizeof(T) / sizeof(HVX_Vector); ++i)
      *(const volatile HVX_Vector *)&Ptr->v[i];
  }
};

template <typename T, typename IT>
class ScatterTestBase : public TestBase<T, IT> {
  using TestBase<T, IT>::SparseLen;
  using TestBase<T, IT>::DenseLen;
  using TestBase<T, IT>::DenseAllocLen;
  using TestBase<T, IT>::Sparse;
  using TestBase<T, IT>::Dense;
  using TestBase<T, IT>::Indexes;

public:
  ScatterTestBase(TestFramework &TF) : TestBase<T, IT>(TF, N, ::SparseLen) {}
  void prepare(unsigned NumIterations) override {
    TestBase<T, IT>::prepareCommon();
    for (size_t i = 0; i != SparseLen; ++i)
      Sparse->s[i] = -1;
    for (size_t i = 0; i != DenseAllocLen; ++i)
      Dense->s[i] = i;
  }
  bool verify() const override {
    std::vector<T> RefOutput(SparseLen);
    std::fill(RefOutput.begin(), RefOutput.end(), -1);
    ref_scatter(DenseLen, Indexes->s, Dense->s, SparseLen, &RefOutput[0]);
    bool ret = true;
    for (size_t i = 0; i != SparseLen; ++i) {
      if (Sparse->s[i] != RefOutput[i])
        ret = false;
    }
    return ret;
  }
  void SyncOutput() const {
    // The address must be anywhere in VTCM, and the same address must be used
    // by the store and load.
    // TODO: Can an HVX register be specified as an inline asm operand?
    __asm __volatile("\
        vmem(%0+#0):scatter_release \n\
        v0 = vmem(%0)               \n\
        "
                     :
                     : "r"(Sparse)
                     : "v0", "memory");
  }
};

// Scalar tests.
template <typename T, typename IT = int>
class ScalarGatherTest : public GatherTestBase<T, IT> {
public:
  ScalarGatherTest(TestFramework &TF) : GatherTestBase<T, IT>(TF) {}
  void run(unsigned) override {
    ref_gather(N, TestBase<T, IT>::Indexes->s, TestBase<T, IT>::Dense->s,
               TestBase<T, IT>::SparseLen, TestBase<T, IT>::Sparse->s);
  }
};

template <typename T, typename IT = int>
class ScalarScatterTest : public ScatterTestBase<T, IT> {
public:
  ScalarScatterTest(TestFramework &TF) : ScatterTestBase<T, IT>(TF) {}
  void run(unsigned) override {
    ref_scatter(N, TestBase<T, IT>::Indexes->s, TestBase<T, IT>::Dense->s,
                TestBase<T, IT>::SparseLen, TestBase<T, IT>::Sparse->s);
  }
};

// HVX intrinsics and their tests.
void hvx_gather_intr(HVX_Vector &Output, const int16_t *Input,
                     HVX_Vector Indexes, size_t Size) {
  Q6_vgather_ARMVh(&Output, (uintptr_t)Input, Size - 1,
                   Q6_Vh_vasl_VhR(Indexes, 1));
}

void hvx_scatter_intr(int16_t *Output, HVX_Vector Input, HVX_Vector Indexes,
                      size_t Size) {
  Q6_vscatter_RMVhV((uintptr_t)Output, Size - 1, Q6_Vh_vasl_VhR(Indexes, 1),
                    Input);
}

void hvx_gather_intr(HVX_Vector &Output, const int32_t *Input,
                     HVX_Vector Indexes, size_t Size) {
  Q6_vgather_ARMVw(&Output, (uintptr_t)Input, Size - 1,
                   Q6_Vw_vasl_VwR(Indexes, 2));
}

void hvx_scatter_intr(int32_t *Output, HVX_Vector Input, HVX_Vector Indexes,
                      size_t Size) {
  Q6_vscatter_RMVwV((uintptr_t)Output, Size - 1, Q6_Vw_vasl_VwR(Indexes, 2),
                    Input);
}

template <typename T> class VectorGatherTest : public GatherTestBase<T, T> {
public:
  VectorGatherTest(TestFramework &TF) : GatherTestBase<T, T>(TF) {}
  void run(unsigned) override {
    assert(N * sizeof(T) % sizeof(HVX_Vector) == 0);
    for (size_t i = 0; i != N * sizeof(T) / sizeof(HVX_Vector); ++i)
      hvx_gather_intr(TestBase<T, T>::Dense->v[i], TestBase<T, T>::Sparse->s,
                      TestBase<T, T>::Indexes->v[i],
                      TestBase<T, T>::SparseLen * sizeof(T));
    GatherTestBase<T, T>::SyncOutput();
  }
};

template <typename T> class VectorScatterTest : public ScatterTestBase<T, T> {
public:
  VectorScatterTest(TestFramework &TF) : ScatterTestBase<T, T>(TF) {}
  void run(unsigned) override {
    assert(N * sizeof(T) % sizeof(HVX_Vector) == 0);
    for (size_t i = 0; i != N * sizeof(T) / sizeof(HVX_Vector); ++i)
      hvx_scatter_intr(TestBase<T, T>::Sparse->s, TestBase<T, T>::Dense->v[i],
                       TestBase<T, T>::Indexes->v[i],
                       TestBase<T, T>::SparseLen * sizeof(T));
    ScatterTestBase<T, T>::SyncOutput();
  }
};

// Ripple HVX functions tests.
template <typename T, typename IT = int>
class RippleGatherTest : public GatherTestBase<T, IT> {
public:
  RippleGatherTest(TestFramework &TF) : GatherTestBase<T, IT>(TF) {}
  void run(unsigned) override {
    Ripple_gather(N, TestBase<T, IT>::Indexes->s, TestBase<T, IT>::Dense->s,
                  TestBase<T, IT>::SparseLen, TestBase<T, IT>::Sparse->s);
    GatherTestBase<T, IT>::SyncOutput();
  }
};

template <typename T, typename IT = int>
class RippleScatterTest : public ScatterTestBase<T, IT> {
public:
  RippleScatterTest(TestFramework &TF) : ScatterTestBase<T, IT>(TF) {}
  void run(unsigned) override {
    Ripple_scatter(N, TestBase<T, IT>::Indexes->s, TestBase<T, IT>::Dense->s,
                   TestBase<T, IT>::SparseLen, TestBase<T, IT>::Sparse->s);
    ScatterTestBase<T, IT>::SyncOutput();
  };
};

// Scalar tests - for reference timings.
DefineTest<ScalarGatherTest<int8_t>> ScalarGatherTest_i8("gather_ref(i8)");
DefineTest<ScalarGatherTest<int16_t>> ScalarGatherTest_i16("gather_ref(i16)");
DefineTest<ScalarGatherTest<int32_t>> ScalarGatherTest_i32("gather_ref(i32)");
DefineTest<ScalarGatherTest<int64_t>> ScalarGatherTest_i64("gather_ref(i64)");

DefineTest<ScalarScatterTest<int8_t>> ScalarScatterTest_i8("scatter_ref(i8)");
DefineTest<ScalarScatterTest<int16_t>>
    ScalarScatterTest_i16("scatter_ref(i16)");
DefineTest<ScalarScatterTest<int32_t>>
    ScalarScatterTest_i32("scatter_ref(i32)");
DefineTest<ScalarScatterTest<int64_t>>
    ScalarScatterTest_i64("scatter_ref(i64)");

// HVX intrinsics are only defined for 16 and 32 bit data, indexes are of the
// same size.
// TODO: HVX intrinsic tests operate on whole vectors.
#if 0
DefineTest<VectorGatherTest<int16_t>> VectorGatherTest_i16("gather_hvx(i16_16)");
DefineTest<VectorGatherTest<int32_t>> VectorGatherTest_i32("gather_hvx(i32)");

DefineTest<VectorScatterTest<int16_t>>
    VectorScatterTest_i16("scatter_hvx(i16_16)");
DefineTest<VectorScatterTest<int32_t>>
    VectorScatterTest_i32("scatter_hvx(i32)");
#endif

// Ripple tests.
DefineTest<RippleGatherTest<int8_t>> RippleGatherTest_i8("gather_ripple(i8)");
DefineTest<RippleGatherTest<int8_t, int16_t>>
    RippleGatherTest_i8_16("gather_ripple(i8_16)");

DefineTest<RippleGatherTest<int16_t>>
    RippleGatherTest_i16("gather_ripple(i16)");
DefineTest<RippleGatherTest<int16_t, int16_t>>
    RippleGatherTest_i16_16("gather_ripple(i16_16)");

DefineTest<RippleGatherTest<int32_t>>
    RippleGatherTest_i32("gather_ripple(i32)");

DefineTest<RippleGatherTest<int64_t>>
    RippleGatherTest_i64("gather_ripple(i64)");

DefineTest<RippleScatterTest<int8_t>>
    RippleScatterTest_i8("scatter_ripple(i8)");
DefineTest<RippleScatterTest<int8_t, int16_t>>
    RippleScatterTest_i8_16("scatter_ripple(i8_16)");

DefineTest<RippleScatterTest<int16_t>>
    RippleScatterTest_i16("scatter_ripple(i16)");
DefineTest<RippleScatterTest<int16_t, int16_t>>
    RippleScatterTest_i16_16("scatter_ripple(i16_16)");

DefineTest<RippleScatterTest<int32_t>>
    RippleScatterTest_i32("scatter_ripple(i32)");

DefineTest<RippleScatterTest<int64_t>>
    RippleScatterTest_i64("scatter_ripple(i64)");
} // namespace
