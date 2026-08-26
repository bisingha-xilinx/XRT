// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XRT_CORE_PCIE_LINUX_NPU_ZOCL_H
#define XRT_CORE_PCIE_LINUX_NPU_ZOCL_H

#include "core/common/device.h"

#include <memory>

namespace xrt_core::edge {
class dev;
}

namespace xrt_core::npu_zocl {

std::shared_ptr<edge::dev>
get_dev(device::id_type id);

} // namespace xrt_core::npu_zocl

#endif
