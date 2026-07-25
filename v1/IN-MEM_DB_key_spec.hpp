#pragma once
#include <cstdint>
#include <string>
#include <string_view>
// DB key spec , the main keytype we use for index maps
struct DBKey {
  uint32_t hash; // Cached 32-bit hash for fast inline integer comparisons
  std::string raw_key;
  DBKey() = default;
  DBKey(std::string key) : raw_key(std::move(key)) {
    hash = compute_fnv1a(raw_key);
  }

private:
  static uint32_t compute_fnv1a(std::string_view str) {
    uint32_t hash = 2166136261U;
    for (char c : str) {
      hash ^= static_cast<uint32_t>(c);
      hash *= 16777619U;
    }
    return hash;
  }
};

struct DBKeyComparator {
  // Returns: -1 if a < b, 0 if a == b, 1 if a > b
  int operator()(const DBKey &a, const DBKey &b) const {

    if (a.hash != b.hash) {
      return (a.hash < b.hash) ? -1 : 1;
    }

    if (a.raw_key == b.raw_key)
      return 0;
    return (a.raw_key < b.raw_key) ? -1 : 1;
  }
};
// simple comporator for strings , used hash maps and other place ....
struct StringComparator {
  int operator()(const std::string &a, const std::string &b) const {
    int res = a.compare(b);
    if (res < 0)
      return -1; // a comes before b
    if (res > 0)
      return 1; // a comes after b
    return 0;   // strings are identical
  }
};
