//
//  taproot_script.hpp
//  blocksci
//

#ifndef taproot_script_hpp
#define taproot_script_hpp

#include "script.hpp"

#include <blocksci/blocksci_export.h>
#include <blocksci/core/bitcoin_uint256.hpp>
#include <blocksci/core/hash_combine.hpp>
#include <blocksci/core/witness_stack.hpp>

#include <range/v3/utility/optional.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>

namespace blocksci {
  template <> class BLOCKSCI_EXPORT ScriptAddress<AddressType::WITNESS_TAPROOT> : public ScriptBase {
    const TaprootSpendScriptData *rawInputData;

    const TaprootScriptData *getData() const {
      return reinterpret_cast<const TaprootScriptData *>(ScriptBase::getData());
    }

  public:
    constexpr static AddressType::Enum addressType = AddressType::WITNESS_TAPROOT;

    ScriptAddress() = default;
    ScriptAddress(uint32_t scriptNum_, std::tuple<const TaprootScriptData *, const TaprootSpendScriptData *> &&rawData_,
                  DataAccess &access_)
        : ScriptBase(scriptNum_, addressType, access_, std::get<0>(rawData_)), rawInputData(std::get<1>(rawData_)) {
    }
    ScriptAddress(uint32_t addressNum_, DataAccess &access_);

    const uint256 &getOutputKey() const;
    std::string getOutputKeyString() const;
    std::string addressString() const;
    ranges::optional<witness_stack::Stack> getWitnessStack() const;

    std::string toString() const;
    std::string toPrettyString() const;
  };
} // namespace blocksci

namespace std {
  template <> struct BLOCKSCI_EXPORT hash<blocksci::ScriptAddress<blocksci::AddressType::WITNESS_TAPROOT>> {
    size_t operator()(const blocksci::ScriptAddress<blocksci::AddressType::WITNESS_TAPROOT> &address) const {
      std::size_t seed = 32847957;
      blocksci::hash_combine(seed, static_cast<const blocksci::ScriptBase &>(address));
      return seed;
    }
  };
} // namespace std

#endif /* taproot_script_hpp */
