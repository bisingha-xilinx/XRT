/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <iomanip>

#define XDP_CORE_SOURCE

#include "xdp/profile/database/events/device_events.h"
#include "xdp/profile/database/static_info_database.h"

namespace xdp {
  
  // **************************
  // Device event definitions
  // **************************

  VTFDeviceEvent::VTFDeviceEvent(uint64_t s_id, double ts, VTFEventType ty, uint64_t devId, uint32_t monId)
                : VTFEvent(s_id, ts, ty),
                  deviceId(devId),
                  deviceTimestamp(0),
                  monitorId(monId)
  {
  }

  VTFDeviceEvent::~VTFDeviceEvent()
  {
  }

  void VTFDeviceEvent::dumpTimestamp(std::ofstream& fout)
  {
    // Device events are accurate up to nanoseconds.
    // Timestamps are in milliseconds, so we should print up to 
    //  6 past the decimal point
    std::ios_base::fmtflags flags = fout.flags() ;
    fout << std::fixed << std::setprecision(6) << timestamp ;
    fout.flags(flags) ;
  }

  void VTFDeviceEvent::dump(std::ofstream& fout, uint32_t bucket)
  { 
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl;
  } 

  KernelEvent::KernelEvent(uint64_t s_id, double ts, VTFEventType ty,
                           uint64_t devId, uint32_t monId, int32_t cuIdx)
             : VTFDeviceEvent(s_id, ts, ty, devId, monId),
               cuId(cuIdx)
  {
  }

  KernelEvent::~KernelEvent()
  {
  }

  void KernelEvent::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    // Don't dump endline.  The writer will add tool tips for this event type
  }

  KernelStall::KernelStall(uint64_t s_id, double ts, VTFEventType ty,
                           uint64_t devId, uint32_t monId, int32_t cuIdx)
             : KernelEvent(s_id, ts, ty, devId, monId, cuIdx),
               // Until implemented, provide a default value for all members
               burstLength(0)
  {
  }

  KernelStall::~KernelStall()
  {
  }

  void KernelStall::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << std::endl;
  }

  DeviceMemoryAccess::DeviceMemoryAccess(uint64_t s_id, double ts, VTFEventType ty,
                                         uint64_t devId, uint32_t monId, int32_t cuIdx,
                                         uint64_t memStrId)
                    : VTFDeviceEvent(s_id, ts, ty, devId, monId),
                      cuId(cuIdx),
                      memoryName(memStrId),
                      // Until implemented, provide a default value for all members
                      portName(0), argumentNames(0), burstLength(0),
                      numBytes(0)
  {
  }

  DeviceMemoryAccess::~DeviceMemoryAccess()
  {
  }

  void DeviceMemoryAccess::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    fout << "," << memoryName << std::endl;
  }

  DeviceStreamAccess::DeviceStreamAccess(uint64_t s_id, double ts, VTFEventType ty,
                                         uint64_t devId, uint32_t monId, int32_t cuIdx)
                    : VTFDeviceEvent(s_id, ts, ty, devId, monId),
                      cuId(cuIdx),
                      // Until implemented, provide a default value for all members
                      portName(0), streamName(0), burstLength(0)
  {
  }

  DeviceStreamAccess::~DeviceStreamAccess()
  {
  }

  HostRead::HostRead(uint64_t s_id, double ts, uint64_t devId, uint32_t monId)
          : VTFDeviceEvent(s_id, ts, HOST_READ, devId, monId)
  {
  }

  HostRead::~HostRead()
  {
  }

  HostWrite::HostWrite(uint64_t s_id, double ts, uint64_t devId, uint32_t monId)
           : VTFDeviceEvent(s_id, ts, HOST_WRITE, devId, monId)
  {
  }

  HostWrite::~HostWrite()
  {
  }

  XclbinEnd::XclbinEnd(uint64_t s_id, double ts, uint64_t devId, uint32_t monId)
    : VTFDeviceEvent(s_id, ts, XCLBIN_END, devId, monId)
  {
    // Just a marker to know when an xclbin has been completely flushed
  }

  XclbinEnd::~XclbinEnd()
  {
  }

} // end namespace xdp
