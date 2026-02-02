//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <ripple-test-suite/ripple-test-suite.h>

#include "narrow_shift.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <hexagon_protos.h>
#include <hexagon_standalone.h>
#include <hexagon_types.h>
#include <inttypes.h>
#include <limits>
#include <ripple.h>
#include <stdio.h>
#include <vector>

#include <iomanip>
#include <iostream>

namespace {
using namespace ripple_test_suite;

enum class ShiftMode { Trunc, Sat, RndSat };

template <typename T> constexpr bool is_signed_helper() { return T(-1) < T(0); }

template <typename InType, typename OutType> OutType saturate(InType value) {
  const InType out_max = std::numeric_limits<OutType>::max();
  const InType out_min = std::numeric_limits<OutType>::min();

  constexpr bool in_signed = is_signed_helper<InType>();
  constexpr bool out_signed = is_signed_helper<OutType>();

  if (in_signed && !out_signed) {
    if (value < 0) {
      return 0;
    }
    if (value > out_max) {
      return static_cast<OutType>(out_max);
    }
    return static_cast<OutType>(value);
  } else if (!in_signed && out_signed) {
    if (value > out_max) {
      return static_cast<OutType>(out_max);
    }
    return static_cast<OutType>(value);
  } else {
    if (value > out_max) {
      return static_cast<OutType>(out_max);
    }
    if (value < out_min) {
      return static_cast<OutType>(out_min);
    }
    return static_cast<OutType>(value);
  }
}

// Packing helpers
inline uint16_t pack(uint8_t even, uint8_t odd) {
  return (uint16_t(even) << 8) | uint16_t(odd);
}

inline int16_t pack(int8_t even, int8_t odd) {
  // Avoid sign-extension when packing
  uint8_t even_u = static_cast<uint8_t>(even) & 0xFF;
  uint8_t odd_u = static_cast<uint8_t>(odd) & 0xFF;
  return (int16_t(even_u) << 8) | odd_u;
}

inline uint32_t pack(uint16_t even, uint16_t odd) {
  return (uint32_t(even) << 16) | uint32_t(odd);
}

inline int32_t pack(int16_t even, int16_t odd) {
  // Avoid sign-extension when packing
  uint16_t even_u = static_cast<uint16_t>(even) & 0xFFFF;
  uint16_t odd_u = static_cast<uint16_t>(odd) & 0xFFFF;
  return (int32_t(even_u) << 16) | odd_u;
}

// Helper to deduce a Wide Type capable of holding two narrowed
// values without overflow when packed together.
uint32_t deduce_wide_type(uint16_t *) { return uint32_t(); }
int32_t deduce_wide_type(int16_t *) { return int32_t(); }
uint64_t deduce_wide_type(uint32_t *) { return uint64_t(); }
int64_t deduce_wide_type(int32_t *) { return int64_t(); }

#define WIDE_TYPE_OF(src_type) decltype(deduce_wide_type((src_type *)nullptr))

// Helpers to deduce individual narrowed element's type
uint16_t deduce_individual_type(uint32_t *) { return uint16_t(); }
int16_t deduce_individual_type(int32_t *) { return int16_t(); }
uint8_t deduce_individual_type(uint16_t *) { return uint8_t(); }
int8_t deduce_individual_type(int16_t *) { return int8_t(); }

#define INDIVIDUAL_TYPE_OF(dst_type)                                           \
  decltype(deduce_individual_type((dst_type *)nullptr))

template <typename SrcType, typename DstType>
void ref_shift(size_t N, DstType *output, const SrcType *Vu, const SrcType *Vv,
               int32_t Rt, ShiftMode mode) {

  const int shamt = (sizeof(SrcType) == 2) ? (Rt & 7) : (Rt & 15);
  using WideType = WIDE_TYPE_OF(SrcType);
  const WideType add_rnd = (mode == ShiftMode::RndSat && shamt > 0)
                               ? (WideType(1) << (shamt - 1))
                               : WideType(0);

  for (size_t i = 0; i < N; ++i) {
    WideType even =
        static_cast<WideType>(Vv[i]) + static_cast<WideType>(add_rnd);
    WideType odd =
        static_cast<WideType>(Vu[i]) + static_cast<WideType>(add_rnd);

    if (shamt) {
      even >>= shamt;
      odd >>= shamt;
    }
    using IndvType = INDIVIDUAL_TYPE_OF(DstType);
    IndvType even_indv, odd_indv;
    if (mode == ShiftMode::Trunc) {
      const int elem_mask = (sizeof(IndvType) == 1) ? 0xFF : 0xFFFF;
      even_indv = static_cast<IndvType>(even & elem_mask);
      odd_indv = static_cast<IndvType>(odd & elem_mask);
    } else {
      even_indv = saturate<WideType, IndvType>(even);
      odd_indv = saturate<WideType, IndvType>(odd);
    }
    output[i] = pack(even_indv, odd_indv);
  }
}

template <typename SrcType, typename DstType>
void ref_shift_noshuff(size_t N, DstType *output, const SrcType *Vu,
                       const SrcType *Vv, int32_t Rt, ShiftMode mode) {
  const int shamt = (sizeof(SrcType) == 2) ? (Rt & 7) : (Rt & 15);
  using WideType = WIDE_TYPE_OF(SrcType);
  using IndvType = INDIVIDUAL_TYPE_OF(DstType);

  const WideType add_rnd = (mode == ShiftMode::RndSat && shamt > 0)
                               ? (WideType(1) << (shamt - 1))
                               : WideType(0);

  std::vector<IndvType> temp;
  temp.reserve(2 * N);

  for (size_t i = 0; i < N; ++i) {
    WideType even = static_cast<WideType>(Vv[i]) + add_rnd;

    if (shamt) {
      even >>= shamt;
    }

    IndvType e;
    if (mode == ShiftMode::Trunc) {
      const int elem_mask = (sizeof(IndvType) == 1) ? 0xFF : 0xFFFF;
      e = static_cast<IndvType>(even & elem_mask);
    } else {
      e = saturate<WideType, IndvType>(even);
    }

    temp.push_back(e);
  }
  for (size_t i = 0; i < N; ++i) {
    WideType odd = static_cast<WideType>(Vu[i]) + add_rnd;

    if (shamt) {
      odd >>= shamt;
    }

    IndvType o;
    if (mode == ShiftMode::Trunc) {
      const int elem_mask = (sizeof(IndvType) == 1) ? 0xFF : 0xFFFF;
      o = static_cast<IndvType>(odd & elem_mask);
    } else {
      o = saturate<WideType, IndvType>(odd);
    }
    temp.push_back(o);
  }

  // Pack every two elements into output array
  for (size_t i = 0; i < N; ++i) {
    IndvType low = temp[2 * i];
    IndvType high = temp[2 * i + 1];
    output[i] = pack(high, low);
  }
}

// Vector Value initializers
inline void initialize_inputs(std::vector<uint32_t> &vec) {
  const uint32_t init_vals[] = {
      0xFFFFFFFFu, // Max u32 → always saturates to 0xFFFF
      0x00000000u, // Zero → baseline test, no rounding/saturation
      0x000003E8u, // 1000 → small value, tests rounding/truncation
      0x0000FFFFu, // Max u16 → should pass through unchanged
      0x00008000u, // 32768 → just above i16 max, rounds to 32768 → saturates
      0x0001007Fu, // 65663 → rounds to 32832 → saturates
      0x00010080u, // 65664 → rounds to 32832 → saturates
      0x00010081u, // 65665 → rounds to 32833 → saturates due to rounding
      0x7FFE7FFFu, // Lower 16 bits near i16 max → tests packing/saturation
      0x7FFE8000u, // Lower 16 bits at i16 max → tests saturation boundary
      0x7FFE8001u, // Lower 16 bits just above i16 max → tests overflow
      0x0000F0F0u  // Patterned value
  };

  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 12];
}

