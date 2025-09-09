#pragma once

#include <cstdlib>
#include <string>

// TODO(turuslan): config
inline size_t getPeerIndex() {
  static size_t i = [] {
    if (auto s = getenv("PeerIndex")) {
      return std::stoul(s);
    } else {
      return 0ul;
    }
  }();
  return i;
}
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
