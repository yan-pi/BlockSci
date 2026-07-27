#include <blocksci/core/witness_stack.hpp>

#include <gtest/gtest.h>

#include <variant>
#include <vector>

namespace {

  using blocksci::witness_stack::Bytes;
  using blocksci::witness_stack::DecodeError;
  using blocksci::witness_stack::DecodeResult;
  using blocksci::witness_stack::Stack;

  void expectRoundTrip(const Stack &expected) {
    const auto encoded = blocksci::witness_stack::encode(expected);
    const auto decoded = blocksci::witness_stack::decode(encoded.data(), encoded.size());

    ASSERT_TRUE(std::holds_alternative<Stack>(decoded));
    EXPECT_EQ(std::get<Stack>(decoded), expected);
  }

  void expectDecodeError(const Bytes &encoded, DecodeError expected) {
    const auto decoded = blocksci::witness_stack::decode(encoded.data(), encoded.size());

    ASSERT_TRUE(std::holds_alternative<DecodeError>(decoded));
    EXPECT_EQ(std::get<DecodeError>(decoded), expected);
  }

  TEST(WitnessStackCodec, EncodesEmptyStack) {
    EXPECT_EQ(blocksci::witness_stack::encode(Stack{}), (Bytes{0x00}));
    expectRoundTrip(Stack{});
  }

  TEST(WitnessStackCodec, PreservesEmptyAndMultipleItems) {
    const Stack input{Bytes{}, Bytes{0x01, 0x02}, Bytes{}};

    EXPECT_EQ(blocksci::witness_stack::encode(input), (Bytes{0x03, 0x00, 0x02, 0x01, 0x02, 0x00}));
    expectRoundTrip(input);
  }

  TEST(WitnessStackCodec, PreservesEmbeddedFeBytes) {
    const Stack input{Bytes{0x00, 0xfe, 0xff}, Bytes{}, Bytes{0xfe, 0xfe}};

    EXPECT_EQ(blocksci::witness_stack::encode(input),
              (Bytes{0x03, 0x03, 0x00, 0xfe, 0xff, 0x00, 0x02, 0xfe, 0xfe}));
    expectRoundTrip(input);
  }

  TEST(WitnessStackCodec, PreservesSchnorrSignatureLengths) {
    const Stack input{Bytes(64, 0x11), Bytes(65, 0xfe)};

    expectRoundTrip(input);
  }

  TEST(WitnessStackCodec, PreservesCompactSizeBoundaries) {
    const Stack input{Bytes(252, 0x11), Bytes(253, 0x22), Bytes(65535, 0x33), Bytes(65536, 0x44)};

    expectRoundTrip(input);
  }

  TEST(WitnessStackCodec, RejectsTruncatedStackCount) {
    expectDecodeError(Bytes{}, DecodeError::TRUNCATED_STACK_COUNT);
  }

  TEST(WitnessStackCodec, RejectsTruncatedItemLength) {
    expectDecodeError(Bytes{0x01}, DecodeError::TRUNCATED_ITEM_LENGTH);
  }

  TEST(WitnessStackCodec, RejectsTruncatedItem) {
    expectDecodeError(Bytes{0x01, 0x03, 0xaa, 0xbb}, DecodeError::TRUNCATED_ITEM);
  }

  TEST(WitnessStackCodec, RejectsNonCanonicalCompactSize) {
    expectDecodeError(Bytes{0xfd, 0xfc, 0x00}, DecodeError::NON_CANONICAL_COMPACT_SIZE);
  }

  TEST(WitnessStackCodec, RejectsEveryTruncatedCompactSizeWidth) {
    expectDecodeError(Bytes{0xfd}, DecodeError::TRUNCATED_STACK_COUNT);
    expectDecodeError(Bytes{0xfd, 0x01}, DecodeError::TRUNCATED_STACK_COUNT);
    expectDecodeError(Bytes{0xfe, 0x01, 0x00, 0x00}, DecodeError::TRUNCATED_STACK_COUNT);
    expectDecodeError(Bytes{0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                      DecodeError::TRUNCATED_STACK_COUNT);
  }

  TEST(WitnessStackCodec, RejectsOversizedPersistedStack) {
    const Bytes oversized(4'000'001, 0x00);

    expectDecodeError(oversized, DecodeError::RESOURCE_LIMIT);
  }

  TEST(WitnessStackCodec, RejectsTrailingBytes) {
    expectDecodeError(Bytes{0x01, 0x00, 0x00}, DecodeError::TRAILING_BYTES);
  }

} // namespace
