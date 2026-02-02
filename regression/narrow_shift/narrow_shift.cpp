//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include "narrow_shift.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ripple.h>
#include <ripple/HVX_Narrow_Shift.h>
#include <stdio.h>
#define VECTOR_BYTES 128

template <typename SrcType, typename DstType, typename FuncTag>
inline void Ripple_narrow_shift_kernel(std::size_t Length, DstType *output,
                                       const SrcType *Vec_odd,
                                       const SrcType *Vec_even, uint32_t Shift,
                                       FuncTag shift_func) {
  auto BlockShape = ripple_set_block_shape(0, VECTOR_BYTES / sizeof(SrcType));
  ripple_parallel(BlockShape, 0);
  for (std::size_t i = 0; i < Length; ++i) {
    output[i] = shift_func(Vec_odd[i], Vec_even[i], Shift);
  }
}

// i32--> u16
template <>
void Ripple_narrow_shift_sat<int32_t, uint32_t>(std::size_t Length,
                                                uint32_t *out,
                                                const int32_t *odd,
                                                const int32_t *even,
                                                uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i32tou16);
}

template <>
void Ripple_narrow_shift_rnd_sat<int32_t, uint32_t>(std::size_t Length,
                                                    uint32_t *out,
                                                    const int32_t *odd,
                                                    const int32_t *even,
                                                    uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i32tou16);
}

template <>
void Ripple_narrow_shift_sat_noshuff<int32_t, uint32_t>(std::size_t Length,
                                                        uint32_t *out,
                                                        const int32_t *odd,
                                                        const int32_t *even,
                                                        uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i32tou16_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<int32_t, uint32_t>(std::size_t Length,
                                                            uint32_t *out,
                                                            const int32_t *odd,
                                                            const int32_t *even,
                                                            uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i32tou16_noshuff);
}

// i32--> i16
template <>
void Ripple_narrow_shift<int32_t, int32_t>(std::size_t Length, int32_t *out,
                                           const int32_t *odd,
                                           const int32_t *even,
                                           uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift, hvx_narsh_i32toi16);
}

template <>
void Ripple_narrow_shift_sat<int32_t, int32_t>(std::size_t Length, int32_t *out,
                                               const int32_t *odd,
                                               const int32_t *even,
                                               uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i32toi16);
}

template <>
void Ripple_narrow_shift_rnd_sat<int32_t, int32_t>(std::size_t Length,
                                                   int32_t *out,
                                                   const int32_t *odd,
                                                   const int32_t *even,
                                                   uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i32toi16);
}

template <>
void Ripple_narrow_shift_noshuff<int32_t, int32_t>(std::size_t Length,
                                                   int32_t *out,
                                                   const int32_t *odd,
                                                   const int32_t *even,
                                                   uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_i32toi16_noshuff);
}

template <>
void Ripple_narrow_shift_sat_noshuff<int32_t, int32_t>(std::size_t Length,
                                                       int32_t *out,
                                                       const int32_t *odd,
                                                       const int32_t *even,
                                                       uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i32toi16_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<int32_t, int32_t>(std::size_t Length,
                                                           int32_t *out,
                                                           const int32_t *odd,
                                                           const int32_t *even,
                                                           uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i32toi16_noshuff);
}

// u32 --> u16
template <>
void Ripple_narrow_shift_sat<uint32_t, uint32_t>(std::size_t Length,
                                                 uint32_t *out,
                                                 const uint32_t *odd,
                                                 const uint32_t *even,
                                                 uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_u32tou16);
}

template <>
void Ripple_narrow_shift_rnd_sat<uint32_t, uint32_t>(std::size_t Length,
                                                     uint32_t *out,
                                                     const uint32_t *odd,
                                                     const uint32_t *even,
                                                     uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_u32tou16);
}

template <>
void Ripple_narrow_shift_sat_noshuff<uint32_t, uint32_t>(std::size_t Length,
                                                         uint32_t *out,
                                                         const uint32_t *odd,
                                                         const uint32_t *even,
                                                         uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_u32tou16_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<uint32_t, uint32_t>(
    std::size_t Length, uint32_t *out, const uint32_t *odd,
    const uint32_t *even, uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_u32tou16_noshuff);
}

// u16 --> u8
template <>
void Ripple_narrow_shift_sat<uint16_t, uint16_t>(std::size_t Length,
                                                 uint16_t *out,
                                                 const uint16_t *odd,
                                                 const uint16_t *even,
                                                 uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_u16tou8);
}

template <>
void Ripple_narrow_shift_rnd_sat<uint16_t, uint16_t>(std::size_t Length,
                                                     uint16_t *out,
                                                     const uint16_t *odd,
                                                     const uint16_t *even,
                                                     uint32_t Shift) {

  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_u16tou8);
}

template <>
void Ripple_narrow_shift_sat_noshuff<uint16_t, uint16_t>(std::size_t Length,
                                                         uint16_t *out,
                                                         const uint16_t *odd,
                                                         const uint16_t *even,
                                                         uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_u16tou8_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<uint16_t, uint16_t>(
    std::size_t Length, uint16_t *out, const uint16_t *odd,
    const uint16_t *even, uint32_t Shift) {

  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_u16tou8_noshuff);
}

// i16 --> u8
template <>
void Ripple_narrow_shift_sat<int16_t, uint16_t>(std::size_t Length,
                                                uint16_t *out,
                                                const int16_t *odd,
                                                const int16_t *even,
                                                uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i16tou8);
}

template <>
void Ripple_narrow_shift_rnd_sat<int16_t, uint16_t>(std::size_t Length,
                                                    uint16_t *out,
                                                    const int16_t *odd,
                                                    const int16_t *even,
                                                    uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i16tou8);
}

template <>
void Ripple_narrow_shift_sat_noshuff<int16_t, uint16_t>(std::size_t Length,
                                                        uint16_t *out,
                                                        const int16_t *odd,
                                                        const int16_t *even,
                                                        uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i16tou8_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<int16_t, uint16_t>(std::size_t Length,
                                                            uint16_t *out,
                                                            const int16_t *odd,
                                                            const int16_t *even,
                                                            uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i16tou8_noshuff);
}

// i16 --> i8
template <>
void Ripple_narrow_shift_sat<int16_t, int16_t>(std::size_t Length, int16_t *out,
                                               const int16_t *odd,
                                               const int16_t *even,
                                               uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i16toi8);
}

template <>
void Ripple_narrow_shift_rnd_sat<int16_t, int16_t>(std::size_t Length,
                                                   int16_t *out,
                                                   const int16_t *odd,
                                                   const int16_t *even,
                                                   uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i16toi8);
}

template <>
void Ripple_narrow_shift_sat_noshuff<int16_t, int16_t>(std::size_t Length,
                                                       int16_t *out,
                                                       const int16_t *odd,
                                                       const int16_t *even,
                                                       uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_sat_i16toi8_noshuff);
}

template <>
void Ripple_narrow_shift_rnd_sat_noshuff<int16_t, int16_t>(std::size_t Length,
                                                           int16_t *out,
                                                           const int16_t *odd,
                                                           const int16_t *even,
                                                           uint32_t Shift) {
  Ripple_narrow_shift_kernel(Length, out, odd, even, Shift,
                             hvx_narsh_rnd_sat_i16toi8_noshuff);
}

// Ripple
