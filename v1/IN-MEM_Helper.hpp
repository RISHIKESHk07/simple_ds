#pragma once
#include "../Data_structures/AVLTree.hpp"
#include "../Data_structures/Skiplist.hpp"
#include "./IN-MEM_DB_key_spec.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
  friend std::ostream &operator<<(std::ostream &os,
                                  const Wrap_ZZSET_internal_DB_key &p) {
    return os << p.score;
  }
};

struct Wrap_ZSET_internal_DB_compactor {
  int operator()(const Wrap_ZZSET_internal_DB_key &a,
                 const Wrap_ZZSET_internal_DB_key &b) const {
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
  }
};

struct Wrap_ZSET {
  std::unordered_map<std::string, double> score_hash;
  SkipList<Wrap_ZZSET_internal_DB_key, std::string,
           Wrap_ZSET_internal_DB_compactor>
      score_tree;
  Wrap_ZSET() = default;
};

enum class Error_codes { NONE, RUNTIME_ERROR, NOT_FOUND };

class Error_category {
public:
  std::optional<std::string> message = std::nullopt;
  Error_codes err_code = Error_codes::NONE;

  void reset() {
    message = std::nullopt;
    err_code = Error_codes::NONE;
  }

  void set_error(const std::string &str_mess, Error_codes err_c) {
    message = str_mess;
    err_code = err_c;
  }

  bool has_error() const { return err_code != Error_codes::NONE; }
};

class Wrap_object {
  Wrap_object_type type;
  Wrap_encoding encoding;
  std::variant<std::string, std::vector<std::string>,
               std::unordered_map<std::string, std::string>,
               AVLtree<std::string, std::string, StringComparator> *,
               Wrap_ZSET *>
      ptr;
  Error_category ec;

  std::vector<std::string> &as_vector() {
    return std::get<std::vector<std::string>>(this->ptr);
  }
  const std::vector<std::string> &as_vector() const {
    return std::get<std::vector<std::string>>(this->ptr);
  }
  std::string &as_string() { return std::get<std::string>(this->ptr); }
  const std::string &as_string() const {
    return std::get<std::string>(this->ptr);
  }
  AVLtree<std::string, std::string, StringComparator> &as_avl_tree() {
    return *std::get<AVLtree<std::string, std::string, StringComparator> *>(
        this->ptr);
  }
  const AVLtree<std::string, std::string, StringComparator> &
  as_avl_tree() const {
    return *std::get<AVLtree<std::string, std::string, StringComparator> *>(
        this->ptr);
  }
  std::unordered_map<std::string, std::string> &as_unordered_map() {
    return std::get<std::unordered_map<std::string, std::string>>(this->ptr);
  }
  Wrap_ZSET &as_ZSET() { return *std::get<Wrap_ZSET *>(this->ptr); }
  const Wrap_ZSET &as_ZSET() const { return *std::get<Wrap_ZSET *>(this->ptr); }

  void cleanup_pointers() {
    if (type == Wrap_object_type::HASH) {
      if (auto **t = std::get_if<
              AVLtree<std::string, std::string, StringComparator> *>(
              &this->ptr)) {
        delete *t;
      }
    } else if (type == Wrap_object_type::SORTED_SET) {
      if (auto **z = std::get_if<Wrap_ZSET *>(&this->ptr)) {
        delete *z;
      }
    }
  }

public:
  Wrap_object()
      : type(Wrap_object_type::STRING), encoding(Wrap_encoding::RAW_STRING),
        ptr(std::string("")) {}

  ~Wrap_object() { cleanup_pointers(); }

  // Prevent double-free issues on simple copies
  Wrap_object(const Wrap_object &) = delete;
  Wrap_object &operator=(const Wrap_object &) = delete;

  Wrap_object(Wrap_object &&other) noexcept
      : type(other.type), encoding(other.encoding), ptr(std::move(other.ptr)),
        ec(other.ec) {
    other.type = Wrap_object_type::STRING;
    other.ptr = std::string("");
  }

  Wrap_object &operator=(Wrap_object &&other) noexcept {
    if (this != &other) {
      cleanup_pointers();
      type = other.type;
      encoding = other.encoding;
      ptr = std::move(other.ptr);
      ec = other.ec;
      other.type = Wrap_object_type::STRING;
      other.ptr = std::string("");
    }
    return *this;
  }

  const Error_category &get_error() const { return ec; }

