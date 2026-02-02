//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cstddef>
#include <ripple.h>
#include <stdint.h>
#include <vector>

#include <ripple-test-suite/ripple-test-suite.h>

namespace {

extern "C" void loop_stencil_ispc(int t0, int t1, int x0, int x1, int y0,
                                  int y1, int z0, int z1, int Nx, int Ny,
                                  int Nz, const float coef[4],
                                  const float vsq[], float Aeven[],
                                  float Aodd[]);

#define _Ain3D(i2, i1, i0) Ain[Nx * Ny * i2 + Nx * i1 + i0]
#define Ain3D(i2, i1, i0) (_Ain3D((i2), (i1), (i0)))
#define _Aout3D(i2, i1, i0) Aout[Nx * Ny * i2 + Nx * i1 + i0]
#define Aout3D(i2, i1, i0) (_Aout3D((i2), (i1), (i0)))
#define _vsq3D(i2, i1, i0) vsq[Nx * Ny * i2 + Nx * i1 + i0]
#define vsq3D(i2, i1, i0) (_vsq3D((i2), (i1), (i0)))

/**
 * @brief Computes the recurrence formula \f$u^{t+1} \gets 2u^t -u^{t-1} + v^2
 * (\partial_{xx} u^t)$. This is the recurrence equation for solving the wave
 * equation \f$u_{tt} = c^2 u_{xx}$. This routine implements a third order FD
 * method for computing \f$u_{xx}$
 *
 * @param x0
 * @param x1
 * @param y0
 * @param y1
 * @param z0
 * @param z1
 * @param Nx
 * @param Ny
 * @param Nz
 * @param coefs
 */
void wave_3d_third_order_single_timestep_ref(size_t x0, size_t x1, size_t y0,
                                             size_t y1, size_t z0, size_t z1,
                                             size_t Nx, size_t Ny, size_t Nz,
                                             const float *__restrict coeffs,
                                             const float *__restrict vsq,
                                             const float *__restrict Ain,
                                             float *__restrict Aout) {
  const float coef0 = coeffs[0], coef1 = coeffs[1], coef2 = coeffs[2],
              coef3 = coeffs[3];
  for (size_t z = z0; z != z1; z++) {
    for (size_t y = y0; y != y1; y++) {
      for (size_t x = x0; x != x1; x++) {
        float div = coef0 * Ain3D(z, y, x) +
                    coef1 * (Ain3D(z - 1, y, x) + Ain3D(z + 1, y, x) +
                             Ain3D(z, y - 1, x) + Ain3D(z, y + 1, x) +
                             Ain3D(z, y, x - 1) + Ain3D(z, y, x + 1)) +
                    coef2 * (Ain3D(z - 2, y, x) + Ain3D(z + 2, y, x) +
                             Ain3D(z, y - 2, x) + Ain3D(z, y + 2, x) +
                             Ain3D(z, y, x - 2) + Ain3D(z, y, x + 2)) +
                    coef3 * (Ain3D(z - 3, y, x) + Ain3D(z + 3, y, x) +
                             Ain3D(z, y - 3, x) + Ain3D(z, y + 3, x) +
                             Ain3D(z, y, x - 3) + Ain3D(z, y, x + 3));
        Aout3D(z, y, x) =
            2 * Ain3D(z, y, x) - Aout3D(z, y, x) + vsq3D(z, y, x) * div;
      }
    }
  }
}

/**
 * @brief Performs \p wave_3d_third_order_single_timestep_ref \p T times.
 *
 * @param[in] Aeven Input start evolving the function as per wave-equation.
 * @param[in] Aodd Buffer with the same size as Aeven needed for the
 * procedure's working.
 *
 * @note The If \p T is odd, output is populated in Aodd and Aeven has the
 * function values at (T-1). Similarly, if \p T is even, output is populated
 * in Aeven and the penultimate values will be in Aodd.
 */
void __attribute__((noinline))
wave_3d_third_order_ref(size_t T, size_t x0, size_t x1, size_t y0, size_t y1,
                        size_t z0, size_t z1, size_t Nx, size_t Ny, size_t Nz,
                        const float *__restrict coeffs,
                        const float *__restrict vsq, float *__restrict Aeven,
                        float *__restrict Aodd) {
  for (size_t t = 0; t != T; ++t) {
    if ((t % 2) == 0)
      wave_3d_third_order_single_timestep_ref(x0, x1, y0, y1, z0, z1, Nx, Ny,
                                              Nz, coeffs, vsq, Aeven, Aodd);

    else
      wave_3d_third_order_single_timestep_ref(x0, x1, y0, y1, z0, z1, Nx, Ny,
                                              Nz, coeffs, vsq, Aodd, Aeven);
  }
}

/**
 * @brief Ripple implementation for wave_3d_third_order_single_timestep_ref.
 */
void wave_3d_third_order_single_timestep_ripple(size_t x0, size_t x1, size_t y0,
                                                size_t y1, size_t z0, size_t z1,
                                                size_t Nx, size_t Ny, size_t Nz,
                                                const float *__restrict coeffs,
                                                const float *__restrict vsq,
                                                const float *__restrict Ain,
                                                float *__restrict Aout) {

  constexpr size_t nv0 = 8;
  ripple_block_t BS = ripple_set_block_shape(0, nv0);
  size_t v0 = ripple_id(BS, 0);

  const float coef0 = coeffs[0], coef1 = coeffs[1], coef2 = coeffs[2],
              coef3 = coeffs[3];
  for (size_t z = z0; z != z1; z++) {
    for (size_t y = y0; y != y1; y++) {
      size_t x;
      for (x = x0; x + nv0 <= x1; x += nv0) {
        float u_at_x_minus3 = Ain3D(z, y, x + v0 - 3);
        float u_at_x_plus5 = Ain3D(z, y, x + v0 + 5);
        float u_at_x_minus2 =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 1; });
        float u_at_x_minus1 =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 2; });
        float u_at_x =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 3; });
        float u_at_x_plus1 =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 4; });
        float u_at_x_plus2 =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 5; });
        float u_at_x_plus3 =
            ripple_shuffle_pair(u_at_x_minus3, u_at_x_plus5,
                                [](size_t i, size_t n) { return i + 6; });
        float div = coef0 * u_at_x +
                    coef1 * (Ain3D(z - 1, y, x + v0) + Ain3D(z + 1, y, x + v0) +
                             Ain3D(z, y - 1, x + v0) + Ain3D(z, y + 1, x + v0) +
                             u_at_x_minus1 + u_at_x_plus1) +
                    coef2 * (Ain3D(z - 2, y, x + v0) + Ain3D(z + 2, y, x + v0) +
                             Ain3D(z, y - 2, x + v0) + Ain3D(z, y + 2, x + v0) +
                             u_at_x_minus2 + u_at_x_plus2) +
                    coef3 * (Ain3D(z - 3, y, x + v0) + Ain3D(z + 3, y, x + v0) +
                             Ain3D(z, y - 3, x + v0) + Ain3D(z, y + 3, x + v0) +
                             u_at_x_minus3 + u_at_x_plus3);
        Aout3D(z, y, x + v0) =
            2 * u_at_x - Aout3D(z, y, x + v0) + vsq3D(z, y, x + v0) * div;
      }
      if (x + v0 < x1) {
        float div =
            coef0 * Ain3D(z, y, x + v0) +
            coef1 * (Ain3D(z - 1, y, x + v0) + Ain3D(z + 1, y, x + v0) +
                     Ain3D(z, y - 1, x + v0) + Ain3D(z, y + 1, x + v0) +
                     Ain3D(z, y, x + v0 - 1) + Ain3D(z, y, x + v0 + 1)) +
            coef2 * (Ain3D(z - 2, y, x + v0) + Ain3D(z + 2, y, x + v0) +
                     Ain3D(z, y - 2, x + v0) + Ain3D(z, y + 2, x + v0) +
                     Ain3D(z, y, x + v0 - 2) + Ain3D(z, y, x + v0 + 2)) +
            coef3 * (Ain3D(z - 3, y, x + v0) + Ain3D(z + 3, y, x + v0) +
                     Ain3D(z, y - 3, x + v0) + Ain3D(z, y + 3, x + v0) +
                     Ain3D(z, y, x + v0 - 3) + Ain3D(z, y, x + v0 + 3));
        Aout3D(z, y, x + v0) = 2 * Ain3D(z, y, x + v0) - Aout3D(z, y, x + v0) +
                               vsq3D(z, y, x + v0) * div;
      }
    }
  }
}

