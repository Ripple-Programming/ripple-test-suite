//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#ifdef __cplusplus
}
#endif
#include <ripple.h>

#ifndef HVX_LANE
#define HVX_LANE 0
#endif

// _______________________ Unzipping small-length tuples _______________________
//
//

/// @brief A 1-d unzip Ripple source function (cf Ripple manual).
/// Goes from a zipped [(a,b,c), (a,b,c), ... (a,b,c)] to an unzipped one
/// [(a,a,..., a), (b, b, ..., b), (c, c, ..., c)]
///
/// This basically creates N_PACKS unzipped "packs", considering that we are in
/// Ex: We can start from a set of SET_SIZE blocks:
/// [abcabcab][cabcabca][bcabcabc]
/// We can use:
/// - unzip<3, 0, 0> on block 0 to get  [aaabbbcc] ('a' in position 0), then
/// - unzip<3, 1, 1> on block 1 to get [cccaaabb] ('a' in position 1), then
/// - unzip<3, 2, 2> on block 2 to get  [bbbcccaa] ('a' in position 2).
///
/// We can also introduce a rotation to align the unzipped data
/// on a given set index, for instance pack 1 (the b's)
/// We can use:
/// - unzip<3, 0, 2> on block 0 to get  [bbbccaaa], then
/// - unzip<3, 1, 0> on the second block to get [aaabbccc], then
/// - unzip<3, 2, 1> on the third block to get  [cccaabbb].
///
/// The advantage of that second strategy
/// is that the sequence of b's (pack # 1) is now
/// in the right order across the unzipped blocks,
/// and all we need to do is combine the blocks to get all the b's.
///
/// @tparam SET_SIZE the number of elements in a zipped element
///         (in the ex: 3, we have triples)
/// @tparam CUR_BLOCK index of the block we are unzipping,
///         among the sequence of blocks to be unzipped.
/// @tparam OFFSET the position in which
///         we are interested in putting the first element's packet
///         among the unzipped packets.
/// @param dst_id the destination vector lane
/// @param n the block size
template <int SET_SIZE, int CUR_BLOCK, int OFFSET>
static size_t shuffle_unzip(size_t dst_id, size_t n) {
  // Position of the first occurrence of set CUR_SET in this block in the source
  size_t pack_size = (n + SET_SIZE - 1) / SET_SIZE; // ceil(n, set_size)
  size_t src_set = dst_id / pack_size;
  size_t src_rank = dst_id % pack_size;

  // Offset of the first element in the current set
  size_t per_block_offset = pack_size * SET_SIZE - n;
  size_t cur_block_offset = (per_block_offset * CUR_BLOCK) % SET_SIZE;
  // We want the 1st element's offset in the current block to be OFFSET
  size_t corrective_offset = OFFSET - cur_block_offset;

  // Shift the source function by CUR_SET
  return SET_SIZE * src_rank + (src_set + corrective_offset) % SET_SIZE;
}

/// @brief Portable vectorized load of a block of pairs into a pair of blocks.
/// Meant to be inlined always.
template <typename ElType>
[[gnu::always_inline]] static void unzip_pairs(ElType &zipped_pairs0,
                                               ElType &zipped_pairs1,
                                               ElType &evens, ElType &odds) {
  size_t v = ripple_id(HVX_LANE, 0);
  size_t one_half = (ripple_get_block_size(HVX_LANE, 0) + 1) / 2;

  // Note: This may be a lot of shuffles. Using shuffle_unzip<2, 0) + rotate
  // might be faster.
  ElType even0 = ripple_shuffle(zipped_pairs0, shuffle_unzip<2, 0, 0>);
  ElType even1 = ripple_shuffle(zipped_pairs1, shuffle_unzip<2, 1, 1>);
  evens = v < one_half ? even0 : even1;

  ElType odd0 = ripple_shuffle(zipped_pairs0, shuffle_unzip<2, 0, 1>);
  ElType odd1 = ripple_shuffle(zipped_pairs1, shuffle_unzip<2, 1, 0>);
  odds = v < one_half ? odd0 : odd1;
}

/// @brief Portable vectorized load and unzip of a block of triples
/// into a triple of blocks.
/// Meant to be inlined always.
template <typename ElType>
[[gnu::always_inline]] static void
unzip_triples(ElType &zipped_triples0, ElType &zipped_triples1,
              ElType &zipped_triples2, ElType &unzipped0, ElType &unzipped1,
              ElType &unzipped2) {
  size_t v = ripple_id(HVX_LANE, 0);
  size_t nv = ripple_get_block_size(HVX_LANE, 0);
  size_t one_third = (nv + 2) / 3;
  size_t small_third = nv / 3;
  // The third packet may start in one of two places
  size_t two_thirds = (one_third + 2 * small_third == nv)
                          ? one_third + small_third
                          : 2 * one_third;

  // The strategy here is to form abc, cab ('a' rotated to an offset of 1)
  // and bca ('a' rotated to an offset of 2) and then mix the blocks
  ElType abc = ripple_shuffle(zipped_triples0, shuffle_unzip<3, 0, 0>);
  ElType cab = ripple_shuffle(zipped_triples1, shuffle_unzip<3, 1, 1>);
  ElType bca = ripple_shuffle(zipped_triples2, shuffle_unzip<3, 2, 2>);
  ElType aab = v < one_third ? abc : cab;
  ElType bbc = v < one_third ? bca : abc;
  ElType cca = v < one_third ? cab : bca;
  unzipped0 = v < two_thirds ? aab : bca; // all a's are in the right position

  ElType bbb = v < two_thirds ? bbc : cab;
  // The first set of b's made it to the second position, from abc. Correct it.
  unzipped1 = ripple_rotate_to_lower(bbb, one_third);

  ElType ccc = v < two_thirds ? cca : bbc;
  // The first set of c's made it to the third position, from abc. Correct it.
  unzipped2 = ripple_rotate_to_lower(ccc, two_thirds);
}

