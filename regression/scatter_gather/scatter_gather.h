//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdlib>

template <typename T, typename IT>
void Ripple_gather(size_t N, const IT *indexes, T *destination,
                   size_t source_len, const T *source);

template <typename T, typename IT>
void Ripple_scatter(size_t N, const IT *indexes, const T *source,
                    size_t destination_len, T *destination);
