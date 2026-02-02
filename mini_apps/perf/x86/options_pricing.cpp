//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple.h>
#include <ripple_math.h>

#include <ripple-test-suite/ripple-test-suite.h>

extern "C" void black_scholes_ispc(float Sa[], float Xa[], float Ta[],
                                   float ra[], float va[], float result[],
                                   int count);

extern "C" void binomial_put_ispc(float Sa[], float Xa[], float Ta[],
                                  float ra[], float va[], float result[],
                                  int count);

namespace {

/**
 * @brief Approximation for the Cumulative normal distribution.
 */
inline float __attribute__((always_inline)) CND(float X) {
  const float invSqrt2Pi = 0.39894228040f;
  float L = fabsf(X);
  float K1, K2, K3, K4, K5;
  // Set up 4-floats so that Kx = K, Ky = K^2, Kz = K^3, Kw = K^4
  K1 = 1.0f / (1.0f + 0.2316419f * L);
  K2 = K1 * K1;
  K3 = K1 * K2;
  K4 = K2 * K2;
  K5 = K3 * K2;
  // compute K, K^2, K^3, and K^4 terms, reordered for efficient
  // vectorization. Above, we precomputed the K powers; here we'll
  // multiply each one by its corresponding scale and sum up the
  // terms efficiently with the dot() routine.
  float w = 0.31938153f * K1 - 0.356563782f * K2 + 1.781477937f * K3 +
            -1.821255978f * K4 + 1.330274429f * K5;
  w *= invSqrt2Pi * expf(-L * L * .5f);
  if (X > 0)
    w = 1.0 - w;
  return w;
}

inline float __attribute__((always_inline))
_black_scholes_call_val(float S, float X, float T, float r, float v) {
  // Copied from "Pharr, Matt, and Randima Fernando. GPU Gems 2: Programming
  // techniques for high-performance graphics and general-purpose computation
  // (gpu gems). Addison-Wesley Professional, 2005."
  float sqrtT = sqrtf(T);
  float d1 = (logf(S / X) + (r + v * v * .5f) * T) / (v * sqrtT);
  float d2 = d1 - v * sqrtT;
  return S * CND(d1) - X * expf(-r * T) * CND(d2);
}

/**
 * @brief Compute Black–Scholes European call option prices for a batch of
 * inputs (reference/serial implementation).
 *
 * @param[in]  N   Number of options to price (length of all input/output
 * arrays).
 * @param[in]  S   Pointer to array of spot/underlying prices S_i (size N). Each
 * S_i > 0.
 * @param[in]  X   Pointer to array of strike prices X_i (size N). Each X_i > 0.
 * @param[in]  T   Pointer to array of times to expiration T_i in years (size
 * N). Each T_i >= 0.
 * @param[in]  r   Pointer to array of risk-free interest rates r_i (annual,
 * decimal; size N).
 * @param[in]  v   Pointer to array of volatilities v_i (annual, decimal; size
 * N). Each v_i >= 0.
 * @param[out] cost Pointer to array where computed call prices C_i will be
 * written (size N).
 */
void black_scholes_ref(const int N, const float *S, const float *X,
                       const float *T, const float *r, const float *v,
                       float *cost) {
  for (int i = 0; i < N; ++i) {
    cost[i] = _black_scholes_call_val(S[i], X[i], T[i], r[i], v[i]);
  }
}

/**
 * @brief Compute Black–Scholes European call option prices for a batch of
 * inputs (ripple implementation).
 *
 * @param[in]  N   Number of options to price (length of all input/output
 * arrays).
 * @param[in]  S   Pointer to array of spot/underlying prices S_i (size N). Each
 * S_i > 0.
 * @param[in]  X   Pointer to array of strike prices X_i (size N). Each X_i > 0.
 * @param[in]  T   Pointer to array of times to expiration T_i in years (size
 * N). Each T_i >= 0.
 * @param[in]  r   Pointer to array of risk-free interest rates r_i (annual,
 * decimal; size N).
 * @param[in]  v   Pointer to array of volatilities v_i (annual, decimal; size
 * N). Each v_i >= 0.
 * @param[out] cost Pointer to array where computed call prices C_i will be
 * written (size N).
 */
void black_scholes_ripple(const int N, const float *S, const float *X,
                          const float *T, const float *r, const float *v,
                          float *cost) {
  ripple_block_t BS = ripple_set_block_shape(0, 8);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; ++i) {
    cost[i] = _black_scholes_call_val(S[i], X[i], T[i], r[i], v[i]);
  }
}

