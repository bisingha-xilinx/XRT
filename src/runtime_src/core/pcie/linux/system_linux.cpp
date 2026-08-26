// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include files
#include "device_linux.h"
#include "system_linux.h"
#include "pcidev.h"
#include "pcidrv_xclmgmt.h"
#include "pcidrv_xocl.h"
#include "core/common/module_loader.h"
#include "core/common/query_requests.h"
#ifdef XRT_NPU_ZOCL
# include "npu_zocl.h"
# include "core/edge/user/dev.h"
# include "core/edge/user/drv_zocl.h"
#endif

// 3rd Party Library - Include files
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <gnu/libc-version.h>
#include <sys/utsname.h>

// Local - Include files
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

namespace driver_list {

static std::vector<std::shared_ptr<xrt_core::pci::drv>> list;

void
append(std::shared_ptr<xrt_core::pci::drv> driver)
{
  list.push_back(std::move(driver));
}

const std::vector<std::shared_ptr<xrt_core::pci::drv>>&
get()
{
  return list;
}

}

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
static xrt_core::system_linux*
singleton_system_linux()
{
  static xrt_core::system_linux singleton;
  return &singleton;
}

// Dynamic linking automatically constructs the singleton
struct X
{
  X() { singleton_system_linux(); }
} x;

static boost::property_tree::ptree
driver_version(const std::string& driver)
{
  boost::property_tree::ptree _pt;
  std::string ver("unknown");
  std::string hash("unknown");
  std::string path("/sys/module/");
  path += driver;
  path += "/version";

  std::ifstream stream(path);
  if (stream.is_open()) {
    std::string line;
    getline(stream, line);
    std::stringstream ss(line);
    getline(ss, ver, ',');
    getline(ss, hash, ',');
  }

  if (!((boost::iequals(driver, "xclmgmt") || boost::iequals(driver, "xocl")) && (boost::iequals(ver, "unknown")))) {
    _pt.put("name", driver);
    _pt.put("version", ver);
    _pt.put("hash", hash);
  }
  return _pt;
}

}

namespace xrt_core {

std::shared_ptr<pci::dev>
system_linux::
get_pcidev(unsigned index, bool is_user) const
{
  if (is_user) {
    if (index < user_ready_list.size())
      return user_ready_list[index];

    if ((index - user_ready_list.size()) < user_nonready_list.size())
      return user_nonready_list.at(index - user_ready_list.size());
  }
  else {
    if (index < mgmt_ready_list.size())
      return mgmt_ready_list[index];

    if ((index - mgmt_ready_list.size()) < mgmt_nonready_list.size())
      return mgmt_nonready_list.at(index - mgmt_ready_list.size());
  }

  // given index is not present in list
  throw std::runtime_error(" No such device with index '"+ std::to_string(index) + "'");
}

size_t
system_linux::
get_num_dev_ready(bool is_user) const
{
  if (is_user)
    return user_ready_list.size();

  return mgmt_ready_list.size();
}

size_t
system_linux::
get_num_dev_total(bool is_user) const
{
  if (is_user)
    return user_ready_list.size() + user_nonready_list.size();

  return mgmt_ready_list.size() + mgmt_nonready_list.size();
}

#ifdef XRT_NPU_ZOCL
bool
system_linux::
is_zocl_device(device::id_type id) const
{
  const auto zocl_begin = static_cast<device::id_type>(user_ready_list.size());
  const auto zocl_end = zocl_begin + static_cast<device::id_type>(zocl_dev_list.size());
  return id >= zocl_begin && id < zocl_end;
}

std::shared_ptr<edge::dev>
system_linux::
get_zocl_dev(device::id_type id) const
{
  if (!is_zocl_device(id))
    throw std::runtime_error("No ZOCL device with index '" + std::to_string(id) + "'");

  return zocl_dev_list.at(id - user_ready_list.size());
}
#endif

system_linux::
system_linux()
{
  // Add built-in driver to the list.
  driver_list::append(std::make_shared<pci::drv_xocl>());
  driver_list::append(std::make_shared<pci::drv_xclmgmt>());

  // Load driver plug-ins. Driver list will be updated during loading.
  // Don't need to die on a plug-in loading failure.
  try {
    xrt_core::driver_loader plugins;
  }
  catch (const std::runtime_error& err) {
    xrt_core::send_exception_message(err.what(), "WARNING");
  }

  for (const auto& driver : driver_list::get()) {
    if (driver->is_user())
      driver->scan_devices(user_ready_list, user_nonready_list);
    else
      driver->scan_devices(mgmt_ready_list, mgmt_nonready_list);
  }

#ifdef XRT_NPU_ZOCL
  try {
    edge::drv_zocl{}.scan_devices(zocl_dev_list);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what(), "WARNING");
  }
#endif
}

void
system_linux::
get_driver_info(boost::property_tree::ptree &pt)
{
  boost::property_tree::ptree _ptDriverInfo;

  for (const auto& drv : driver_list::get()) {
    boost::property_tree::ptree _drv = driver_version(drv->name());
    if (!_drv.empty())
      _ptDriverInfo.push_back( {"", _drv} );
  }
#ifdef XRT_NPU_ZOCL
  if (!zocl_dev_list.empty())
    _ptDriverInfo.push_back({"", driver_version("zocl")});
#endif
  pt.put_child("drivers", _ptDriverInfo);
}