inline void initialize_inputs(std::vector<int32_t> &vec) {
  const int32_t init_vals[] = {
      0x7FFFFFFF, // Max i32 → always saturates to 32767
      0x00000000, // Zero → baseline test, no rounding/saturation
      0x000003E8, // 1000 → rounds to 1000 + 1 >> 1 = 500 → no saturation
      0x0000FFFF, // 65535 → rounds to 32768 → saturates
      0x00008000, // 32768 → rounds to 16384 → no saturation
      0x0001007F, // 65663 → rounds to 32832 → saturates
      0x00010080, // 65664 → rounds to 32832 → saturates
      0x00010081, // 65665 → rounds to 32833 → saturates due to rounding
      -1,         // Negative value → truncates to -1 → no saturation
      -32768,     // Min i16 → should pass through unchanged
      -32769,     // Just below i16 min → saturates to -32768
      0x7FFE7FFF, // Lower 16 bits near i16 max → tests packing/saturation
      0x7FFE8000, // Lower 16 bits at i16 max → tests saturation boundary
      0x7FFE8001, // Lower 16 bits just above i16 max → tests overflow
      0x0000F0F0, // Patterned value
      0x0000F1F1  // Patterned value
  };

  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 16];
}

inline void initialize_inputs(std::vector<uint16_t> &vec) {
  const uint16_t init_vals[] = {
      0xFFFFu, // Max → saturates to 0xFF
      0x03E8u, // Small (1000)
      0x00FFu, // Max uint8_t
      0x0100u, // Just above uint8_t max → clamps
      0x00FEu, // Just below max
      0x0080u, // Midpoint of uint8_t
      0x7FFFu, // Large value → clamps
      0x0F0Fu, // Patterned value
      0xF0F0u, // Patterned value
      0x0101u, // Just above max with both bits set
      0x0000u  // Zero
  };

  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 11];
}

