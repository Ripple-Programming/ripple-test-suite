//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstddef>
#include <cstdint>

template <typename SrcType, typename DstType>
void Ripple_narrow_shift(std::size_t Length, DstType *out, const SrcType *odd,
                         const SrcType *even, uint32_t Shift);

template <typename SrcType, typename DstType>
void Ripple_narrow_shift_sat(std::size_t Length, DstType *out,
                             const SrcType *odd, const SrcType *even,
                             uint32_t Shift);

template <typename SrcType, typename DstType>
void Ripple_narrow_shift_rnd_sat(std::size_t Length, DstType *out,
                                 const SrcType *odd, const SrcType *even,
                                 uint32_t Shift);

template <typename SrcType, typename DstType>
void Ripple_narrow_shift_noshuff(std::size_t Length, DstType *out,
                                 const SrcType *odd, const SrcType *even,
                                 uint32_t Shift);

template <typename SrcType, typename DstType>
void Ripple_narrow_shift_sat_noshuff(std::size_t Length, DstType *out,
                                     const SrcType *odd, const SrcType *even,
                                     uint32_t Shift);

template <typename SrcType, typename DstType>
void Ripple_narrow_shift_rnd_sat_noshuff(std::size_t Length, DstType *out,
                                         const SrcType *odd,
                                         const SrcType *even, uint32_t Shift);
