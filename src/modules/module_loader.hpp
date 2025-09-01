/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cerrno>
#include <cstring>
#include <deque>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <string>

#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

#include "modules/module.hpp"

namespace fs = std::filesystem;
template <typename T>
using Result = outcome::result<T>;

namespace lean::modules {

  class ModuleLoader {
   public:
    enum class Error : uint8_t {
      PathIsNotADir,
      OpenLibraryFailed,
      NoLoaderIdExport,
      UnexpectedLoaderId,
      NoModuleInfoExport,
      UnexpectedModuleInfo,
    };

    explicit ModuleLoader(const std::string &dir_path) : dir_path_(dir_path) {}

    Result<std::deque<std::shared_ptr<Module>>> get_modules() {
      std::deque<std::shared_ptr<Module>> modules;
      OUTCOME_TRY(recursive_search(fs::path(dir_path_), modules));
      return modules;
    }

   private:
    std::string dir_path_;

    Result<void> recursive_search(const fs::path &dir_path,
                                  std::deque<std::shared_ptr<Module>> &modules);
    Result<void> load_module(const std::string &module_path,
                             std::deque<std::shared_ptr<Module>> &modules);
  };

}  // namespace lean::modules

OUTCOME_HPP_DECLARE_ERROR(lean::modules, ModuleLoader::Error);
