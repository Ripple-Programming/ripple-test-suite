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

#define VEC 0

/** A matmul kernel taken from the manual.
 * Dimensions must be multiples of the block sizes.
 */
template <unsigned Ni, unsigned Nj, unsigned Ti, unsigned Tj>
void AT_dot_B_basic(unsigned Nk, const float (*A)[Ni], const float (*B)[Nj],
                    float C[Ni][Nj]) {
  static_assert(Ni % Ti == 0,
                "Tile length must exactly divide dimension length.");
  static_assert(Nj % Tj == 0,
                "Tile length must exactly divide dimension length.");

  auto BS = ripple_set_block_shape(VEC, Tj, Ti);

  size_t x = ripple_id(BS, 0);
  size_t y = ripple_id(BS, 1);
  for (size_t i = 0; i < Ni; i += Ti) {
    for (size_t j = 0; j < Nj; j += Tj) {
      float acc = 0;
      for (int k = 0; k < Nk; ++k)
        acc += A[k][i + y] * B[k][j + x];
      C[i + y][j + x] = acc;
    }
  }
}

#ifndef __hexagon__
/**
 * @brief Computes the einsum("ki,kj->ij", A, B) and stores in the tensor "C".
 *
 * @tparam Ni Dimension length of a tensor axis indexed with "i".
 * @tparam Nj Dimension length of a tensor axis index with "j"
 * @tparam Ti Tile length of a tensor axis indexed with "i". Ti must exactly
 * divide Ni.
 * @tparam Tj Tile length of a tensor axis index with "j". Tj must exactly
 * divide Nj.
 * @tparam Tk Tile length of a tensor axis index with "k".
 * @param Nk Dimension length of a tensor axis indexed with
 * @param A
 * @param B
 * @param C
 */
template <unsigned Ni, unsigned Nj, unsigned Ti, unsigned Tj, unsigned Tk>
static __attribute__((always_inline)) void
AT_dot_B_perfect_tiled_variant(unsigned Nk, const float (*A)[Ni],
                               const float (*B)[Nj], float (*C)[Nj]) {
  static_assert(Ni % Ti == 0,
                "Tile length must exactly divide dimension length.");
  static_assert(Nj % Tj == 0,
                "Tile length must exactly divide dimension length.");
  static_assert(Ti == Tk, "Transform implementation requirement.");
  static_assert(Tj <= Tk, "Transform implementation requirement.");
  static_assert(Tk < 2 * Tj, "Transform implementation requirement.");

  auto BS = ripple_set_block_shape(VEC, Tj, Ti);

  float A_tile[Tk][Ti], B_tile[Tk][Tj];
  size_t v0 = ripple_id(BS, 0), v1 = ripple_id(BS, 1);

  for (int i = 0; i < Ni; i += Ti) {
    for (int j = 0; j < Nj; j += Tj) {
      int k = 0;
      float acc = 0;
      for (k = 0; k + Tk <= Nk; k += Tk) {
        B_tile[v1][v0] = B[k + v1][j + v0];
        A_tile[v1][v0] = A[k + v1][i + v0];
        if (v0 + Tj < Ti)
          A_tile[v1][v0 + Tj] = A[k + v1][i + v0 + Tj];

        for (int k_inner = 0; k_inner < Tk; k_inner++)
          acc += A_tile[k_inner][v1] * B_tile[k_inner][v0];
      }
      if (k + v1 < Nk) {
        B_tile[v1][v0] = B[k + v1][j + v0];
        A_tile[v1][v0] = A[k + v1][i + v0];
        if (v0 + Tj < Ti)
          A_tile[v1][v0 + Tj] = A[k + v1][i + v0 + Tj];
      }

      for (int k_inner = 0; k_inner + k < Nk; k_inner++)
        acc += A_tile[k_inner][v1] * B_tile[k_inner][v0];

      C[i + v1][j + v0] = acc;
    }
  }
}

