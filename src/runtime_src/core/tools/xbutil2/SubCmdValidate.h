// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmdValidate_h_
#define __SubCmdValidate_h_

#include <boost/program_options.hpp>
#include "tools/common/SubCmd.h"
#include "tools/common/XBHelpMenus.h"

class SubCmdValidate : public SubCmd {
 public:
  virtual void execute(const SubCmdOptions &_options) const;

 public:
  SubCmdValidate(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations);

 private:
  std::string               m_device;
  std::vector<std::string>  m_tests_to_run;
  std::string               m_format;
  std::string               m_output;
  std::string               m_param;
  std::string               m_xclbin_location;
  std::string               m_pmode;
  bool                      m_help;

  void print_help_internal() const;
  void handle_errors_and_validate_tests(boost::program_options::variables_map&, 
                                        std::vector<std::string>&,
                                        std::vector<std::string>&) const;
  XBUtilities::VectorPairStrings getTestNameDescriptions(const bool addAdditionOptions) const;
};

#endif
