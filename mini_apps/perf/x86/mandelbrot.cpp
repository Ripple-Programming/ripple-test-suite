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

extern "C" void mandelbrot_ispc(float x0, float y0, float x1, float y1,
                                int width, int height, int maxIterations,
                                int output[]);

namespace {
inline int __attribute__((always_inline))
mandelbrot_inner_ref(const float c_re, const float c_im, const int max_iters) {
  float z_re = c_re, z_im = c_im;
  int i_at_break = max_iters;
  for (int i = 0; i < max_iters; ++i) {
    if (z_re * z_re + z_im * z_im > 4.f) {
      i_at_break = i;
      break;
    }

    // {{{ Compute z^2 + c

    float new_re = z_re * z_re - z_im * z_im;
    float new_im = 2.f * z_re * z_im;
    z_re = c_re + new_re;
    z_im = c_im + new_im;

    // }}}
  }
  return i_at_break;
}

void __attribute__((noinline))
mandelbrot_ref(const float x0, const float y0, const float x1, const float y1,
               const int width, const int height, const int max_iters,
               int *output) {
  float dx = (x1 - x0) / width;
  float dy = (y1 - y0) / height;

  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; ++i) {
      float x = x0 + i * dx;
      float y = y0 + j * dy;

      int index = (j * width + i);
      output[index] = mandelbrot_inner_ref(x, y, max_iters);
    }
  }
}

inline int __attribute__((always_inline))
mandelbrot_inner_ripple(ripple_block_t BS, const float c_re, const float c_im,
                        const int max_iters) {
  float z_re = c_re, z_im = ripple_broadcast(BS, 0b1, c_im);
  int i_at_break = ripple_broadcast(BS, 0b1, 0);
  for (int i = 0; i < max_iters; ++i) {
    int8_t is_not_diverging = ((z_re * z_re + z_im * z_im) <= 4.f);

    if (!ripple_reduceor(0b1, is_not_diverging))
      break;

    if (is_not_diverging) // { uncomment this brace once QTOOL-136834 fixed
      i_at_break += 1;

    float new_re = z_re * z_re - z_im * z_im;
    float new_im = 2.f * z_re * z_im;
    z_re = c_re + new_re;
    z_im = c_im + new_im;

    // } uncomment this brace once QTOOL-136834 fixed
  }
  return i_at_break;
}

void __attribute__((noinline))
mandelbrot_ripple(const float x0, const float y0, const float x1,
                  const float y1, const int width, const int height,
                  const int max_iters, int *output) {
  ripple_block_t BS = ripple_set_block_shape(0, 8);
  float dx = (x1 - x0) / width;
  float dy = (y1 - y0) / height;

  for (int j = 0; j < height; j++) {
    ripple_parallel_full(BS, 0);
    for (int i = 0; i < width; ++i) {
      float x = x0 + i * dx;
      float y = y0 + j * dy;

      int index = (j * width + i);
      output[index] = mandelbrot_inner_ripple(BS, x, y, max_iters);
    }
  }
}

} // namespace

namespace ripple_test_suite {

enum KernelT {
  Reference,
  RippleOpt,
  ISPC,
};

template <KernelT KT> class MandelBrotTest : public Test {
  static constexpr int max_iters = 256;
  static constexpr int width = 768;
  static constexpr int height = 128;
  static constexpr float x0 = -2;
  static constexpr float x1 = 1;
  static constexpr float y0 = -1;
  static constexpr float y1 = 1;

  int Y[width * height], YRef[width * height];

public:
  MandelBrotTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (unsigned i = 0; i != width * height; ++i)
      Y[i] = YRef[i] = 0;

    mandelbrot_ref(x0, y0, x1, y1, width, height, max_iters, YRef);
  }

  void run(unsigned) override {
    if (KT == KernelT::Reference)
      mandelbrot_ref(x0, y0, x1, y1, width, height, max_iters, Y);
    if (KT == KernelT::RippleOpt)
      mandelbrot_ripple(x0, y0, x1, y1, width, height, max_iters, Y);
    if (KT == KernelT::ISPC)
      mandelbrot_ispc(x0, y0, x1, y1, width, height, max_iters, Y);
  }

  bool verify() const override {

    if (0) {
      for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; ++i) {
          int index = (j * width + i);
          if (Y[index] < max_iters) {
            printf(" ");
          } else {
            printf("X");
          }
        }
        printf("\n");
      }
    }

    return equal(1e-5, Y, YRef);
  }
};

DefineTest<MandelBrotTest<Reference>>
    MandelBrotTestInstance_0("mandelbrot.ref");
DefineTest<MandelBrotTest<ISPC>> MandelBrotTestInstance_1("mandelbrot.ispc");
DefineTest<MandelBrotTest<RippleOpt>>
    MandelBrotTestInstance_2("mandelbrot.ripple");

} // namespace ripple_test_suite
