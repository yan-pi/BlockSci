//
//  witness_program.hpp
//  blocksci
//

#ifndef witness_program_hpp
#define witness_program_hpp

#include <blocksci/core/address_types.hpp>

#include <cstddef>
#include <cstdint>

namespace blocksci {

  /** Classify a syntactically valid witness program by version and program length. */
  constexpr AddressType::Enum classifyWitnessProgram(uint8_t version, std::size_t programSize) {
    if (version == 0U) {
      if (programSize == 20U) {
        return AddressType::WITNESS_PUBKEYHASH;
      }
      if (programSize == 32U) {
        return AddressType::WITNESS_SCRIPTHASH;
      }
      return AddressType::NONSTANDARD;
    }
    if (version == 1U && programSize == 32U) {
      return AddressType::WITNESS_TAPROOT;
    }
    return AddressType::WITNESS_UNKNOWN;
  }

} // namespace blocksci

#endif /* witness_program_hpp */
