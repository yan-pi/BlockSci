//
//  witness_stack.hpp
//  blocksci
//

#ifndef witness_stack_hpp
#define witness_stack_hpp

#include <blocksci/blocksci_export.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <variant>
#include <vector>

namespace blocksci::witness_stack {

  using Bytes = std::vector<unsigned char>;
  using Stack = std::vector<Bytes>;

  /** Describes why persisted witness-stack bytes could not be decoded. */
  enum class DecodeError {
    TRUNCATED_STACK_COUNT,
    TRUNCATED_ITEM_LENGTH,
    TRUNCATED_ITEM,
    NON_CANONICAL_COMPACT_SIZE,
    TRAILING_BYTES,
    RESOURCE_LIMIT,
  };

  using DecodeResult = std::variant<Stack, DecodeError>;

  namespace detail {
    inline void appendLittleEndian(Bytes &output, uint64_t value, std::size_t width) {
      for (std::size_t index = 0; index < width; ++index) {
        output.push_back(static_cast<unsigned char>((value >> (index * 8U)) & 0xffU));
      }
    }

    inline void appendCompactSize(Bytes &output, uint64_t value) {
      if (value < 0xfdU) {
        output.push_back(static_cast<unsigned char>(value));
      } else if (value <= std::numeric_limits<uint16_t>::max()) {
        output.push_back(0xfdU);
        appendLittleEndian(output, value, sizeof(uint16_t));
      } else if (value <= std::numeric_limits<uint32_t>::max()) {
        output.push_back(0xfeU);
        appendLittleEndian(output, value, sizeof(uint32_t));
      } else {
        output.push_back(0xffU);
        appendLittleEndian(output, value, sizeof(uint64_t));
      }
    }

    class Reader {
    public:
      Reader(const unsigned char *data_, std::size_t size_) : data(data_), size(size_) {
      }

      bool readByte(unsigned char &value) {
        if (position >= size) {
          return false;
        }
        value = data[position++];
        return true;
      }

      bool readLittleEndian(std::size_t width, uint64_t &value) {
        if (width > remaining()) {
          return false;
        }
        value = 0;
        for (std::size_t index = 0; index < width; ++index) {
          value |= static_cast<uint64_t>(data[position + index]) << (index * 8U);
        }
        position += width;
        return true;
      }

      bool readBytes(std::size_t length, Bytes &value) {
        if (length > remaining()) {
          return false;
        }
        value.assign(data + position, data + position + length);
        position += length;
        return true;
      }

      std::size_t remaining() const {
        return size - position;
      }

    private:
      const unsigned char *data;
      std::size_t size;
      std::size_t position = 0;
    };

    enum class CompactSizeError { NONE, TRUNCATED, NON_CANONICAL };

    inline CompactSizeError readCompactSize(Reader &reader, uint64_t &value) {
      unsigned char prefix = 0;
      if (!reader.readByte(prefix)) {
        return CompactSizeError::TRUNCATED;
      }
      if (prefix < 0xfdU) {
        value = prefix;
        return CompactSizeError::NONE;
      }

      const auto width = prefix == 0xfdU ? sizeof(uint16_t) : prefix == 0xfeU ? sizeof(uint32_t) : sizeof(uint64_t);
      if (!reader.readLittleEndian(width, value)) {
        return CompactSizeError::TRUNCATED;
      }
      if ((width == sizeof(uint16_t) && value < 0xfdU) ||
          (width == sizeof(uint32_t) && value <= std::numeric_limits<uint16_t>::max()) ||
          (width == sizeof(uint64_t) && value <= std::numeric_limits<uint32_t>::max())) {
        return CompactSizeError::NON_CANONICAL;
      }
      return CompactSizeError::NONE;
    }
  } // namespace detail

  /** Encode a witness stack using CompactSize item counts and lengths. */
  inline Bytes encode(const Stack &stack) {
    Bytes output;
    detail::appendCompactSize(output, stack.size());
    for (const auto &item : stack) {
      detail::appendCompactSize(output, item.size());
      output.insert(output.end(), item.begin(), item.end());
    }
    return output;
  }

  /** Decode a complete length-prefixed witness stack without losing item boundaries. */
  inline DecodeResult decode(const unsigned char *data, std::size_t size) {
    constexpr std::size_t maxEncodedWitnessSize = 4'000'000;
    constexpr uint64_t maxWitnessItems = 1'000'000;
    if (size > maxEncodedWitnessSize) {
      return DecodeError::RESOURCE_LIMIT;
    }
    if (data == nullptr && size != 0U) {
      return DecodeError::TRUNCATED_STACK_COUNT;
    }

    detail::Reader reader{data, size};
    uint64_t itemCount = 0;
    const auto countResult = detail::readCompactSize(reader, itemCount);
    if (countResult == detail::CompactSizeError::TRUNCATED) {
      return DecodeError::TRUNCATED_STACK_COUNT;
    }
    if (countResult == detail::CompactSizeError::NON_CANONICAL) {
      return DecodeError::NON_CANONICAL_COMPACT_SIZE;
    }
    if (itemCount > maxWitnessItems) {
      return DecodeError::RESOURCE_LIMIT;
    }
    if (itemCount > reader.remaining()) {
      return DecodeError::TRUNCATED_ITEM_LENGTH;
    }

    Stack stack;
    stack.reserve(static_cast<std::size_t>(itemCount));
    for (uint64_t index = 0; index < itemCount; ++index) {
      uint64_t itemLength = 0;
      const auto lengthResult = detail::readCompactSize(reader, itemLength);
      if (lengthResult == detail::CompactSizeError::TRUNCATED) {
        return DecodeError::TRUNCATED_ITEM_LENGTH;
      }
      if (lengthResult == detail::CompactSizeError::NON_CANONICAL) {
        return DecodeError::NON_CANONICAL_COMPACT_SIZE;
      }
      if (itemLength > reader.remaining()) {
        return DecodeError::TRUNCATED_ITEM;
      }

      Bytes item;
      if (!reader.readBytes(static_cast<std::size_t>(itemLength), item)) {
        return DecodeError::TRUNCATED_ITEM;
      }
      stack.push_back(std::move(item));
    }

    if (reader.remaining() != 0U) {
      return DecodeError::TRAILING_BYTES;
    }
    return stack;
  }

} // namespace blocksci::witness_stack

#endif /* witness_stack_hpp */
