//
//  block_xor.hpp
//  blocksci
//

#ifndef block_xor_hpp
#define block_xor_hpp

#include <array>
#include <cstdint>

namespace blocksci::block_xor {

  /** Bitcoin Core's repeating block-file obfuscation key. */
  using Key = std::array<uint8_t, 8>;

  /** Return whether a block-file key performs nontrivial obfuscation. */
  constexpr bool isEnabled(const Key &key) {
    for (const auto byte : key) {
      if (byte != 0U) {
        return true;
      }
    }
    return false;
  }

  /** Apply Bitcoin Core's symmetric XOR transform at an absolute file offset. */
  constexpr uint8_t apply(uint8_t byte, const Key &key, uint64_t absoluteOffset) {
    return static_cast<uint8_t>(byte ^ key[absoluteOffset % key.size()]);
  }

} // namespace blocksci::block_xor

#endif /* block_xor_hpp */