extern "C" void AT_dot_B_N_10_300(unsigned N, const float A[N][10],
                                  const float B[N][300], float C[10][300]) {
  AT_dot_B_perfect_tiled_variant<10, 300, 10, 10, 10>(N, A, B, C);
}

extern "C" void AT_dot_B_N_300_784(unsigned N, const float A[N][300],
                                   const float B[N][784], float C[300][784]) {
  AT_dot_B_perfect_tiled_variant<300, 784, 10, 8, 10>(N, A, B, C);
}

void A_dot_B_N_10_300(unsigned N, const float A[N][10], const float B[N][300],
                      float C[10][300]) {
  auto BS = ripple_set_block_shape(VEC, 8, 4);

  size_t v0 = ripple_id(BS, 0), nv0 = ripple_get_block_size(BS, 0),
         v1 = ripple_id(BS, 1), nv1 = ripple_get_block_size(BS, 1), i;

  float A_tile[4][10], B_tile[10][8];

  for (i = 0; i + nv1 <= N; i += nv1) {
    *((&A_tile[0][0]) + 8 * v1 + v0) = *((&A[i][0]) + 8 * v1 + v0);
    if (v1 == 0)
      A_tile[3][2 + v0] = A[i + 3][2 + v0];
    size_t j = 0;
    for (j = 0; j + nv0 <= 300; j += nv0) {
      B_tile[v1][v0] = B[v1][j + v0];
      B_tile[4 + v1][v0] = B[4 + v1][j + v0];
      if (v1 < 2)
        B_tile[8 + v1][v0] = B[8 + v1][j + v0];

      float acc = 0;

      for (int k = 0; k < 10; k++)
        acc += A_tile[v1][k] * B_tile[k][v0];

      C[i + v1][j + v0] = acc;
    }
    if (j + v0 < 300) {
      for (int k = 0; k < 10; k++)
        B_tile[k][v0] = B[k][j + v0];

      float acc = 0;

      for (int k = 0; k < 10; k++)
        acc += A_tile[v1][k] * B_tile[k][v0];

      C[i + v1][j + v0] = acc;
    }
  }
}
#endif

void A_dot_B_N_10_300_ref(unsigned N, const float (*A)[10],
                          const float B[10][300], float (*C)[300]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < 300; j++) {
      float acc = 0.;
      for (int k = 0; k < 10; k++) {
        acc += A[i][k] * B[k][j];
      }
      C[i][j] = acc;
    }
  }
  return;
}

template <unsigned K, unsigned Ni, unsigned Nj>
void AT_dot_B_N_ref(unsigned Nk, const float A[K][Ni], const float B[K][Nj],
                    float C[Ni][Nj]) {
  for (int i = 0; i < Ni; i++) {
    for (int j = 0; j < Nj; j++) {
      float acc = 0.;
      for (int k = 0; k < Nk; k++) {
        acc += A[k][i] * B[k][j];
      }
      C[i][j] = acc;
    }
  }
}

using namespace ripple_test_suite;

// Reference implementation, for measurement only.
class test_AT_dot_B_Base : public Test {
protected:
  static constexpr int N = 80;
  static constexpr int Ni = 64;
  static constexpr int Nj = 64;
  float A[N][Ni], B[N][Nj], CRef[Ni][Nj];

public:
  test_AT_dot_B_Base(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < Ni; j++)
        A[i][j] = -16 * randn();
      for (int j = 0; j < Nj; j++)
        B[i][j] = -16 * randn();
    }
  }

  void run(unsigned) override { AT_dot_B_N_ref<N, Ni, Nj>(N, A, B, CRef); }

  bool verify() const override { return true; }
};

DefineTest<test_AT_dot_B_Base> T_test_AT_dot_B_Base("matmul[ref]");

