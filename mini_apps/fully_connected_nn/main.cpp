//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include "nanopool.hpp"
#include <cstdint>

extern "C" float categorical_cross_entropy_loss(uint32_t Nimage,
                                                const float Yref[Nimage][10],
                                                const float Ypred[Nimage][10]);
extern "C" void axpby(uint32_t N, float a, const float x[N], float b,
                      const float y[N],
                      float out[N]); // evaluates out[i] = a * x[i] + b* y[i]
extern "C" void dense_layer_300_x_784(uint32_t N, const float I[N][784],
                                      const float W[300][784],
                                      const float B[300], float out[N][300]);
extern "C" void dense_layer_10_x_300(uint32_t N, const float I[N][300],
                                     const float W[10][300], const float B[10],
                                     float out[N][10]);
extern "C" void relu(uint32_t N, const float X[N], float Y[N]);
extern "C" void softmax_10(uint32_t N, const float X[N][10], float Y[N][10]);
extern "C" void sum_along_columns_10(uint32_t N, const float X[N][10],
                                     float Y[10]);
extern "C" void sum_along_columns_300(uint32_t N, const float X[N][300],
                                      float Y[300]);
extern "C" void relu_backward(uint32_t N, const float X[N],
                              const float dReluX[N], float dX[N]);

extern "C" void A_dot_B_10_300(uint32_t N, const float A[N][10],
                               const float B[10][300], float out[N][300]);
extern "C" void AT_dot_B_10_300(uint32_t N, const float A[N][10],
                                const float B[N][300], float out[10][300]);
extern "C" void AT_dot_B_300_784(uint32_t N, const float A[N][300],
                                 const float B[N][784], float out[300][784]);

extern "C" void compute_forward_loss_and_backward(
    Mempool alloc, int Nimage,
    // Inputs ->
    const float layer1_wts[300][784], const float layer1_biases[300],
    const float layer2_wts[10][300], const float layer2_biases[10],
    const float X[Nimage][784], const float Y[Nimage][10],
    // Outputs ->
    float Y_pred[Nimage][10], float *loss, float dlayer1_wts[300][784],
    float dlayer1_biases[300], float dlayer2_wts[10][300],
    float dlayer2_biases[10]) {

  // Allocate
  auto layer1_preoutputs =
      (float(*)[300])alloc.allocate(Nimage * 300 * sizeof(float));
  auto dlayer1_preoutputs =
      (float(*)[300])alloc.allocate(Nimage * 300 * sizeof(float));
  auto layer1_outputs =
      (float(*)[300])alloc.allocate(Nimage * 300 * sizeof(float));
  auto dlayer1_outputs =
      (float(*)[300])alloc.allocate(Nimage * 300 * sizeof(float));
  auto layer2_outputs =
      (float(*)[10])alloc.allocate(Nimage * 10 * sizeof(float));
  auto dlayer2_outputs =
      (float(*)[10])alloc.allocate(Nimage * 10 * sizeof(float));

  // Forward
  dense_layer_300_x_784(Nimage, X, layer1_wts, layer1_biases,
                        layer1_preoutputs);
  relu(Nimage * 300, (float *)layer1_preoutputs, (float *)layer1_outputs);
  dense_layer_10_x_300(Nimage, layer1_outputs, layer2_wts, layer2_biases,
                       layer2_outputs);
  softmax_10(Nimage, layer2_outputs, Y_pred);
  *loss = categorical_cross_entropy_loss(Nimage, Y, Y_pred);

  // Backward
  axpby(Nimage * 10, 1.0 / Nimage, (float *)Y_pred, -1.0 / Nimage, (float *)Y,
        (float *)dlayer2_outputs);
  AT_dot_B_10_300(Nimage, dlayer2_outputs, layer1_outputs, dlayer2_wts);
  sum_along_columns_10(Nimage, dlayer2_outputs, dlayer2_biases);

  A_dot_B_10_300(Nimage, dlayer2_outputs, layer2_wts, dlayer1_outputs);
  relu_backward(Nimage, (float *)layer1_preoutputs, (float *)dlayer1_outputs,
                (float *)dlayer1_preoutputs);
  AT_dot_B_300_784(Nimage, dlayer1_preoutputs, X, dlayer1_wts);
  sum_along_columns_300(Nimage, dlayer1_preoutputs, dlayer1_biases);

  // Free:
  alloc.deallocate((intptr_t)layer1_preoutputs);
  alloc.deallocate((intptr_t)dlayer1_preoutputs);
  alloc.deallocate((intptr_t)layer1_outputs);
  alloc.deallocate((intptr_t)dlayer1_outputs);
  alloc.deallocate((intptr_t)layer2_outputs);
  alloc.deallocate((intptr_t)dlayer2_outputs);
}