inline void initialize_inputs(std::vector<int16_t> &vec) {
  const int16_t init_vals[] = {
      0x7FFF, // Max int16_t → saturates to 127
      0x03E8, // Small (1000)
      0x00FF, // 255 → overflow for int8_t
      0x007F, // Max int8_t
      0x0080, // Just above int8_t max
      0x0100, // 256 → overflow
      -1,     // Negative value
      -128,   // Min int8_t
      -129,   // Just below int8_t min → saturates
      -32768, // Min int16_t
      0x7F7F, // Patterned value
      0x0000  // Zero
  };
  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 12];
}

inline void initialize_inputs_Odd(std::vector<uint32_t> &vec) {
  const uint32_t init_vals[] = {
      0x22222222u,
      0x33333333u,
      0x44444444u,
      0x55555555u,
  };
  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 4];
}

inline void initialize_inputs_Odd(std::vector<int32_t> &vec) {
  const int32_t init_vals[] = {
      0x02222222,
      0x03333333,
      0x04444444,
      0x05555555,
  };
  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 4];
}

inline void initialize_inputs_Odd(std::vector<uint16_t> &vec) {
  const uint16_t init_vals[] = {
      0x2222u,
      0x3333u,
      0x4444u,
      0x5555u,
  };
  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 4];
}

inline void initialize_inputs_Odd(std::vector<int16_t> &vec) {
  const int16_t init_vals[] = {
      0x2222,
      0x3333,
      0x4444,
      0x5555,
  };
  for (size_t i = 0; i < vec.size(); ++i)
    vec[i] = init_vals[i % 4];
}

// Base test class
template <typename SrcType, typename DstType> class TestBase : public Test {
protected:
  const size_t N;
  std::vector<SrcType> VecEven, VecOdd;
  std::vector<DstType> Output, RefOutput;

public:
  TestBase(TestFramework &TF, size_t N)
      : Test(TF), N(N), VecEven(N), VecOdd(N), Output(N), RefOutput(N) {}

  void prepare(unsigned) override {
    initialize_inputs(VecEven);
    initialize_inputs_Odd(VecOdd);
  }

  bool verify() const override {
    bool all_match = true;
    for (size_t i = 0; i < N; ++i) {
      if (RefOutput[i] != Output[i]) {
        all_match = false;
        std::cerr << "Mismatch @" << i << ": input_even=0x" << std::hex
                  << std::setw(8) << std::setfill('0')
                  << static_cast<uint32_t>(VecEven[i]) << ": input_even[i+1]=0x"
                  << std::hex << std::setw(8) << std::setfill('0')
                  << static_cast<uint32_t>(VecEven[i + 1]) << " ripple=0x"
                  << std::setw(8) << static_cast<uint32_t>(Output[i])
                  << " ref=0x" << std::setw(8)
                  << static_cast<uint32_t>(RefOutput[i]) << std::dec
                  << std::endl;
      }
    }
    return all_match;
  }
};