device::id_type
system_linux::
get_device_id(const std::string& bdf) const
{
  // Treat non bdf as device index
  if (bdf.find_first_not_of("0123456789") == std::string::npos)
    return system::get_device_id(bdf);

  try {
    for (unsigned int i = 0;; i++) {
      auto dev = get_pcidev(i);
      auto unified_id = i;
#ifdef XRT_NPU_ZOCL
      if (i >= user_ready_list.size())
        unified_id += zocl_dev_list.size();
#endif
      // [dddd:bb:dd.f]
      auto dev_bdf = boost::str(boost::format("%04x:%02x:%02x.%01x") % dev->m_domain % dev->m_bus % dev->m_dev % dev->m_func);
      if (dev_bdf == bdf)
        return unified_id;
      // consider default domain as 0000 and try to find a matching device
      if (dev->m_domain == 0) {
        dev_bdf = boost::str(boost::format("%02x:%02x.%01x") % dev->m_bus % dev->m_dev % dev->m_func);
        if (dev_bdf == bdf)
          return unified_id;
      }
    }
  }
  catch (...) {
    throw xrt_core::system_error(EINVAL, "No such device '" + bdf + "'");
  }
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices(bool is_user) const
{
#ifdef XRT_NPU_ZOCL
  if (is_user) {
    const auto zocl_count = static_cast<device::id_type>(zocl_dev_list.size());
    return std::make_pair(
      static_cast<device::id_type>(get_num_dev_total(true)) + zocl_count,
      static_cast<device::id_type>(get_num_dev_ready(true)) + zocl_count);
  }
#endif
  return std::make_pair(pci::get_dev_total(is_user), pci::get_dev_ready(is_user));
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
system_linux::
get_bdf_info(device::id_type id, bool is_user) const
{
#ifdef XRT_NPU_ZOCL
  if (is_user && is_zocl_device(id))
    return std::make_tuple(0, 0, 0, static_cast<uint16_t>(id));

  if (is_user && id >= user_ready_list.size() + zocl_dev_list.size())
    id -= zocl_dev_list.size();
#endif
  auto pdev = get_pcidev(id, is_user);
  return std::make_tuple(pdev->m_domain, pdev->m_bus, pdev->m_dev, pdev->m_func);
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::id_type id) const
{
#ifdef XRT_NPU_ZOCL
  if (is_zocl_device(id)) {
    auto zdev = get_zocl_dev(id);
    return xrt_core::get_userpf_device(zdev->create_shim(id));
  }

  if (id >= user_ready_list.size() + zocl_dev_list.size())
    id -= zocl_dev_list.size();
#endif
  auto pdev = get_pcidev(id, true);
  return xrt_core::get_userpf_device(pdev->create_shim(id));
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
#ifdef XRT_NPU_ZOCL
  if (is_zocl_device(id))
    return get_zocl_dev(id)->create_device(handle, id);

  if (id >= user_ready_list.size() + zocl_dev_list.size())
    id -= zocl_dev_list.size();
#endif
  auto pdev = get_pcidev(id, true);
  return pdev->create_device(handle, id);
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  auto pdev = get_pcidev(id, false);
  return pdev->create_device(nullptr, id);
}

void
system_linux::
program_plp(const device* dev, const std::vector<char> &buffer, bool force) const
{
  try {
    xrt_core::scope_value_guard<int, std::function<void()>> fd = dev->file_open("icap", O_WRONLY);
    auto ret = write(fd.get(), buffer.data(), buffer.size());
    if (static_cast<size_t>(ret) != buffer.size())
      throw xrt_core::error(EINVAL, "Write plp to icap subdev failed");

  } catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), "XBMGMT");
  }

  auto value = xrt_core::query::rp_program_status::value_type(1);
  xrt_core::device_update<xrt_core::query::rp_program_status>(dev, value);

  // asynchronously check if the download is complete
  const static int program_timeout_sec = 60;
  bool is_complete = false;
  int retry_count = 0;
  while (!is_complete && retry_count++ < program_timeout_sec) {
    is_complete = xrt_core::query::rp_program_status::to_bool(xrt_core::device_query<xrt_core::query::rp_program_status>(dev));
    if (retry_count == program_timeout_sec)
      throw xrt_core::error(ETIMEDOUT, "PLP programmming timed out");

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

namespace pci {

std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id)
{
  singleton_system_linux(); // force loading if necessary
  return xrt_core::get_userpf_device(device_handle, id);
}

device::id_type
get_device_id_from_bdf(const std::string& bdf)
{
  singleton_system_linux(); // force loading if necessary
  return xrt_core::get_device_id(bdf);
}

size_t
get_dev_ready(bool user)
{
  return singleton_system_linux()->get_num_dev_ready(user);
}

size_t
get_dev_total(bool user)
{
  return singleton_system_linux()->get_num_dev_total(user);
}

std::shared_ptr<dev>
get_dev(unsigned index, bool user)
{
  return singleton_system_linux()->get_pcidev(index, user);
}

void
register_driver(std::shared_ptr<drv> driver)
{
  driver_list::append(std::move(driver));
}

} // namespace pci

#ifdef XRT_NPU_ZOCL
namespace npu_zocl {

std::shared_ptr<edge::dev>
get_dev(device::id_type id)
{
  return singleton_system_linux()->get_zocl_dev(id);
}

} // namespace npu_zocl
#endif

} // xrt_core
