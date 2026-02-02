//===----------------------------------------------------------------------===//
//
// (c) 2025-2026 Qualcomm Technologies, Inc. All rights reserved.
//
// See https://spdx.org/licenses/BSD-3-Clause-Clear.html for license information.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//
//===----------------------------------------------------------------------===//

#include <cstdint>
#include <map>
#include <memory>
#include <stack>
#include <stdexcept>
#include <stdint.h>
#include <utility>

class Mempool {
public:
  Mempool() : nbytes_to_avlbl_buffers(), buffer_to_nbytes(), allocator() {}

  intptr_t allocate(size_t nbytes) {
    if (nbytes_to_avlbl_buffers.find(nbytes) == nbytes_to_avlbl_buffers.end())
      _allocate_and_register_buffer(n);

    if (nbytes_to_avlbl_buffers.at(n).empty())
      _allocate_and_register_buffer(nbytes);

    intptr_t ptr = nbytes_to_avlbl_buffers.at(nbytes).top();
    nbytes_to_avlbl_buffers.at(nbytes).pop();
    return ptr;
  }

  void deallocate(intptr_t ptr) {
    auto it = buffer_to_nbytes.find(ptr);
    if (it == buffer_to_nbytes.end()) {
      throw std::invalid_argument("Pointer is not managed by memory pool.");
    }

    // Reclaim pointer.
    nbytes_to_avlbl_buffers[it->second].push(ptr);
  }

private:
  std::map<size_t, std::stack<intptr_t>> nbytes_to_avlbl_buffers;
  std::map<intptr_t, size_t> buffer_to_nbytes;
  std::allocator<int8_t> allocator;

  /**
   * @brief Allocates and register a buffer of size "nbytes" to the memory pool.
   *
   * @param nbytes The size of the memory that
   */
  void _allocate_and_register_buffer(size_t nbytes) {
    if (nbytes_to_avlbl_buffers.find(nbytes) == nbytes_to_avlbl_buffers.end()) {
      std::stack<intptr_t> q;
      nbytes_to_avlbl_buffers.insert(std::pair(nbytes, q));
    }

    intptr_t ptr = (intptr_t)allocator.allocate(nbytes);

    nbytes_to_avlbl_buffers.at(nbytes).push(ptr);
    buffer_to_nbytes.insert(std::pair(ptr, nbytes));
  }
};