/**
 * @tparam  H   Height of the binomial tree.
 */
template <unsigned H>
inline float __attribute__((always_inline))
_binomial_put_val_ref(float S, float X, float T, float r, float v) {
  float V[H];

  float dt = T / H;
  float u = expf(v * sqrtf(dt));
  float d = 1.f / u;
  float disc = expf(r * dt);
  float Pu = (disc - d) / (u - d);

  for (int j = 0; j < H; ++j) {
    float upow = powf(u, (2.f * j - H));
    V[j] = std::max(0.f, X - S * upow);
  }

  for (int j = H - 1; j >= 0; --j)
    for (int k = 0; k < j; ++k)
      V[k] = ((1 - Pu) * V[k] + Pu * V[k + 1]) / disc;

  return V[0];
}

/**
 * @tparam  H   Height of the binomial tree.
 */
template <unsigned H, unsigned nv0>
inline float __attribute__((always_inline))
_binomial_put_val_ripple(ripple_block_t BS, float S, float X, float T, float r,
                         float v) {

  size_t v0 = ripple_id(BS, 0);
  float V[H * nv0];

  float dt = T / H;
  float u = expf(v * sqrtf(dt));
  float d = 1.f / u;
  float disc = expf(r * dt);
  float Pu = (disc - d) / (u - d);

  for (int j = 0; j < H; ++j) {
    float upow = powf(u, (2.f * j - H));
    V[nv0 * j + v0] = std::max(0.f, X - S * upow);
  }

  for (int j = H - 1; j >= 0; --j)
    for (int k = 0; k < j; ++k)
      V[nv0 * k + v0] =
          ((1 - Pu) * V[nv0 * k + v0] + Pu * V[nv0 * (k + 1) + v0]) / disc;

  return V[0 * nv0 + v0];
}

/**
 * @brief Compute Binomial put call option prices for a batch of inputs
 * (reference implementation).
 *
 * @param[in]  N   Number of options to price (length of all input/output
 * arrays).
 * @param[in]  S   Pointer to array of spot/underlying prices S_i (size N). Each
 * S_i > 0.
 * @param[in]  X   Pointer to array of strike prices X_i (size N). Each X_i > 0.
 * @param[in]  T   Pointer to array of times to expiration T_i in years (size
 * N). Each T_i >= 0.
 * @param[in]  r   Pointer to array of risk-free interest rates r_i (annual,
 * decimal; size N).
 * @param[in]  v   Pointer to array of volatilities v_i (annual, decimal; size
 * N). Each v_i >= 0.
 * @param[out] cost Pointer to array where computed put prices C_i will be
 * written (size N).
 */
void binomial_put_ref(const int N, const float *S, const float *X,
                      const float *T, const float *r, const float *v,
                      float *cost) {
  for (int i = 0; i < N; ++i) {
    cost[i] = _binomial_put_val_ref<64>(S[i], X[i], T[i], r[i], v[i]);
  }
}

