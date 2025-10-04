#pragma once

#include <cstdlib>
#include <string>

inline size_t getValidatorCount() {
  static size_t i = [] {
    if (auto s = getenv("ValidatorCount")) {
      return std::stoul(s);
    } else {
      return 1ul;
    }
  }();
  return i;
}
