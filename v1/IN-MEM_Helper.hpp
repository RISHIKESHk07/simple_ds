#pragma once
#include "../Data_structures/AVLTree.hpp"
#include "../Data_structures/Skiplist.cpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
// Internal wrapper objects
enum class Wrap_object_type { LIST, SORTED_SET, HASH, STRING };
enum class Wrap_encoding {
  RAW_STRING,  // pure string
  HASHMAP,     // either this or nested tree used
  LINKED_LIST, // Linked list for lists
  NESTED_TREE,
  ZSET_TREE // custom hash + tree for ZSET
};

struct Wrap_ZZSET_internal_DB_key {
  double score;
  uint32_t hash;
  std::string raw_member;
  Wrap_ZZSET_internal_DB_key(double s, std::string member)
      : score(s), raw_member(std::move(member)) {
    hash = compute_fnv1a(raw_member);
  }
  static Wrap_ZZSET_internal_DB_key neg_inf() {
    return {-std::numeric_limits<double>::infinity(), ""};
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

public:
  // define this or else printing issues will occur and break the skiplist code/
  // any tree code
  friend std::ostream &operator<<(std::ostream &os,
                                  const Wrap_ZZSET_internal_DB_key &p) {
    return os << p.score;
  };
};
struct Wrap_ZSET_internal_DB_compactor {
  int operator()(const Wrap_ZZSET_internal_DB_key &a,
                 const Wrap_ZZSET_internal_DB_key &b) {
    if (a.score != b.score) {
      return (a.score < b.score) ? 1 : -1;
    }
    if (a.hash != b.hash) {
      return (a.hash < b.hash) ? 1 : -1;
    }

    if (a.raw_member == b.raw_member)
      return 0;
    else {
      return (a.raw_member < b.raw_member) ? 1 : -1;
    }
  };
};

struct Wrap_ZSET {
  // internal hashmap using unordered
  // tree based search (or) skiplist , we need a order static tree for impl here
  // the current avl tree is quite messy already would not want to change stuff
  // again , so you can do this when optimising performance later on , we will
  // be using skiplist as it does gives us span based attribute help with this .
  std::unordered_map<std::string, int> score_hash;
  SkipList<Wrap_ZZSET_internal_DB_key, std::string,
           Wrap_ZSET_internal_DB_compactor>
      score_tree;
  Wrap_ZSET() = default;
};
class Wrap_object {
  Wrap_object_type type;
  Wrap_encoding encoding;
  std::variant<std::string, std::vector<std::string>,
               std::unordered_map<std::string, std::string>,
               AVLtree<DBKey, std::string, DBKeyComparator> *, Wrap_ZSET *>
      ptr;
  std::vector<std::string> &as_vector() {
    return std::get<std::vector<std::string>>(this->ptr);
  }
  std::string &as_string() { return std::get<std::string>(this->ptr); }
  AVLtree<DBKey, std::string, DBKeyComparator> &as_avl_tree() {
    return *std::get<AVLtree<DBKey, std::string, DBKeyComparator> *>(this->ptr);
  }
  std::unordered_map<std::string, std::string> &as_unordered_map() {
    return std::get<std::unordered_map<std::string, std::string>>(this->ptr);
  }
  Wrap_ZSET &as_ZSET() { return *std::get<Wrap_ZSET *>(this->ptr); }

public:
  Wrap_object *create_string(std::string &val) {
    this->type = Wrap_object_type::STRING;
    this->encoding = Wrap_encoding::RAW_STRING;
    this->ptr = val;
    return this;
  }
  // string ops
  std::string get_string() { return std::get<std::string>(this->ptr); };
  int64_t incr_by(int64_t increment) {
    auto new_value = std::stoi(std::get<std::string>(this->ptr)) + increment;
    this->ptr = std::to_string(new_value);
    return new_value;
  };
  double incr_by_float(double increment) {
    auto new_value = std::stod(std::get<std::string>(this->ptr)) + increment;
    this->ptr = std::to_string(new_value);
    return new_value;
  };
  size_t append(const std::string value) {
    this->ptr = std::get<std::string>(this->ptr) + (value);
    return std::get<std::string>(this->ptr).size();
  };
  std::string get_range_string(int64_t start, int64_t end) const {
    return std::get<std::string>(this->ptr).substr(start, end);
  };
  size_t set_range(size_t offset, const std::string value) {
    std::get<std::string>(this->ptr).insert(offset, value);
    return std::get<std::string>(this->ptr).size();
  };