template <typename SrcType, typename DstType, int Shift, ShiftMode Mode,
          size_t N>
class RippleNarrowShiftTest : public TestBase<SrcType, DstType> {
public:
  RippleNarrowShiftTest(TestFramework &TF)
      : TestBase<SrcType, DstType>(TF, N) {}

  void prepare(unsigned) override {
    this->TestBase<SrcType, DstType>::prepare(0);
    ref_shift<SrcType, DstType>(N, &this->RefOutput[0], &this->VecOdd[0],
                                &this->VecEven[0], Shift, Mode);
  }

  void run(unsigned) override {
    // Early exit
    if (Mode == ShiftMode::Trunc) {
      if (!(sizeof(SrcType) == 4 && is_signed_helper<SrcType>() &&
            deduce_individual_type((DstType *)nullptr) == int16_t())) {
        std::cerr
            << "Error: Trunc mode is only supported for narrowing i32 to i16\n";
        std::exit(1);
      }
    }
    switch (Mode) {
    case ShiftMode::Trunc:
      Ripple_narrow_shift<SrcType, DstType>(
          N, &this->Output[0], &this->VecEven[0], &this->VecOdd[0], Shift);
      break;
    case ShiftMode::Sat:
      Ripple_narrow_shift_sat<SrcType, DstType>(
          N, &this->Output[0], &this->VecEven[0], &this->VecOdd[0], Shift);
      break;
    case ShiftMode::RndSat:
      Ripple_narrow_shift_rnd_sat<SrcType, DstType>(
          N, &this->Output[0], &this->VecEven[0], &this->VecOdd[0], Shift);
      break;
    }
  }
};

template <typename SrcType, typename DstType, int Shift, ShiftMode Mode,
          size_t N>
class RippleNarrowShiftNoShuffTest : public TestBase<SrcType, DstType> {
public:
  RippleNarrowShiftNoShuffTest(TestFramework &TF)
      : TestBase<SrcType, DstType>(TF, N) {}

  void prepare(unsigned) override {
    this->TestBase<SrcType, DstType>::prepare(0);
    ref_shift_noshuff<SrcType, DstType>(N, &this->RefOutput[0],
                                        &this->VecOdd[0], &this->VecEven[0],
                                        Shift, Mode);
  }

  void run(unsigned) override {
    switch (Mode) {
    case ShiftMode::Trunc:
      Ripple_narrow_shift_noshuff<SrcType, DstType>(
          N, &this->Output[0], &this->VecOdd[0], &this->VecEven[0], Shift);
      break;
    case ShiftMode::Sat:
      Ripple_narrow_shift_sat_noshuff<SrcType, DstType>(
          N, &this->Output[0], &this->VecOdd[0], &this->VecEven[0], Shift);
      break;
    case ShiftMode::RndSat:
      Ripple_narrow_shift_rnd_sat_noshuff<SrcType, DstType>(
          N, &this->Output[0], &this->VecOdd[0], &this->VecEven[0], Shift);
      break;
    }
  }
};

// u32 → u16
DefineTest<
    RippleNarrowShiftTest<uint32_t, uint32_t, 2, ShiftMode::Sat, /*N=*/129>>
    RippleNarrowShift_u32tou16_Sat_S2("RippleNarrowShift_u32tou16_Sat_S2");
DefineTest<
    RippleNarrowShiftTest<uint32_t, uint32_t, 8, ShiftMode::Sat, /*N=*/129>>
    RippleNarrowShift_u32tou16_Sat_S8("RippleNarrowShiftSat_u32tou16_Sat_S8");
