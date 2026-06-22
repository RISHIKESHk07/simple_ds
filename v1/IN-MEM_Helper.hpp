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
struct Wrap_ZSET;
struct Wrap_object {
  Wrap_object_type type;
  Wrap_encoding encoding;
  std::variant<std::string, std::vector<std::string>,
               std::unordered_map<std::string, std::string>,
               std::shared_ptr<Wrap_ZSET>>
      ptr;

public:
  Wrap_object create_string(std::string &val) {
    Wrap_object w;
    w.type = Wrap_object_type::STRING;
    w.encoding = Wrap_encoding::RAW_STRING;
    w.ptr = std::move(val);
    return w;
  }
  Wrap_object create_list(std::vector<std::string> &val) {
    Wrap_object w;
    w.type = Wrap_object_type::LIST;
    w.encoding = Wrap_encoding::LINKED_LIST;
    w.ptr = std::move(val);
    return w;
  }
  Wrap_object create_hash(std::unordered_map<std::string, std::string> &val) {
    Wrap_object w;
    w.type = Wrap_object_type::STRING;
    w.encoding = Wrap_encoding::RAW_STRING;
    w.ptr = std::move(val);
    return w;
  }
  Wrap_object create_ZSET(std::shared_ptr<Wrap_ZSET> &val) {
    Wrap_object w;
    w.type = Wrap_object_type::SORTED_SET;
    w.encoding = Wrap_encoding::ZSET_TREE;
    w.ptr = std::move(val);
    return w;
  }
};
