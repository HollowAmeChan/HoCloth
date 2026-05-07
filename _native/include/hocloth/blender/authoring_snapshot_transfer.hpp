#pragma once

#include "hocloth_runtime_api.hpp"

#include <nanobind/nanobind.h>

namespace hocloth::blender {

CompiledScene ParseAuthoringSnapshot(const nanobind::dict& root);

}  // namespace hocloth::blender
