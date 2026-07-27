#include "taproot_proxy_py.hpp"

#include "generic_proxy.hpp"
#include "method_tags.hpp"
#include "proxy/basic.hpp"
#include "proxy/equality.hpp"
#include "proxy/optional.hpp"
#include "proxy/range.hpp"
#include "proxy_apply_py.hpp"
#include "proxy_py.hpp"
#include "scripts/address_py.hpp"
#include "taproot_py.hpp"

#include <blocksci/chain/block.hpp>
#include <blocksci/cluster/cluster.hpp>
#include <blocksci/scripts/scripts_fwd.hpp>
#include <blocksci/scripts/taproot_script.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/range_for.hpp>
#include <range/v3/utility/optional.hpp>
#include <range/v3/view/transform.hpp>

#include <pybind11/pytypes.h>

#include <string>
#include <vector>

using namespace blocksci;
namespace py = pybind11;

struct AddTaprootMethods {
  template <typename FuncApplication> void operator()(FuncApplication func) {
    func(property_tag, "address_string", &script::WitnessTaproot::addressString, "Bitcoin Taproot address string");
    func(property_tag, "output_key", &script::WitnessTaproot::getOutputKeyString, "Taproot x-only output key");
    func(
        property_tag, "witness_stack",
        +[](const script::WitnessTaproot &script) -> ranges::optional<py::list> {
          auto stack = script.getWitnessStack();
          if (stack) {
            py::list list;
            RANGES_FOR(auto &&item, *stack) {
              auto charVector =
                  item | ranges::views::transform([](auto &&c) -> char { return c; }) | ranges::to<std::vector>();
              list.append(py::bytes(std::string{charVector.begin(), charVector.end()}));
            }
            return list;
          } else {
            return ranges::nullopt;
          }
        },
        "Witness stack of spending Taproot input");
  }
};

void addTaprootProxyMethods(AllProxyClasses<script::WitnessTaproot, ProxyAddress> &cls) {
  cls.applyToAll(AddProxyMethods{});
  setupRangesProxy(cls);
  addProxyOptionalMethods(cls.optional);

  applyMethodsToProxy(cls.base, AddTaprootMethods{});
  addProxyEqualityMethods(cls.base);
}
