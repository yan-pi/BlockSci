#ifndef taproot_proxy_py_h
#define taproot_proxy_py_h

#include "python_fwd.hpp"

#include <blocksci/scripts/scripts_fwd.hpp>

void addTaprootProxyMethods(AllProxyClasses<blocksci::script::WitnessTaproot, ProxyAddress> &cls);

#endif /* taproot_proxy_py_h */
