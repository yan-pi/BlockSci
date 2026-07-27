#ifndef blocksci_taproot_py_h
#define blocksci_taproot_py_h

#include "python_range.hpp"

#include <blocksci/scripts/scripts_fwd.hpp>

void init_taproot(pybind11::class_<blocksci::script::WitnessTaproot> &cl);
void addTaprootRangeMethods(RangeClasses<blocksci::script::WitnessTaproot> &classes);

#endif /* blocksci_taproot_py_h */
