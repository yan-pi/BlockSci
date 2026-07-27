#include "taproot_py.hpp"

#include "caster_py.hpp"
#include "python_range.hpp"
#include "ranges_py.hpp"

#include <blocksci/scripts/scripts_fwd.hpp>
#include <blocksci/scripts/taproot_script.hpp>

#include <pybind11/pybind11.h>

using namespace blocksci;
namespace py = pybind11;

void init_taproot(py::class_<script::WitnessTaproot> &cl) {
  cl.def("__repr__", &script::WitnessTaproot::toString).def("__str__", &script::WitnessTaproot::toPrettyString);
}

void addTaprootRangeMethods(RangeClasses<script::WitnessTaproot> &classes) {
  addAllRangeMethods(classes);
}