  // list ops
  Wrap_object *create_list(std::vector<std::string> &val) {
    this->type = Wrap_object_type::LIST;
    this->encoding = Wrap_encoding::LINKED_LIST;
    this->ptr = val;
    return this;
  }
  size_t push_left_list(const std::string &element) {
    auto &v = as_vector();
    v.insert(v.begin(), element);
    return v.size();
  }; // LPUSH
  size_t push_right_list(const std::string &element) {
    auto &v = as_vector();
    v.push_back(element);
    return v.size();
  }; // RPUSH
  std::string pop_left_list() {
    auto &v = as_vector();
    if (v.empty())
      return "Error:empty";
    std::string str = v.front();
    v.erase(v.begin());
    return str;
  }; // LPOP
  std::string pop_right_list() {
    auto &v = as_vector();
    if (v.empty())
      return "Error:empty";
    std::string str = v.back();
    v.pop_back();
    return str;
  }; // RPOP
  std::string get_by_index_list(int64_t index) {
    auto &v = as_vector();
    int64_t resolved_index =
        (index < 0) ? (static_cast<int64_t>(v.size()) + index) : index;
    if (resolved_index < 0 || resolved_index > static_cast<int64_t>(v.size())) {
      return "out of bounds";
    }
    return v[resolved_index];
  }; // LINDEX
  void set_by_index_list(int64_t index, const std::string &value) {
    auto &v = as_vector();
    int64_t resolved_index =
        (index < 0) ? (static_cast<int64_t>(v.size()) + index) : index;
    if (resolved_index < 0 || resolved_index > static_cast<int64_t>(v.size())) {
      return;
    }
    v[resolved_index] = value;
  }; // LSET
  size_t length() {
    auto &v = as_vector();
    return v.size();
  }; // LLEN
  std::vector<std::string> get_range_list(int64_t start, int64_t end) {
    auto &v = as_vector();
    int64_t start_resolved_index =
        (start < 0) ? (static_cast<int64_t>(v.size()) + start) : start;
    int64_t end_resolved_index =
        (end < 0) ? (static_cast<int64_t>(v.size()) + end) : end;
    std::vector<std::string> temp;
    if (start < 0 || start > v.size()) {
      return temp;
    } else if (end < 0 || end > v.size()) {
      return temp;
    } else if (start > end) {
      return temp;
    } else {

      return std::vector<std::string>(v.begin() + start, v.begin() + end);
    }
  };

  // hash ops
  bool set_field(const std::string &field, const std::string &value) {
    auto &t = as_avl_tree();
    auto s_db_key = new DBKey(field);
    auto s = t.search_internal(*s_db_key, t.tree);
    if (s.first == nullptr) {
      t.insertion(*s_db_key, value);
      return true;
    }
    s.first->val = value;
    return false;

  }; // HSET (returns true if new)

  Wrap_object *create_hashMap() {
    this->type = Wrap_object_type::HASH;
    this->encoding = Wrap_encoding::HASHMAP;
    AVLtree<DBKey, std::string, DBKeyComparator> *avl;
    this->ptr = avl;
    return this;
  }

  std::string get_field(const std::string &field) {
    auto &t = as_avl_tree();
    auto s_db_key = new DBKey(field);
    auto s = t.search_internal(*s_db_key, t.tree);
    if (s.first == nullptr) {
      return "not fund ..";
    }
    return s.first->val;
  }; // HGET
  bool delete_field(const std::string &field) {
    auto &t = as_avl_tree();
    auto s_db_key = new DBKey(field);
    auto s = t.search_internal(*s_db_key, t.tree);
    if (s.first == nullptr) {
      return false;
    }
    t.deletion(*s_db_key);
    return true;

  }; // HDEL