DefineTest<
    RippleNarrowShiftTest<uint32_t, uint32_t, 16, ShiftMode::RndSat, /*N=*/129>>
    RippleNarrowShift_u32tou16_RndSat_S16(
        "RippleNarrowShiftRndSat_u32tou16_RndSat_S16");

// i32 → i16
DefineTest<
    RippleNarrowShiftTest<int32_t, int32_t, 1, ShiftMode::Trunc, /*N=*/500>>
    RippleNarrowShift_i32toi16_S1("RippleNarrowShift_i32toi16_S1");
DefineTest<
    RippleNarrowShiftTest<int32_t, int32_t, 3, ShiftMode::Sat, /*N=*/500>>
    RippleNarrowShift_i32toi16_Sat_S3("RippleNarrowShift_i32toi16_Sat_S3");
DefineTest<
    RippleNarrowShiftTest<int32_t, int32_t, 15, ShiftMode::RndSat, /*N=*/500>>
    RippleNarrowShift_i32toi16_RndSat_S15(
        "RippleNarrowShift_i32toi16_RndSat_S15");

// i32 → u16
DefineTest<
    RippleNarrowShiftTest<int32_t, uint32_t, 2, ShiftMode::Sat, /*N=*/65>>
    RippleNarrowShift_i32tou16_Sat_S2("RippleNarrowShift_i32tou16_Sat_S2");
DefineTest<
    RippleNarrowShiftTest<int32_t, uint32_t, 8, ShiftMode::Sat, /*N=*/65>>
    RippleNarrowShift_i32tou16_Sat_S8("RippleNarrowShift_i32tou16_Sat_S8");
DefineTest<
    RippleNarrowShiftTest<int32_t, uint32_t, 16, ShiftMode::RndSat, /*N=*/65>>
    RippleNarrowShift_i32tou16_RndSat_S16(
        "RippleNarrowShift_i32tou16_RndSat_S16");

// u16 → u8
DefineTest<
    RippleNarrowShiftTest<uint16_t, uint16_t, 2, ShiftMode::Sat, /*N=*/65>>
    RippleNarrowShift_u16tou8_Sat("RippleNarrowShift_u16tou8_Sat_S2");
DefineTest<
    RippleNarrowShiftTest<uint16_t, uint16_t, 7, ShiftMode::RndSat, /*N=*/65>>
    RippleNarrowShift_u16tou8_RndSat("RippleNarrowShift_u16tou8_RndSat_S7");

// i16 → i8
DefineTest<RippleNarrowShiftTest<int16_t, int16_t, 2, ShiftMode::Sat, /*N=*/38>>
    RippleNarrowShift_i16toi8_Sat("RippleNarrowShift_i16toi8_Sat_S2");
DefineTest<
    RippleNarrowShiftTest<int16_t, int16_t, 8, ShiftMode::RndSat, /*N=*/38>>
    RippleNarrowShift_i16toi8_RndSat("RippleNarrowShift_i16toi8_RndSat_S8");

// i16 → u8
DefineTest<
    RippleNarrowShiftTest<int16_t, uint16_t, 3, ShiftMode::Sat, /*N=*/320>>
    RippleNarrowShift_i16tou8_Sat("RippleNarrowShift_i16tou8_Sat_S3");
DefineTest<
    RippleNarrowShiftTest<int16_t, uint16_t, 6, ShiftMode::RndSat, /*N=*/320>>
    RippleNarrowShift_i16tou8_RndSat("RippleNarrowShift_i16tou8_RndSat_S6");

///////////////////////////////No Shuffle////////////////////////////////
// Currently, these tests should only be executed on HVX native vector size
// i.e., 32 elements for 32 to 16 bit narrowing and 64 elements for 16 to 8 bit
// narrowing.

