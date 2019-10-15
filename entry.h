#pragma once
#include <type_traits>

#include "types.h"

template <typename Url, bool min = true> struct entry {
  Url url;
  count_t count;

  entry() = default;

  template <typename String>
  entry(String str, count_t c) : url(str), count(c) {}

  bool operator<(const entry &rhs) {
    if constexpr (min) {
      return count < rhs.count;
    } else {
      return count > rhs.count;
    }
  }

  bool operator==(const entry &rhs) const {
    return url == rhs.url && count == rhs.count;
  }
};

template <typename T> struct is_entry : std::false_type {};

template <typename Url, bool m>
struct is_entry<entry<Url, m>> : std::true_type {};