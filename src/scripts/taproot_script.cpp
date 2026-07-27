//
//  taproot_script.cpp
//  blocksci_interface
//

#include <blocksci/core/address_types.hpp>
#include <blocksci/scripts/taproot_script.hpp>

#include "bitcoin_segwit_addr.hpp"

#include <range/v3/utility/optional.hpp>
#include <internal/address_info.hpp>
#include <internal/data_access.hpp>
#include <internal/script_access.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace blocksci {

  ScriptAddress<AddressType::WITNESS_TAPROOT>::ScriptAddress(uint32_t addressNum_, DataAccess &access_)
      : ScriptAddress(addressNum_, access_.getScripts().getScriptData<dedupType(addressType)>(addressNum_), access_) {
  }

  const uint256 &ScriptAddress<AddressType::WITNESS_TAPROOT>::getOutputKey() const {
    return getData()->outputKey;
  }

  std::string ScriptAddress<AddressType::WITNESS_TAPROOT>::getOutputKeyString() const {
    return getOutputKey().GetHexReverse();
  }

  std::string ScriptAddress<AddressType::WITNESS_TAPROOT>::addressString() const {
    auto outputKey = getOutputKey();
    std::vector<uint8_t> witprog{outputKey.begin(), outputKey.end()};
    return segwit_addr::encode(getAccess().config.chainConfig, 1, witprog);
  }

  ranges::optional<witness_stack::Stack> ScriptAddress<AddressType::WITNESS_TAPROOT>::getWitnessStack() const {
    if (rawInputData != nullptr) {
      const auto decoded = witness_stack::decode(rawInputData->scriptData.begin(), rawInputData->scriptData.size());
      if (const auto *stack = std::get_if<witness_stack::Stack>(&decoded)) {
        return *stack;
      }
      return ranges::nullopt;
    } else {
      return ranges::nullopt;
    }
  }

  std::string ScriptAddress<AddressType::WITNESS_TAPROOT>::toString() const {
    std::stringstream ss;
    ss << "TaprootAddress(" << addressString() << ")";
    return ss.str();
  }

  std::string ScriptAddress<AddressType::WITNESS_TAPROOT>::toPrettyString() const {
    std::stringstream ss;
    ss << "TaprootAddress(" << addressString() << ")";
    return ss.str();
  }
} // namespace blocksci