/**
 * @brief Ripple implementation for wave_3d_third_order_ref.
 */
void __attribute__((noinline))
wave_3d_third_order_ripple(size_t T, size_t x0, size_t x1, size_t y0, size_t y1,
                           size_t z0, size_t z1, size_t Nx, size_t Ny,
                           size_t Nz, const float *__restrict coeffs,
                           const float *__restrict vsq, float *__restrict Aeven,
                           float *__restrict Aodd) {
  for (size_t t = 0; t != T; ++t) {
    if ((t % 2) == 0)
      wave_3d_third_order_single_timestep_ripple(x0, x1, y0, y1, z0, z1, Nx, Ny,
                                                 Nz, coeffs, vsq, Aeven, Aodd);

    else
      wave_3d_third_order_single_timestep_ripple(x0, x1, y0, y1, z0, z1, Nx, Ny,
                                                 Nz, coeffs, vsq, Aodd, Aeven);
  }
}

} // namespace

namespace ripple_test_suite {

enum KernelT { Reference, RippleOpt, ISPC };

template <size_t Tfinal, bool verifyResults, KernelT KT>
class StencilTest : public Test {
  static constexpr size_t N = 512;
  std::vector<float> u, uRef, uScratch, uScratchRef, vsq;
  float coeffs[4];

public:
  StencilTest(TestFramework &TestFramework) : Test(TestFramework) {
    u.resize(N * N * N);
    uScratch.resize(N * N * N);
    uRef.resize(N * N * N);
    uScratchRef.resize(N * N * N);
    vsq.resize(N * N * N);

    coeffs[0] = .5;
    coeffs[1] = -.25;
    coeffs[2] = .125;
    coeffs[3] = -.0625;

    for (size_t z = 0; z != N; ++z)
      for (size_t y = 0; y != N; ++y)
        for (size_t x = 0; x != N; ++x) {
          float val = (x < N / 2) ? x / float(N) : y / float(N);
          u[N * N * z + N * y + x] = val;
          uRef[N * N * z + N * y + x] = val;
          uScratchRef[N * N * z + N * y + x] = 0;
          uScratch[N * N * z + N * y + x] = 0;
          vsq[N * N * z + N * y + x] =
              (z / float(N)) * (y / float(N)) * (x / float(N));
        }

    wave_3d_third_order_ref(Tfinal, 3, N - 3, 3, N - 3, 3, N - 3, N, N, N,
                            coeffs, vsq.data(), uRef.data(),
                            uScratchRef.data());
  }