  std::optional<Wrap_object *> create_string(std::string &val) {
    ec.reset();
    try {
      cleanup_pointers();
      this->type = Wrap_object_type::STRING;
      this->encoding = Wrap_encoding::RAW_STRING;
      this->ptr = val;
      return this;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  // String operations
  std::optional<std::string> get_string() const {
    const_cast<Wrap_object *>(this)->ec.reset();
    try {
      return std::get<std::string>(this->ptr);
    } catch (const std::exception &e) {
      const_cast<Wrap_object *>(this)->ec.set_error(e.what(),
                                                    Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<int64_t> incr_by(int64_t increment) {
    ec.reset();
    try {
      auto new_value = std::stoll(std::get<std::string>(this->ptr)) + increment;
      this->ptr = std::to_string(new_value);
      return new_value;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<double> incr_by_float(double increment) {
    ec.reset();
    try {
      auto new_value = std::stod(std::get<std::string>(this->ptr)) + increment;
      this->ptr = std::to_string(new_value);
      return new_value;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> append(const std::string &value) {
    ec.reset();
    try {
      this->ptr = std::get<std::string>(this->ptr) + value;
      return std::get<std::string>(this->ptr).size();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::string> get_range_string(int64_t start,
                                              int64_t end) const {
    const_cast<Wrap_object *>(this)->ec.reset();
    try {
      const auto &str = std::get<std::string>(this->ptr);
      if (start < 0 || start >= static_cast<int64_t>(str.size()) ||
          start > end) {
        const_cast<Wrap_object *>(this)->ec.set_error("Index out of bounds",
                                                      Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      return str.substr(start, end - start + 1);
    } catch (const std::exception &e) {
      const_cast<Wrap_object *>(this)->ec.set_error(e.what(),
                                                    Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> set_range(size_t offset, const std::string &value) {
    ec.reset();
    try {
      auto &str = std::get<std::string>(this->ptr);
      if (offset > str.size()) {
        ec.set_error("Offset out of bounds", Error_codes::RUNTIME_ERROR);
        return std::nullopt;
      }
      str.insert(offset, value);
      return str.size();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  // List operations
  std::optional<Wrap_object *> create_list(std::vector<std::string> &val) {
    ec.reset();
    try {
      cleanup_pointers();
      this->type = Wrap_object_type::LIST;
      this->encoding = Wrap_encoding::LINKED_LIST;
      this->ptr = val;
      return this;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> push_left_list(const std::string &element) {
    ec.reset();
    try {
      auto &v = as_vector();
      v.insert(v.begin(), element);
      return v.size();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> push_right_list(const std::string &element) {
    ec.reset();
    try {
      auto &v = as_vector();
      v.push_back(element);
      return v.size();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::string> pop_left_list() {
    ec.reset();
    try {
      auto &v = as_vector();
      if (v.empty()) {
        ec.set_error("List is empty", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      std::string str = v.front();
      v.erase(v.begin());
      return str;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::string> pop_right_list() {
    ec.reset();
    try {
      auto &v = as_vector();
      if (v.empty()) {
        ec.set_error("List is empty", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      std::string str = v.back();
      v.pop_back();
      return str;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::string> get_by_index_list(int64_t index) {
    ec.reset();
    try {
      auto &v = as_vector();
      int64_t resolved_index =
          (index < 0) ? (static_cast<int64_t>(v.size()) + index) : index;
      if (resolved_index < 0 ||
          resolved_index >= static_cast<int64_t>(v.size())) {
        ec.set_error("Index out of bounds", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      return v[resolved_index];
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> set_by_index_list(int64_t index,
                                        const std::string &value) {
    ec.reset();
    try {
      auto &v = as_vector();
      int64_t resolved_index =
          (index < 0) ? (static_cast<int64_t>(v.size()) + index) : index;
      if (resolved_index < 0 ||
          resolved_index >= static_cast<int64_t>(v.size())) {
        ec.set_error("Index out of bounds", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      v[resolved_index] = value;
      return true;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> length() {
    ec.reset();
    try {
      auto &v = as_vector();
      return v.size();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::vector<std::string>> get_range_list(int64_t start,
                                                         int64_t end) {
    ec.reset();
    try {
      auto &v = as_vector();
      int64_t start_resolved_index =
          (start < 0) ? (static_cast<int64_t>(v.size()) + start) : start;
      int64_t end_resolved_index =
          (end < 0) ? (static_cast<int64_t>(v.size()) + end) : end;

      if (start_resolved_index < 0 ||
          start_resolved_index >= static_cast<int64_t>(v.size()) ||
          end_resolved_index < 0 ||
          end_resolved_index >= static_cast<int64_t>(v.size()) ||
          start_resolved_index > end_resolved_index) {
        ec.set_error("Invalid range requested", Error_codes::NOT_FOUND);
        return std::nullopt;
      }

      return std::vector<std::string>(v.begin() + start_resolved_index,
                                      v.begin() + end_resolved_index + 1);
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  // Hash operations
  std::optional<Wrap_object *> create_hashMap() {
    ec.reset();
    try {
      cleanup_pointers();
      this->type = Wrap_object_type::HASH;
      this->encoding = Wrap_encoding::HASHMAP;
      this->ptr = new AVLtree<std::string, std::string, StringComparator>();
      return this;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> set_field(const std::string &field,
                                const std::string &value) {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      auto s = t.search(field, t.tree);
      if (s.first == nullptr) {
        t.insertion(field, value);
        return true; // Created new field
      }
      s.first->val = value;
      return false; // Updated existing field
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::string> get_field(const std::string &field) {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      auto s = t.search(field, t.tree);
      if (s.first == nullptr) {
        ec.set_error("Field not found in Hash", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      return s.first->val;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> delete_field(const std::string &field) {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      auto s = t.search(field, t.tree);
      if (s.first == nullptr) {
        ec.set_error("Field not found for deletion", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      t.deletion(field);
      return true;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }
  // here partial writes are possible , so make cleanup function ...
  std::optional<std::vector<std::optional<bool>>> set_multiple(
      const std::vector<std::pair<std::string, std::string>> &field_values) {
    ec.reset();
    try {
      std::vector<std::optional<bool>> res;
      for (const auto &pair : field_values) {
        auto r = set_field(pair.first, pair.second);
        if (!r.has_value()) {
          res.push_back(std::nullopt);
        } else {
          res.push_back(r.value());
        }
      }
      return res;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::vector<std::optional<std::string>>>
  get_multiple_hash(const std::vector<std::string> &fields) {
    ec.reset();
    try {
      std::vector<std::optional<std::string>> res;
      for (const auto &f : fields) {
        auto &t = as_avl_tree();
        auto s = t.search(f, t.tree);
        if (s.first != nullptr) {
          res.push_back(s.first->val);
        } else {
          res.push_back(""); // Redis HMGET convention returns
                             // null/empty for missing fields
        }
      }
      return res;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> size_hash() {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      return t.size_of_tree();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> exists_hash(const std::string &field) {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      auto s = t.search(field, t.tree);
      if (s.first == nullptr) {
        ec.set_error("Field does not exist", Error_codes::NOT_FOUND);
        return false;
      }
      return true;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::vector<std::pair<std::string, std::string>>>
  get_all_hash() {
    ec.reset();
    try {
      auto &t = as_avl_tree();
      return t.inorder_full_traversal();
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  // ZSet operations
  std::optional<Wrap_object *> create_ZSET() {
    ec.reset();
    try {
      cleanup_pointers();
      this->type = Wrap_object_type::SORTED_SET;
      this->encoding = Wrap_encoding::ZSET_TREE;
      this->ptr = new Wrap_ZSET();
      return this;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> zset_add(double score, const std::string &member) {
    ec.reset();
    try {
      auto &t = as_ZSET();
      t.score_hash[member] = score;
      Wrap_ZZSET_internal_DB_key w(score, member);
      t.score_tree.insert(w, w.raw_member);
      return true;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<bool> remove(const std::string &member) {
    ec.reset();
    try {
      auto &t = as_ZSET();
      auto it = t.score_hash.find(member);
      if (it == t.score_hash.end()) {
        ec.set_error("Member not found in ZSET", Error_codes::NOT_FOUND);
        return false;
      }
      Wrap_ZZSET_internal_DB_key w(it->second, member);
      t.score_hash.erase(it);
      t.score_tree.delete_key(w);
      return true;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<double> get_score(const std::string &member) {
    ec.reset();
    try {
      auto &t = as_ZSET();
      auto it = t.score_hash.find(member);
      if (it == t.score_hash.end()) {
        ec.set_error("Member not found in ZSET", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      return it->second;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<int64_t> get_rank(const std::string &member) {
    ec.reset();
    try {
      auto &t = as_ZSET();
      auto it = t.score_hash.find(member);
      if (it == t.score_hash.end()) {
        ec.set_error("Member not found in ZSET", Error_codes::NOT_FOUND);
        return std::nullopt;
      }
      auto [s, index_length, preds] =
          t.score_tree.search(Wrap_ZZSET_internal_DB_key(it->second, member));
      return index_length;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<size_t> size() {
    ec.reset();
    try {
      auto &t = as_ZSET();
      return t.score_tree.length_skiplist;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::vector<std::pair<std::string, double>>>
  get_range_by_rank(int64_t start, int64_t end) {
    ec.reset();
    try {
      auto &t = as_ZSET();
      std::vector<std::pair<std::string, double>> res;
      auto s = t.score_tree.search_index(start);
      auto e = t.score_tree.search_index(end);
      if (s == nullptr || e == nullptr) {
        ec.set_error("Rank index out of range", Error_codes::NOT_FOUND);
        return res;
      }
      auto r = t.score_tree.range_search(s->k, e->k);
      for (auto i : r) {
        res.push_back({i->k.raw_member, i->k.score});
      }
      return res;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<double> update_increase_by_delta(double delta,
                                                 const std::string &member) {
    ec.reset();
    try {
      auto &z = as_ZSET();
      auto it = z.score_hash.find(member);

      if (it == z.score_hash.end()) {
        zset_add(delta, member);
        return delta;
      }

      double old_score = it->second;
      Wrap_ZZSET_internal_DB_key old_key(old_score, member);
      z.score_tree.delete_key(old_key);

      double new_score = old_score + delta;
      Wrap_ZZSET_internal_DB_key new_key(new_score, member);

      z.score_tree.insert(new_key, member);
      it->second = new_score;

      return new_score;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  std::optional<std::vector<std::pair<std::string, double>>>
  get_range_by_score(double min_score, double max_score) {
    ec.reset();
    try {
      auto &z = as_ZSET();
      Wrap_ZZSET_internal_DB_key left(min_score, "");
      Wrap_ZZSET_internal_DB_key right(
          max_score, std::string(255, static_cast<char>(255)));

      auto nodes = z.score_tree.range_search(left, right);
      std::vector<std::pair<std::string, double>> ans;

      for (auto n : nodes) {
        ans.push_back({n->k.raw_member, n->k.score});
      }
      return ans;
    } catch (const std::exception &e) {
      ec.set_error(e.what(), Error_codes::RUNTIME_ERROR);
      return std::nullopt;
    }
  }

  void pretty_print(std::ostream &os = std::cout) const {
    os << "----------------------------------------\n";
    switch (type) {
    case Wrap_object_type::STRING: {
      os << "Type    : STRING\n";
      os << "Value   : \"" << std::get<std::string>(this->ptr) << "\"\n";
      break;
    }
    case Wrap_object_type::LIST: {
      const auto &v = std::get<std::vector<std::string>>(this->ptr);
      os << "Type    : LIST (Size: " << v.size() << ")\n";
      for (size_t i = 0; i < v.size(); ++i) {
        os << "  [" << i << "] -> \"" << v[i] << "\"\n";
      }
      break;
    }
    case Wrap_object_type::HASH: {
      os << "Type    : HASH (AVL Tree)\n";
      auto *tree_ptr =
          std::get<AVLtree<std::string, std::string, StringComparator> *>(
              this->ptr);
      if (tree_ptr) {
        auto all_pairs = tree_ptr->inorder_full_traversal();
        tree_ptr->printTree();
        os << "Size    : " << all_pairs.size() << "\n";
        for (const auto &[field, value] : all_pairs) {
          os << "  " << field << " => \"" << value << "\"\n";
        }
      } else {
        os << "  (Empty / Uninitialized)\n";
      }
      break;
    }
    case Wrap_object_type::SORTED_SET: {
      os << "Type    : SORTED_SET (ZSET - Skiplist + Hash)\n";
      auto *zset_ptr = std::get<Wrap_ZSET *>(this->ptr);
      if (zset_ptr) {
        os << "Size    : " << zset_ptr->score_hash.size() << "\n";
        os << "Members :\n";
        for (const auto &[member, score] : zset_ptr->score_hash) {
          os << "  - " << member << " (Score: " << score << ")\n";
        }
        zset_ptr->score_tree.prettyPrint();
      } else {
        os << "  (Empty / Uninitialized)\n";
      }
      break;
    }
    }
    os << "----------------------------------------\n";
  }
};
