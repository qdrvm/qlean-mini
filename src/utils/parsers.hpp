/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cctype>
#include <charconv>
#include <optional>
#include <string_view>

namespace lean::util {

  /**
   * Case-insensitive comparison of two string views.
   *
   * @param lhs First string view
   * @param rhs Second string view
   * @return true if strings are equal ignoring case, false otherwise
   */
  inline bool iequals(const std::string_view lhs, const std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(lhs[i]))
          != std::tolower(static_cast<unsigned char>(rhs[i]))) {
        return false;
      }
    }
    return true;
  }

  /**
   * Parses a string representing a byte size (e.g., "10MB", "4 KiB") and
   * converts it to its value in bytes.
   *
   * Recognized suffixes (case-insensitive):
   * - SI (base 1000): B, KB, MB, GB, TB
   * - IEC (base 1024): KiB, MiB, GiB, TiB
   * - Single-letter: K, M, G, T are interpreted as IEC (1024-based)
   *
   * @param input string representation of byte size
   * @return size in bytes if parsing succeeded, std::nullopt otherwise
   */
  inline std::optional<uint64_t> parseByteQuantity(std::string_view input) {
    // Trim whitespace
    auto first = input.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) {
      return std::nullopt;
    }
    auto last = input.find_last_not_of(" \t\n\r");
    input = input.substr(first, last - first + 1);

    // Parse number
    size_t i = 0;
    while (i < input.size() && std::isdigit(input[i])) {
      ++i;
    }
    if (i == 0) {
      return std::nullopt;
    }

    std::string_view number_part = input.substr(0, i);
    while (i < input.size()
           && std::isspace(static_cast<unsigned char>(input[i]))) {
      ++i;
    }
    std::string_view suffix = input.substr(i);

    uint64_t number = 0;
    auto [ptr, ec] =
        std::from_chars(number_part.begin(), number_part.end(), number);
    if (ec != std::errc()) {
      return std::nullopt;
    }

    if (suffix.empty()) {
      return number;
    }

    struct SuffixDef {
      std::string_view suffix;
      uint64_t multiplier;
    };

    // Suffixes: IEC (base 1024) and SI (base 1000)
    static constexpr SuffixDef units[] = {
        {"b", 1},
        {"k", 1ull << 10},
        {"kib", 1ull << 10},
        {"kb", 1000ull},
        {"m", 1ull << 20},
        {"mib", 1ull << 20},
        {"mb", 1000ull * 1000ull},
        {"g", 1ull << 30},
        {"gib", 1ull << 30},
        {"gb", 1000ull * 1000ull * 1000ull},
        {"t", 1ull << 40},
        {"tib", 1ull << 40},
        {"tb", 1000ull * 1000ull * 1000ull * 1000ull},
    };

    for (const auto &[table_suffix, multiplier] : units) {
      if (iequals(table_suffix, suffix)) {
        if (number > UINT64_MAX / multiplier) {
          return std::nullopt;
        }
        return number * multiplier;
      }
    }

    return std::nullopt;
  }

  /**
   * Parses a string representing a time duration (e.g., "5m", "2 hours") and
   * converts it to its value in seconds.
   *
   * Recognized suffixes (case-insensitive): s, sec, minute, hour, day, week,
   * etc.
   *
   * @param input string representation of duration
   * @return duration in seconds if parsing succeeded, std::nullopt otherwise
   */
  inline std::optional<uint64_t> parseTimeDuration(std::string_view input) {
    // Trim whitespace
    auto first = input.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) {
      return std::nullopt;
    }
    auto last = input.find_last_not_of(" \t\n\r");
    input = input.substr(first, last - first + 1);

    // Parse number
    size_t i = 0;
    while (i < input.size() && std::isdigit(input[i])) {
      ++i;
    }
    if (i == 0) {
      return std::nullopt;
    }

    std::string_view number_part = input.substr(0, i);
    while (i < input.size()
           && std::isspace(static_cast<unsigned char>(input[i]))) {
      ++i;
    }
    std::string_view suffix = input.substr(i);

    uint64_t number = 0;
    auto [ptr, ec] =
        std::from_chars(number_part.begin(), number_part.end(), number);
    if (ec != std::errc()) {
      return std::nullopt;
    }

    if (suffix.empty()) {
      return number;
    }

    // Suffix table: each entry maps one or more suffix variants to multiplier
    struct Entry {
      std::string_view suffix;
      uint64_t multiplier;
    };
    static constexpr Entry suffixes[] = {
        {"s", 1},          {"sec", 1},      {"secs", 1},     {"second", 1},
        {"seconds", 1},    {"m", 60},       {"min", 60},     {"mins", 60},
        {"minute", 60},    {"minutes", 60}, {"h", 3600},     {"hr", 3600},
        {"hrs", 3600},     {"hour", 3600},  {"hours", 3600}, {"d", 86400},
        {"day", 86400},    {"days", 86400}, {"w", 604800},   {"week", 604800},
        {"weeks", 604800},
    };

    for (const auto &[table_suffix, multiplier] : suffixes) {
      if (iequals(table_suffix, suffix)) {
        if (number > UINT64_MAX / multiplier) {
          return std::nullopt;
        }
        return number * multiplier;
      }
    }

    return std::nullopt;
  }

}  // namespace lean::util