  void run(unsigned) override {
    if (KT == KernelT::Reference)
      wave_3d_third_order_ref(Tfinal, 3, N - 3, 3, N - 3, 3, N - 3, N, N, N,
                              coeffs, vsq.data(), u.data(), uScratch.data());
    if (KT == KernelT::RippleOpt)
      wave_3d_third_order_ripple(Tfinal, 3, N - 3, 3, N - 3, 3, N - 3, N, N, N,
                                 coeffs, vsq.data(), u.data(), uScratch.data());
    if (KT == KernelT::ISPC)
      loop_stencil_ispc(0, Tfinal, 3, N - 3, 3, N - 3, 3, N - 3, N, N, N,
                        coeffs, vsq.data(), u.data(), uScratch.data());
  }

  bool verify() const override {
    return !verifyResults ||
           (isclose(N * N * N, u.data(), uRef.data(), 1e-3, 1e-6) &&
            isclose(N * N * N, uScratch.data(), uScratchRef.data(), 1e-3,
                    1e-6));
  }
};

DefineTest<StencilTest<1, true, Reference>> StencilTest_0("stencil.ref.verify");
DefineTest<StencilTest<6, false, Reference>> StencilTest_1("stencil.ref.perf");
DefineTest<StencilTest<1, true, ISPC>> StencilTest_2("stencil.ispc.verify");
DefineTest<StencilTest<6, false, ISPC>> StencilTest_3("stencil.ispc.perf");
DefineTest<StencilTest<1, true, RippleOpt>>
    StencilTest_4("stencil.ripple.verify");
DefineTest<StencilTest<6, false, RippleOpt>>
    StencilTest_5("stencil.ripple.perf");

} // namespace ripple_test_suite
