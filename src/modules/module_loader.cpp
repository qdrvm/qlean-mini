/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "modules/module_loader.hpp"

#define COMPONENT_NAME "ModuleLoader"

OUTCOME_CPP_DEFINE_CATEGORY(lean::modules, ModuleLoader::Error, e) {
  using E = lean::modules::ModuleLoader::Error;
  switch (e) {
    case E::PathIsNotADir:
      return COMPONENT_NAME ": path is not a directory";
    case E::OpenLibraryFailed:
      return COMPONENT_NAME ": open library failed";
    case E::NoLoaderIdExport:
      return COMPONENT_NAME ": library doesn't provide loader_id function";
    case E::UnexpectedLoaderId:
      return COMPONENT_NAME ": unexpected loader id";
    case E::NoModuleInfoExport:
      return COMPONENT_NAME ": library doesn't provide module_info function";
    case E::UnexpectedModuleInfo:
      return COMPONENT_NAME ": unexpected module info";
  }
  return COMPONENT_NAME ": unknown error";
}

namespace lean::modules {

  Result<void> ModuleLoader::recursive_search(
      const fs::path &dir_path, std::deque<std::shared_ptr<Module>> &modules) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
      return Error::PathIsNotADir;
    }

    for (const auto &entry : fs::directory_iterator(dir_path)) {
      const auto &entry_path = entry.path();
      const auto &entry_name = entry.path().filename().string();

      if (entry_name[0] == '.' || entry_name[0] == '_') {
        continue;
      }

      if (fs::is_directory(entry)) {
        OUTCOME_TRY(recursive_search(entry_path, modules));
      } else if (fs::is_regular_file(entry)
                 && entry_path.extension() == ".so") {
        OUTCOME_TRY(load_module(entry_path.string(), modules));
      }
    }
    return outcome::success();
  }

  Result<void> ModuleLoader::load_module(
      const std::string &module_path,
      std::deque<std::shared_ptr<Module>> &modules) {
    std::unique_ptr<void, int (*)(void *)> handle(
        dlopen(module_path.c_str(), RTLD_LAZY), dlclose);
    if (!handle) {
      return Error::OpenLibraryFailed;
    }

    typedef const char *(*LoaderIdFunc)();
    LoaderIdFunc loader_id_func =
        (LoaderIdFunc)dlsym(handle.get(), "loader_id");

    if (!loader_id_func) {
      return Error::NoLoaderIdExport;
    }

    const char *loader_id = loader_id_func();
    if (!loader_id) {
      return Error::UnexpectedLoaderId;
    }

    typedef const char *(*ModuleInfoFunc)();
    ModuleInfoFunc module_info_func =
        (ModuleInfoFunc)dlsym(handle.get(), "module_info");

    if (!loader_id_func) {
      return Error::NoModuleInfoExport;
    }

    const char *module_info = module_info_func();
    if (!module_info) {
      return Error::UnexpectedModuleInfo;
    }

    auto module =
        Module::create(module_path, module_info, std::move(handle), loader_id);
    modules.push_back(module);
    return outcome::success();
  }


}  // namespace lean::modules
