//
//  safe_mem_reader.hpp
//  blocksci_parser
//
//  Created by Harry Kalodner on 9/26/17.
//

#ifndef safe_mem_reader_hpp
#define safe_mem_reader_hpp

#include <blocksci/core/block_xor.hpp>

#include <mio/mmap.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

inline blocksci::block_xor::Key readBlockXorKey(const std::string &path) {
  blocksci::block_xor::Key key{};
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return key;
  }
  stream.read(reinterpret_cast<char *>(key.data()), static_cast<std::streamsize>(key.size()));
  if (stream.gcount() != static_cast<std::streamsize>(key.size()) || stream.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error("Invalid Bitcoin Core block XOR key file: " + path);
  }
  return key;
}

inline unsigned int variableLengthIntSize(uint64_t nSize) {
  if (nSize < 253)
    return sizeof(unsigned char);
  else if (nSize <= std::numeric_limits<unsigned short>::max())
    return sizeof(unsigned char) + sizeof(unsigned short);
  else if (nSize <= std::numeric_limits<unsigned int>::max())
    return sizeof(unsigned char) + sizeof(unsigned int);
  else
    return sizeof(unsigned char) + sizeof(uint64_t);
}

class SafeMemReader {
public:
  using iterator = mio::mmap_source::const_iterator;
  using size_type = mio::mmap_source::size_type;
  using difference_type = mio::mmap_source::difference_type;
  using DecodedBuffer = std::vector<char>;
  using DecodedBufferPtr = std::shared_ptr<const DecodedBuffer>;

  explicit SafeMemReader(std::string path_, blocksci::block_xor::Key xorKey_ = {})
      : path(std::move(path_)), xorKey(xorKey_) {
    std::error_code error;
    fileMap.map(path, 0, mio::map_entire_file, error);
    if (error) {
      throw error;
    }

    begin = fileMap.begin();
    end = fileMap.end();
    pos = begin;
  }

  std::string getPath() const {
    return path;
  }

  bool has(difference_type n) {
    return n <= std::distance(pos, end);
  }

  template <typename Type> Type readNext() {
    auto val = peakNext<Type>();
    pos += sizeof(val);
    return val;
  }

  template <typename Type> Type peakNext() {
    constexpr auto size = sizeof(Type);
    if (!has(size)) {
      throw std::out_of_range("Tried to read past end of file");
    }
    Type val;
    if (!blocksci::block_xor::isEnabled(xorKey)) {
      memcpy(&val, pos, size);
      return val;
    }
    auto *output = reinterpret_cast<unsigned char *>(&val);
    const auto startOffset = static_cast<uint64_t>(offset());
    for (size_type index = 0; index < size; ++index) {
      output[index] = blocksci::block_xor::apply(static_cast<uint8_t>(pos[index]), xorKey, startOffset + index);
    }
    return val;
  }

  // reads a variable length integer.
  // See the documentation from here:  https://en.bitcoin.it/wiki/Protocol_specification#Variable_length_integer
  uint32_t readVariableLengthInteger() {
    auto v = readNext<uint8_t>();
    if (v < 0xFD) { // If it's less than 0xFD use this value as the unsigned integer
      return static_cast<uint32_t>(v);
    } else if (v == 0xFD) {
      return static_cast<uint32_t>(readNext<uint16_t>());
    } else if (v == 0xFE) {
      return readNext<uint32_t>();
    } else {
      return static_cast<uint32_t>(readNext<uint64_t>()); // TODO: maybe we should not support this here, we lose data
    }
  }

  void advance(difference_type n) {
    if (!has(n)) {
      throw std::out_of_range("Tried to advance past end of file");
    }
    pos += n;
  }

  void reset() {
    pos = begin;
  }

  void reset(difference_type n) {
    if (begin + n > end) {
      throw std::out_of_range("Tried to reset out of file");
    }
    pos = begin + n;
  }

  difference_type offset() {
    return std::distance(begin, pos);
  }

  const char *unsafePos() {
    if (!blocksci::block_xor::isEnabled(xorKey)) {
      return pos;
    }
    const auto currentOffset = offset();
    if (decodedBuffer == nullptr || currentOffset < decodedStart ||
        currentOffset > decodedStart + static_cast<difference_type>(decodedBuffer->size())) {
      throw std::logic_error("XOR-decoded pointer requested outside the active block range");
    }
    return decodedBuffer->data() + (currentOffset - decodedStart);
  }

  DecodedBufferPtr decodeRange(difference_type start, size_type length) const {
    if (start < 0 || start > std::distance(begin, end) ||
        length > static_cast<size_type>(std::distance(begin + start, end))) {
      throw std::out_of_range("Tried to decode past end of file");
    }
    auto decoded = std::make_shared<DecodedBuffer>(length);
    for (size_type index = 0; index < length; ++index) {
      (*decoded)[index] = static_cast<char>(blocksci::block_xor::apply(
          static_cast<uint8_t>(begin[start + static_cast<difference_type>(index)]), xorKey,
          static_cast<uint64_t>(start) + index));
    }
    return decoded;
  }

  void activateDecodedRange(difference_type start, size_type length) {
    decodedBuffer = decodeRange(start, length);
    decodedStart = start;
  }

  DecodedBufferPtr activeDecodedRange() const {
    return decodedBuffer;
  }

  bool xorEnabled() const {
    return blocksci::block_xor::isEnabled(xorKey);
  }

protected:
  mio::mmap_source fileMap;
  std::string path;
  iterator pos;
  iterator begin;
  iterator end;
  blocksci::block_xor::Key xorKey;
  DecodedBufferPtr decodedBuffer;
  difference_type decodedStart = 0;
};

#endif /* safe_mem_reader_hpp */
