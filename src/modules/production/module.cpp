/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <modules/production/production.hpp>

#define MODULE_C_API extern "C" __attribute__((visibility("default")))
#define MODULE_API __attribute__((visibility("default")))

MODULE_C_API const char *loader_id() {
  return "ProductionLoader";
}

MODULE_C_API const char *module_info() {
  return "ProductionModule v1.0";
}

static std::shared_ptr<lean::modules::ProductionModule> module_instance;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

MODULE_C_API std::weak_ptr<lean::modules::ProductionModule>
query_module_instance(lean::modules::ProductionLoader &loader,
                      std::shared_ptr<lean::log::LoggingSystem> logger) {
  if (!module_instance) {
    module_instance = std::make_shared<lean::modules::ProductionModuleImpl>(
        loader, std::move(logger));
  }
  return module_instance;
}

MODULE_C_API void release_module_instance() {
  module_instance.reset();
}

#pragma GCC diagnostic pop
