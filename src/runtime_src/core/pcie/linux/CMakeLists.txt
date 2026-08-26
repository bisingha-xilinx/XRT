# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
# Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

# Linux shim is part of the base component.  It is used by both
# Alveo and NPU components.
if (NOT XRT_BASE)
  return()
endif()

add_subdirectory(plugin/xdp)

add_library(core_pcielinux_objects OBJECT
  debug.cpp
  device_linux.cpp
  pcidev.cpp
  pcidrv.cpp
  shim.cpp
  system_linux.cpp
  )

set(NPU_ZOCL_OBJECTS)
if (XRT_NPU_ZOCL)
  set(XRT_EDGE_ROOT ${XRT_SOURCE_DIR}/runtime_src/core/edge)
  add_library(core_npu_zocl_objects OBJECT
    ${XRT_EDGE_ROOT}/common/aie_parser.cpp
    ${XRT_EDGE_ROOT}/common/device_edge.cpp
    ${XRT_EDGE_ROOT}/user/aie_sys_parser.cpp
    ${XRT_EDGE_ROOT}/user/dev.cpp
    ${XRT_EDGE_ROOT}/user/dev_zocl.cpp
    ${XRT_EDGE_ROOT}/user/device_linux.cpp
    ${XRT_EDGE_ROOT}/user/drv_zocl.cpp
    ${XRT_EDGE_ROOT}/user/hwctx_object.cpp
    ${XRT_EDGE_ROOT}/user/shim.cpp
    ${XRT_EDGE_ROOT}/user/aie/aie.cpp
    ${XRT_EDGE_ROOT}/user/aie/aie_buffer_object.cpp
    ${XRT_EDGE_ROOT}/user/aie/aied.cpp
    ${XRT_EDGE_ROOT}/user/aie/graph_object.cpp
    ${XRT_EDGE_ROOT}/user/aie/profile_object.cpp
    ${XRT_EDGE_ROOT}/user/aie/common_layer/adf_api_message.cpp
    ${XRT_EDGE_ROOT}/user/aie/common_layer/adf_profiling_api.cpp
    ${XRT_EDGE_ROOT}/user/aie/common_layer/adf_runtime_api.cpp
    ${XRT_EDGE_ROOT}/user/aie/common_layer/fal_util.cpp
    )

  target_compile_definitions(core_npu_zocl_objects
    PRIVATE
    XRT_NPU_ZOCL
    XRT_ENABLE_AIE
    XCLHAL_MAJOR_VER=2
    XCLHAL_MINOR_VER=1
    )

  target_include_directories(core_npu_zocl_objects
    PRIVATE
    ${XRT_EDGE_ROOT}/user
    ${DRM_INCLUDE_DIRS}
    ${XRT_SOURCE_DIR}/runtime_src
    ${XRT_BINARY_DIR}/gen
    )

  set(NPU_ZOCL_OBJECTS $<TARGET_OBJECTS:core_npu_zocl_objects>)
  target_compile_definitions(core_pcielinux_objects PRIVATE XRT_NPU_ZOCL)
endif()

target_compile_definitions(core_pcielinux_objects
  PRIVATE
  XCLHAL_MAJOR_VER=2
  XCLHAL_MINOR_VER=1
  )

target_include_directories(core_pcielinux_objects
  PRIVATE
  ${XRT_SOURCE_DIR}/runtime_src
  ${XRT_BINARY_DIR}/gen
  )


add_library(xrt_core SHARED
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_smi_alveo_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  ${NPU_ZOCL_OBJECTS}
  )

add_library(xrt_core_static STATIC
  $<TARGET_OBJECTS:core_pcielinux_plugin_xdp_objects>
  $<TARGET_OBJECTS:core_pcielinux_objects>
  $<TARGET_OBJECTS:core_smi_alveo_objects>
  $<TARGET_OBJECTS:core_pciecommon_objects>
  $<TARGET_OBJECTS:core_common_objects>
  ${NPU_ZOCL_OBJECTS}
  )

set_target_properties(xrt_core PROPERTIES
  VERSION ${XRT_VERSION_STRING}
  SOVERSION ${XRT_SOVERSION})

# Private dependencies for fully resolved dynamic xrt_core
target_link_libraries(xrt_core
  PRIVATE
  xrt_coreutil
  pthread
  rt
  dl
  uuid
  )

if (XRT_NPU_ZOCL)
  # Link flag rather than the target name: an in-tree aie-rt target named
  # xaiengine exists on the NPU path, but its headers are the NPU variant and
  # are incompatible with the xaiefal headers the edge AIE sources compile
  # against.  Both must come from the same installed libxaiengine.
  target_link_libraries(xrt_core PRIVATE -lxaiengine)
endif()

# Targets linking with xrt_core_static must also link with additional
# libraries to satisfy xrt_core_static dependencies. These type of
# link dependencies are known as INTERFACE dependencies.  Here the
# boost libraries are specified by their system name so that static of
# target can pick static link libraries of boost
target_link_libraries(xrt_core_static
  INTERFACE
  xrt_coreutil_static
  uuid
  dl
  rt
  pthread
  )

if (XRT_NPU_ZOCL)
  target_link_libraries(xrt_core_static INTERFACE -lxaiengine)
endif()

if (NOT WIN32)
  # On linux use xrt_core.a
  set_target_properties(xrt_core_static PROPERTIES OUTPUT_NAME "xrt_core")
endif()

# Shim for Linux is installed in the base component as it is
# used by both Alveo and NPU components.   
install(TARGETS xrt_core
  EXPORT xrt-targets
  RUNTIME DESTINATION ${XRT_INSTALL_BIN_DIR} COMPONENT ${XRT_BASE_COMPONENT}
  LIBRARY DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_COMPONENT} NAMELINK_COMPONENT ${XRT_BASE_DEV_COMPONENT}
  ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT}
)

# Enable install of static library iff requested
if(XRT_INSTALL_STATIC_LIBRARY)
  install(TARGETS xrt_core_static
    EXPORT xrt-targets
    ARCHIVE DESTINATION ${XRT_INSTALL_LIB_DIR} COMPONENT ${XRT_BASE_DEV_COMPONENT})
endif()
