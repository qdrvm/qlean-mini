/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <optional>
#include <string>

#include <utils/ctor_limiters.hpp>

namespace lean::modules {

  class Module final : public std::enable_shared_from_this<Module>,
                       NonCopyable {
   public:
    Module(Module &&) = default;

    // Static method for Module object creation
    static std::shared_ptr<Module> create(
        const std::string &path,
        const std::string &module_info,
        std::unique_ptr<void, int (*)(void *)> handle,
        const std::string &loader_id) {
      return std::shared_ptr<Module>(
          new Module(path, module_info, std::move(handle), loader_id));
    }

    // Getter for the library path
    const std::string &get_path() const {
      return path_;
    }

    // Getter for module info
    const std::string &get_module_info() const {
      return module_info_;
    }

    // Getter for loader Id
    const std::string &get_loader_id() const {
      return loader_id_;
    }

    // Get function address from library
    template <typename ReturnType, typename... ArgTypes>
    std::optional<ReturnType (*)(ArgTypes...)> getFunctionFromLibrary(
        const char *funcName) {
      void *funcAddr = dlsym(handle_.get(), funcName);
      if (!funcAddr) {
        return std::nullopt;
      }
      return reinterpret_cast<ReturnType (*)(ArgTypes...)>(funcAddr);
    }

   private:
    Module(const std::string &path,
           const std::string &module_info,
           std::unique_ptr<void, int (*)(void *)> handle,
           const std::string &loader_id)
        : path_(path),
          module_info_(module_info),
          handle_(std::move(handle)),
          loader_id_(loader_id) {}

    std::string path_;                               // Library path
    std::string module_info_;                        // Module Info
    std::unique_ptr<void, int (*)(void *)> handle_;  // Library handle
    std::string loader_id_;                          // Loader ID

    Module &operator=(Module &&) = delete;
  };

}  // namespace lean::modules