template <unsigned Ti, unsigned Tj>
class test_AT_dot_B_basic : public test_AT_dot_B_Base {
  float C[Ni][Nj];

public:
  test_AT_dot_B_basic(TestFramework &TestFramework)
      : test_AT_dot_B_Base(TestFramework) {
    AT_dot_B_N_ref<N, Ni, Nj>(N, A, B, CRef);
  }

  void run(unsigned) override { AT_dot_B_basic<Ni, Nj, Ti, Tj>(N, A, B, C); }

  bool verify() const override { return equal(1e-5, C, CRef); }
};

DefineTest<test_AT_dot_B_basic<8, 8>>
    T_test_AT_dot_B_basic_8x8("matmul[basic, block=8x8]");

DefineTest<test_AT_dot_B_basic<32, 32>>
    T_test_AT_dot_B_basic_32x32("matmul[basic, block=32x32]");

DefineTest<test_AT_dot_B_basic<1, 8>>
    T_test_AT_dot_B_basic_1x8("matmul[basic, block=1x8]");

DefineTest<test_AT_dot_B_basic<4, 8>>
    T_test_AT_dot_B_basic_4x8("matmul[basic, block=4x8]");

DefineTest<test_AT_dot_B_basic<8, 1>>
    T_test_AT_dot_B_basic_8x1("matmul[basic, block=8x1]");

DefineTest<test_AT_dot_B_basic<8, 4>>
    T_test_AT_dot_B_basic_8x4("matmul[basic, block=8x4]");

#ifndef __hexagon__
class test_A_dot_B_N_10_300 : public Test {
  static constexpr int N = 80; // TODO: Choose a prime number to dis
  float A[N][10], B[10][300], C[N][300], CRef[N][300];

public:
  test_A_dot_B_N_10_300(TestFramework &TestFramework) : Test(TestFramework) {
    for (int k = 0; k < 10; k++) {
      for (int i = 0; i < N; i++)
        A[i][k] = 16 * randn();
      for (int j = 0; j < 300; j++)
        B[k][j] = 16 * randn();
    }

    A_dot_B_N_10_300_ref(N, A, B, CRef);
  }

  void run(unsigned) override { A_dot_B_N_10_300(N, A, B, C); }

  bool verify() const override { return equal(1e-5, C, CRef); }
};

DefineTest<test_A_dot_B_N_10_300>
    T_A_dot_B_N_10_300("matmul[A_dot_B_N_10_300]");

class test_AT_dot_B_N_10_300 : public Test {
  static constexpr int N = 80; // TODO: Choose a prime-number.
  float A[N][10], B[N][300], C[10][300], CRef[10][300];

public:
  test_AT_dot_B_N_10_300(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < 10; j++)
        A[i][j] = -16 * randn();
      for (int j = 0; j < 300; j++)
        B[i][j] = -16 * randn();
    }

    AT_dot_B_N_ref<80, 10, 300>(N, A, B, CRef);
  }

  void run(unsigned) override { AT_dot_B_N_10_300(N, A, B, C); }

  bool verify() const override { return equal(1e-5, C, CRef); }
};

DefineTest<test_AT_dot_B_N_10_300>
    T_AT_dot_B_N_10_300("matmul[AT_dot_B_N_10_300]");

class test_AT_dot_B_N_300_784 : public Test {
  static constexpr int N = 80; // TODO: Choose a prime-number.
  float A[N][300], B[N][784], C[300][784], CRef[300][784];

public:
  test_AT_dot_B_N_300_784(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < 300; j++)
        A[i][j] = -16 * randn();
      for (int j = 0; j < 784; j++)
        B[i][j] = -16 * randn();
    }

    AT_dot_B_N_ref<80, 300, 784>(N, A, B, CRef);
  }

  void run(unsigned) override { AT_dot_B_N_300_784(N, A, B, C); }

  bool verify() const override { return equal(1e-5, C, CRef); }
};

DefineTest<test_AT_dot_B_N_300_784>
    T_AT_dot_B_N_300_784("matmul[AT_dot_B_N_300_784]");
#endif

} // namespace