  // Multi-Field Variants
  void set_multiple(
      const std::vector<std::pair<std::string, std::string>> &field_values) {
    for (auto i : field_values) {
      if (set_field(i.first, i.second)) {
        std::cout << "set completed" << std::endl;
      } else {
        std::cout << "set not completed" << std::endl;
      }
    }
  }; // HMSET
  std::vector<std::string>
  get_multiple_hash(const std::vector<std::string> &fields) {
    std::vector<std::string> res;
    for (auto i : fields) {
      res.push_back(get_field(i));
    }
    return res;
  }; // HMGET
  size_t size_hash() {
    auto &t = as_avl_tree();
    return t.size_of_tree();
  }; // HLEN
  bool exists_hash(const std::string &field) {
    auto &t = as_avl_tree();
    auto s_db_key = new DBKey(field);
    auto s = t.search_internal(*s_db_key, t.tree);
    if (s.first == nullptr) {
      return false;
    }
    return true;
  }; // HEXISTS
  std::vector<std::pair<std::string, std::string>> get_all_hash() {
    auto &t = as_avl_tree();
    return t.inorder_full_traversal();
  };

  // ZSet ops
  // Inserts or updates (Score, Member). If score changes, removes old compound
  // key from AVL and re-inserts.
  Wrap_object *create_ZSET() {
    this->type = Wrap_object_type::HASH;
    this->encoding = Wrap_encoding::ZSET_TREE;
    Wrap_ZSET *zset;
    this->ptr = zset;
    return this;
  }
  bool zset_add(double score, const std::string &member) {
    auto &t = as_ZSET();
    t.score_hash[member] = score;
    auto w = new Wrap_ZZSET_internal_DB_key(score, member);
    auto [s, index_l, preds] = t.score_tree.search(*w);
    t.score_tree.insert(*w, (*w).raw_member);
    return 1;

  }; // ZADD
  bool remove(const std::string &member) {
    auto &t = as_ZSET();
    auto w = new Wrap_ZZSET_internal_DB_key(t.score_hash[member], member);
    t.score_hash.erase(member);
    t.score_tree.delete_key(*w);
    return 1;

  }; // ZREM

  // Score & Rank Lookups (Uses Internal Hash Map)
  double get_score(const std::string &member) {
    auto &t = as_ZSET();

    return t.score_hash[member];
  }; // ZSCORE
  int64_t get_rank(const std::string &member) {
    auto &t = as_ZSET();
    auto w = new Wrap_ZZSET_internal_DB_key(t.score_hash[member], member);
    auto [s, index_lenght, preds] = t.score_tree.search(*w);
    return index_lenght;
  }; // ZRANK (1-indexed position in sorted order)

  // Sorted Range Scans (Traverses Internal Compound Skiplist )
  size_t size() {
    auto &t = as_ZSET();
    return t.score_tree.length_skiplist;
  }; // ZCARD

  // Returns elements ordered by index rank (e.g., top 10 players)
  std::vector<std::pair<std::string, double>> get_range_by_rank(int64_t start,
                                                                int64_t end) {
    auto &t = as_ZSET();
    std::vector<std::pair<std::string, double>> res;
    auto s = t.score_tree.search_index(start);
    auto e = t.score_tree.search_index(end);
    if (s == nullptr || e == nullptr) {
      return res;
    } else {
      auto r = t.score_tree.range_search(s->k, e->k);
      for (auto i : r) {
        res.push_back({i->k.raw_member, i->k.score});
      }
      return res;
    }

  }; // ZRANGE

  // Returns elements bounded by numeric score criteria (e.g., scores 100 to
  // 500)
  std::vector<std::pair<std::string, double>>
  get_range_by_score(double min_score, double max_score) {

  }; // ZRANGEBYSCORE
};