/**
 * @brief Compute Binomial put call option prices for a batch of inputs (ripple
 * implementation).
 *
 * @param[in]  N   Number of options to price (length of all input/output
 * arrays).
 * @param[in]  S   Pointer to array of spot/underlying prices S_i (size N). Each
 * S_i > 0.
 * @param[in]  X   Pointer to array of strike prices X_i (size N). Each X_i > 0.
 * @param[in]  T   Pointer to array of times to expiration T_i in years (size
 * N). Each T_i >= 0.
 * @param[in]  r   Pointer to array of risk-free interest rates r_i (annual,
 * decimal; size N).
 * @param[in]  v   Pointer to array of volatilities v_i (annual, decimal; size
 * N). Each v_i >= 0.
 * @param[out] cost Pointer to array where computed put prices C_i will be
 * written (size N).
 */
void binomial_put_ripple(const int N, const float *S, const float *X,
                         const float *T, const float *r, const float *v,
                         float *cost) {
  constexpr unsigned nv0 = 8;
  ripple_block_t BS = ripple_set_block_shape(0, nv0);
  ripple_parallel(BS, 0);
  for (int i = 0; i < N; ++i) {
    cost[i] =
        _binomial_put_val_ripple<64, nv0>(BS, S[i], X[i], T[i], r[i], v[i]);
  }
}

} // namespace

namespace ripple_test_suite {

enum PricingModelT {
  BlackScholesCall,
  BinomialPut,
};

enum KernelT { Reference, RippleOpt, ISPC };

template <PricingModelT MT, KernelT KT> class OptionsPricingTest : public Test {
  static constexpr int N = 128 * 1024;
  float S[N], X[N], T[N], r[N], v[N], costRef[N], cost[N];

public:
  OptionsPricingTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int i = 0; i < N; ++i) {
      S[i] = 100 + 5 * rand();   // stock price
      X[i] = 98 + 5 * rand();    // option strike price
      T[i] = 2;                  // time (years)
      r[i] = .02 + .04 * rand(); // risk-free interest rate
      v[i] = 4 + 2 * rand();     // volatility
      cost[i] = 0.f;
      costRef[i] = 0.f;
    }
    if (MT == BlackScholesCall)
      black_scholes_ref(N, S, X, T, r, v, costRef);
    if (MT == BinomialPut)
      binomial_put_ref(N, S, X, T, r, v, costRef);
  }

  void run(unsigned) override {
    if (MT == BlackScholesCall && KT == KernelT::Reference)
      black_scholes_ref(N, S, X, T, r, v, cost);
    if (MT == BlackScholesCall && KT == KernelT::RippleOpt)
      black_scholes_ripple(N, S, X, T, r, v, cost);
    if (MT == BlackScholesCall && KT == KernelT::ISPC)
      black_scholes_ispc(S, X, T, r, v, cost, N);
    if (MT == BinomialPut && KT == KernelT::Reference)
      binomial_put_ref(N, S, X, T, r, v, cost);
    if (MT == BinomialPut && KT == KernelT::RippleOpt)
      binomial_put_ripple(N, S, X, T, r, v, cost);
    if (MT == BinomialPut && KT == KernelT::ISPC)
      binomial_put_ispc(S, X, T, r, v, cost, N);
  }

  bool verify() const override { return isclose(N, cost, costRef, 1e-3, 1e-6); }
};

DefineTest<OptionsPricingTest<BlackScholesCall, Reference>>
    OptionsPricingTestInstance_0("options.blackscholes.ref");
DefineTest<OptionsPricingTest<BlackScholesCall, RippleOpt>>
    OptionsPricingTestInstance_1("options.blackscholes.ripple");
DefineTest<OptionsPricingTest<BinomialPut, Reference>>
    OptionsPricingTestInstance_2("options.binomial.ref");
DefineTest<OptionsPricingTest<BinomialPut, RippleOpt>>
    OptionsPricingTestInstance_3("options.binomial.ripple");
DefineTest<OptionsPricingTest<BlackScholesCall, ISPC>>
    OptionsPricingTestInstance_4("options.blackscholes.ispc");
DefineTest<OptionsPricingTest<BinomialPut, Reference>>
    OptionsPricingTestInstance_5("options.binomial.ispc");

} // namespace ripple_test_suite
