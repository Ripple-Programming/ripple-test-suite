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

/**
 * @brief Ripple-kernel to apply a dense layer of neurons, i.e. computes the
 tensor expression: `Y[i, j] <- sum(k, X[i, k] * W[j, k]) + B[j]`.
 *
 * @tparam Nj Number of neurons in the layer.
 * @tparam Nk Number of input features per datapoint.
 * @tparam Ti Tile-length across the "i"-th dimension in the tensor expression.
 * @tparam Tj Tile-length across the "j"-th dimension in the tensor expression.
 * @tparam Tk Tile-length across the "k"-th dimension in the tensor expression.
 * @param Ni Batch size.
 * @param X Inputs.
 * @param W Weights
 * @param B Biases.
 * @param Y Outputs.
 */
template <unsigned Nj, unsigned Nk, unsigned Ti, unsigned Tj, unsigned Tk>
void dense_layer_variant_1(unsigned Ni, const float (*X)[Nk],
                           const float W[Nj][Nk], const float B[Nj],
                           float (*Y)[Nj]) {
  static_assert(Nj % Tj == 0, "Imperfect tile lengths not supported.");
  static_assert(Nk % Tk == 0, "Imperfect tile lengths not supported.");
  static_assert(Tj >= Tk, "Transform implementation requirement.");
  static_assert(Tj >= Ti, "Transform implementation requirement.");
  static_assert(Tj < 2 * Ti, "Transform implementation requirement.");
  auto BS = ripple_set_block_shape(VEC, Tj, Ti);
  size_t v0 = ripple_id(BS, 0), v1 = ripple_id(BS, 1);

  float X_tile[Ti][Tk], W_tile[Tj][Tk], B_tile[Tj];
  for (int j = 0; j < Nj; j += Tj) {
    if (v1 == 0)
      B_tile[v0] = B[j + v0];
    int i = 0;
    for (i = 0; i + Ti <= Ni; i += Ti) {
      float acc = 0.;
      for (int k = 0; k < Nk; k += Tk) {
        // Prefetch X[i:i+Ti][k:k+Tk]
        if (v0 < Tk)
          X_tile[v1][v0] = X[i + v1][k + v0];
        // Prefetch W[j:j+Tj][k:k+Tk]
        if (v0 < Tk) {
          W_tile[v1][v0] = W[j + v1][k + v0];
          if (v1 + Ti < Tj)
            W_tile[v1 + Ti][v0] = W[j + Ti + v1][k + v0];
        }
        for (int k_inner = 0; k_inner < Tk; k_inner++)
          acc += X_tile[v1][k_inner] * W_tile[v0][k_inner];
      }
      acc += B_tile[v0];
      Y[i + v1][j + v0] = acc;
    }
    if (i < Ni) {
      float acc = 0.;
      for (int k = 0; k < Nk; k += Tk) {
        // Prefetch X[i:i+Ti][k:k+Tk]
        if (v0 < Tk && (i + v1) < Ni)
          X_tile[v1][v0] = X[i + v1][k + v0];
        // Prefetch W[j:j+Tj][k:k+Tk]
        if (v0 < Tk) {
          W_tile[v1][v0] = W[j + v1][k + v0];
          if (v1 + Ti < Tj)
            W_tile[v1 + Ti][v0] = W[j + Ti + v1][k + v0];
        }
        for (int k_inner = 0; k_inner < Tk; k_inner++)
          acc += X_tile[v1][k_inner] * W_tile[v0][k_inner];
      }
      acc += B_tile[v0];
      Y[i + v1][j + v0] = acc;
    }
  }
}

/// @brief Reference computation for applying a dense layer of neurons
/// to "X", with weights "W" and biases "B", i.e. computes the
/// tensor expression: `Y[i, j] <- sum(k, X[i, k] * W[j, k]) + B[j]`.
/// @tparam Nj Number of neurons.
/// @tparam Nk Number of input features per batch.
/// @param Ni Batch size.
/// @param W Weights stored row-wise, i.e. "i"-th row contains the weights for
/// "i"-th neuron.
/// @param B Biases of the neurons.
/// @param Y The output activations.
template <unsigned Nj, unsigned Nk>
void dense_layer_ref(unsigned Ni, const float (*X)[Nk], const float W[Nj][Nk],
                     const float B[Nj], float (*Y)[Nj]) {
  for (int i = 0; i < Ni; i++) {
    for (int j = 0; j < Nj; j++) {
      float acc = 0.;
      for (int k = 0; k < Nk; k++) {
        acc += X[i][k] * W[j][k];
      }
      Y[i][j] = acc + B[j];
    }
  }
}

using namespace ripple_test_suite;

template <unsigned Nj, unsigned Nk, unsigned Ti, unsigned Tj, unsigned Tk>
class DenseLayerTest : public Test {
  static constexpr int N = 80; // TODO: Choose a prime-number.
  float X[N][Nk], W[Nj][Nk], B[Nj], Y[N][Nj], YRef[N][Nj];

public:
  DenseLayerTest(TestFramework &TestFramework) : Test(TestFramework) {
    for (int j = 0; j < Nk; ++j) {
      for (int i = 0; i < N; i++)
        X[i][j] = 16 * randn();
      for (int i = 0; i < Nj; i++)
        W[i][j] = 16 * randn();
    }
    for (int i = 0; i < Nj; i++)
      B[i] = 16 * randn();
    dense_layer_ref<Nj, Nk>(N, X, W, B, YRef);
  }

  void run(unsigned) override {
    dense_layer_variant_1<Nj, Nk, Ti, Tj, Tk>(N, X, W, B, Y);
  }

#ifdef __hexagon__
  std::pair<bool, std::string> xfail() const override {
    return {true, "See QSTREAM-1655."};
  }
#endif

  bool verify() const override { return equal(2e-5, Y, YRef); }
};

DefineTest<DenseLayerTest<10, 300, 8, 10, 10>>
    T_10_300("dense_layer[10][300],tile=10");
DefineTest<DenseLayerTest<300, 784, 8, 10, 8>>
    T_300_784("dense_layer[300][784],tile=8");

} // namespace