// u32 → u16
DefineTest<RippleNarrowShiftNoShuffTest<uint32_t, uint32_t, 2, ShiftMode::Sat,
                                        /*N=*/32>>
    RippleNarrowShift_u32tou16_S2_Sat_NoShuff(
        "RippleNarrowShift_u32tou16_S2_Sat_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<uint32_t, uint32_t, 8, ShiftMode::Sat,
                                        /*N=*/32>>
    RippleNarrowShift_u32tou16_S8_Sat_NoShuff(
        "RippleNarrowShift_u32tou16_S8_Sat_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<uint32_t, uint32_t, 16,
                                        ShiftMode::RndSat, /*N=*/32>>
    RippleNarrowShift_u32tou16_S16_RndSat_NoShuff(
        "RippleNarrowShift_u32tou16_S16_RndSat_NoShuff");

// i32 → i16
DefineTest<RippleNarrowShiftNoShuffTest<int32_t, int32_t, 0, ShiftMode::Trunc,
                                        /*N=*/32>>
    RippleNarrowShift_i32toi16_S1_NoShuff(
        "RippleNarrowShift_i32toi16_S1_NoShuff");
DefineTest<
    RippleNarrowShiftNoShuffTest<int32_t, int32_t, 3, ShiftMode::Sat, /*N=*/32>>
    RippleNarrowShift_i32toi16_Sat_S3_NoShuff(
        "RippleNarrowShift_i32toi16_Sat_S3_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<int32_t, int32_t, 15, ShiftMode::RndSat,
                                        /*N=*/32>>
    RippleNarrowShift_i32toi16_RndSat_S15_NoShuff(
        "RippleNarrowShift_i32toi16_RndSat_S15_NoShuff");

// i32 → u16
DefineTest<RippleNarrowShiftNoShuffTest<int32_t, uint32_t, 2, ShiftMode::Sat,
                                        /*N=*/32>>
    RippleNarrowShift_i32tou16_Sat_S2_NoShuff(
        "RippleNarrowShift_i32tou16_Sat_S2_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<int32_t, uint32_t, 8, ShiftMode::Sat,
                                        /*N=*/32>>
    RippleNarrowShift_i32tou16_Sat_S8_NoShuff(
        "RippleNarrowShift_i32tou16_Sat_S8_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<int32_t, uint32_t, 16,
                                        ShiftMode::RndSat, /*N=*/32>>
    RippleNarrowShift_i32tou16_RndSat_S16_NoShuff(
        "RippleNarrowShift_i32tou16_RndSat_S16_NoShuff");

// u16 → u8
DefineTest<RippleNarrowShiftNoShuffTest<uint16_t, uint16_t, 2, ShiftMode::Sat,
                                        /*N=*/64>>
    RippleNarrowShift_u16tou8_Sat_NoShuff(
        "RippleNarrowShift_u16tou8_Sat_S2_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<uint16_t, uint16_t, 7,
                                        ShiftMode::RndSat, /*N=*/64>>
    RippleNarrowShift_u16tou8_RndSat_NoShuff(
        "RippleNarrowShift_u16tou8_RndSat_S7_NoShuff");

// i16 → i8
DefineTest<
    RippleNarrowShiftNoShuffTest<int16_t, int16_t, 2, ShiftMode::Sat, /*N=*/64>>
    RippleNarrowShift_i16toi8_Sat_NoShuff(
        "RippleNarrowShift_i16toi8_Sat_S2_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<int16_t, int16_t, 8, ShiftMode::RndSat,
                                        /*N=*/64>>
    RippleNarrowShift_i16toi8_RndSat_NoShuff(
        "RippleNarrowShift_i16toi8_RndSat_S8_NoShuff");

// i16 → u8
DefineTest<RippleNarrowShiftNoShuffTest<int16_t, uint16_t, 3, ShiftMode::Sat,
                                        /*N=*/64>>
    RippleNarrowShift_i16tou8_Sat_NoShuff(
        "RippleNarrowShift_i16tou8_Sat_S3_NoShuff");
DefineTest<RippleNarrowShiftNoShuffTest<int16_t, uint16_t, 6, ShiftMode::RndSat,
                                        /*N=*/64>>
    RippleNarrowShift_i16tou8_RndSat_NoShuff(
        "RippleNarrowShift_i16tou8_RndSat_S6_NoShuff");

} // namespace