/// @brief Portable vectorized load and unzip of a block of quads
/// into a quad of blocks.
/// Meant to be inlined always.
template <typename ElType>
[[gnu::always_inline]] static void
unzip_quads(ElType &zipped_quads0, ElType &zipped_quads1, ElType &zipped_quads2,
            ElType &zipped_quads3, ElType &unzipped0, ElType &unzipped1,
            ElType &unzipped2, ElType &unzipped3) {
  size_t v = ripple_id(HVX_LANE, 0);
  size_t nv = ripple_get_block_size(HVX_LANE, 0);
  size_t one_quarter = (nv + 3) / 4;
  size_t small_quarter = nv / 4;
  size_t two_quarters;
  size_t three_quarters;
  if (one_quarter + 3 * small_quarter == nv) {
    two_quarters = one_quarter + small_quarter;
    three_quarters = two_quarters + small_quarter;
  } else {
    two_quarters = 2 * one_quarter;
    if (two_quarters + 2 * small_quarter == nv) {
      three_quarters = two_quarters + small_quarter;
    } else {
      three_quarters = two_quarters + one_quarter;
    }
  }

  // The strategy here is to form abcd, dabc ('a' rotated to an offset of 1),
  // cdab ('a' rotated to an offset of 2) and bcda and then mix the blocks
  ElType abcd = ripple_shuffle(zipped_quads0, shuffle_unzip<4, 0, 0>);
  ElType dabc = ripple_shuffle(zipped_quads1, shuffle_unzip<4, 1, 1>);
  ElType cdab = ripple_shuffle(zipped_quads2, shuffle_unzip<4, 2, 2>);
  ElType bcda = ripple_shuffle(zipped_quads3, shuffle_unzip<4, 3, 3>);
  ElType aabc = v < one_quarter ? abcd : dabc;
  ElType bbcd = v < one_quarter ? bcda : abcd;
  ElType ccda = v < one_quarter ? cdab : bcda;
  ElType ddab = v < one_quarter ? dabc : cdab;
  ElType aaab = v < two_quarters ? aabc : ddab;
  ElType bbbc = v < two_quarters ? bbcd : aabc;
  ElType cccd = v < two_quarters ? ccda : bbcd;
  ElType ddda = v < two_quarters ? ddab : ccda;
  // all a's are in the right position
  unzipped0 = v < three_quarters ? aaab : ddda;
  ElType bbbb = v < three_quarters ? bbbc : aaab;
  // The first set of b's made it to the second position, from abcd. Correct it.
  unzipped1 = ripple_rotate_to_lower(bbbb, one_quarter);
  ElType cccc = v < three_quarters ? cccd : bbbc;
  // The first set of c's made it to the third position, from abcd. Correct it.
  unzipped2 = ripple_rotate_to_lower(cccc, two_quarters);
  ElType dddd = v < three_quarters ? ddda : cccd;
  // The first set of d's made it to the 4th position, from abcd. Correct it.
  unzipped3 = ripple_rotate_to_lower(dddd, three_quarters);
}

// _________________________ Load-and-unzip functions __________________________
//
//

template <typename ElType>
[[gnu::always_inline]] static void load_unzip_pairs(ElType *ptr, ElType &evens,
                                                    ElType &odds) {
  ElType *ptr2 = ptr + ripple_get_block_size(HVX_LANE, 0);
  unzip_pairs(*ptr, *ptr2, evens, odds);
}

template <typename ElType>
[[gnu::always_inline]] static void
load_unzip_triples(ElType *ptr, ElType &unzipped0, ElType &unzipped1,
                   ElType &unzipped2) {
  size_t nv = ripple_get_block_size(HVX_LANE, 0);
  ElType *ptr2 = ptr + nv;
  ElType *ptr3 = ptr2 + nv;
  unzip_triples(*ptr, *ptr2, *ptr3, unzipped0, unzipped1, unzipped2);
}

template <typename ElType>
[[gnu::always_inline]] static void
load_unzip_quads(ElType *ptr, ElType &unzipped0, ElType &unzipped1,
                 ElType &unzipped2, ElType &unzipped3) {
  size_t nv = ripple_get_block_size(HVX_LANE, 0);
  ElType *ptr2 = ptr + nv;
  ElType *ptr3 = ptr2 + nv;
  ElType *ptr4 = ptr3 + nv;
  unzip_triples(*ptr, *ptr2, *ptr3, *ptr4, unzipped0, unzipped1, unzipped2,
                unzipped3);
}
