/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdlib>
#include <optional>

#include "log/logger.hpp"

struct rlimit;

namespace lean {

  std::optional<size_t> getFdLimit(const log::Logger &logger);
  void setFdLimit(size_t limit, const log::Logger &logger);

}  // namespace lean
