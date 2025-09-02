/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/fd_limit.hpp"

#include <algorithm>

#include <boost/iterator/counting_iterator.hpp>
#include <sys/resource.h>

namespace lean {
  namespace {
    bool getFdLimit(rlimit &r, const log::Logger &logger) {
      if (getrlimit(RLIMIT_NOFILE, &r) != 0) {
        SL_WARN(logger,
                "Error: getrlimit(RLIMIT_NOFILE) errno={} {}",
                errno,
                strerror(errno));
        return false;
      }
      return true;
    }

    bool setFdLimit(const rlimit &r) {
      return setrlimit(RLIMIT_NOFILE, &r) == 0;
    }
  }  // namespace

  std::optional<size_t> getFdLimit(const log::Logger &logger) {
    rlimit r{};
    if (!getFdLimit(r, logger)) {
      return std::nullopt;
    }
    return r.rlim_cur;
  }

  void setFdLimit(size_t limit, const log::Logger &logger) {
    rlimit r{};
    if (!getFdLimit(r, logger)) {
      return;
    }
    if (r.rlim_max == RLIM_INFINITY) {
      SL_VERBOSE(logger, "current={} max=unlimited", r.rlim_cur);
    } else {
      SL_VERBOSE(logger, "current={} max={}", r.rlim_cur, r.rlim_max);
    }
    const rlim_t current = r.rlim_cur;
    if (limit == current) {
      return;
    }
    r.rlim_cur = limit;
    if (limit < current) {
      SL_WARN(logger, "requested limit is lower than system allowed limit");
      setFdLimit(r);
    } else if (!setFdLimit(r)) {
      std::ignore = std::upper_bound(boost::counting_iterator{current},
                                     boost::counting_iterator{rlim_t{limit}},
                                     nullptr,
                                     [&](std::nullptr_t, auto new_current) {
                                       r.rlim_cur = new_current;
                                       return !setFdLimit(r);
                                     });
    }
    if (!getFdLimit(r, logger)) {
      return;
    }
    if (r.rlim_cur != current) {
      SL_VERBOSE(logger, "changed current={}", r.rlim_cur);
    }
  }
}  // namespace lean
