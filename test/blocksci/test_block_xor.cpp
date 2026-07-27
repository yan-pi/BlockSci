#include <blocksci/core/block_xor.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

  using blocksci::block_xor::Key;

  TEST(BlockXor, ZeroKeyIsDisabled) {
    EXPECT_FALSE(blocksci::block_xor::isEnabled(Key{}));
  }

  TEST(BlockXor, NonzeroKeyIsEnabled) {
    EXPECT_TRUE(blocksci::block_xor::isEnabled(Key{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
  }

  TEST(BlockXor, UsesAbsoluteOffsetAcrossKeyBoundaries) {
    const Key key{0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87};
    const std::vector<uint8_t> plain{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33};
    constexpr uint64_t absoluteOffset = 5;

    std::vector<uint8_t> stored;
    stored.reserve(plain.size());
    for (std::size_t index = 0; index < plain.size(); ++index) {
      stored.push_back(blocksci::block_xor::apply(plain[index], key, absoluteOffset + index));
    }

    std::vector<uint8_t> decoded;
    decoded.reserve(stored.size());
    for (std::size_t index = 0; index < stored.size(); ++index) {
      decoded.push_back(blocksci::block_xor::apply(stored[index], key, absoluteOffset + index));
    }
    EXPECT_EQ(decoded, plain);
  }

  TEST(BlockXor, RepeatsKeyEveryEightBytes) {
    const Key key{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    EXPECT_EQ(blocksci::block_xor::apply(0xaa, key, 0), blocksci::block_xor::apply(0xaa, key, 8));
    EXPECT_EQ(blocksci::block_xor::apply(0xaa, key, 7), blocksci::block_xor::apply(0xaa, key, 15));
  }

} // namespace
