// we need to make a parser which pulls from the network (format:) and then
// returns a wrap object , and in the memtable interface lets also adds the key
// into index map , while remember to keep a general mutation and querying
// lookup as weel in the memtable it self allowing for using the memtable as the
// only visible class to work with DB

#pragma once
#include "./IN-MEM_DB_key_spec.hpp"
#include "./IN-MEM_Helper.hpp"
#include "./IN-MEM_Interface.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Wrote this with AI for scaffloading fast but ig it could be improved with
// byte-to-byte parsing instead of strings , so i will figure this out when i
// have completed some more work .... using AI is shameful :[

enum class ParseStatus {
  SUCCESS,
  INCOMPLETE, // Network buffer split — wait for more socket bytes
  ERROR       // Invalid RESP syntax
};

struct RespCommand {
  std::string name;              // e.g., "ZADD", "HSET", "GET"
  std::vector<std::string> args; // e.g., ["myzset", "100", "Alice"]
};

class RespParser {
public:
  static ParseStatus parse(const std::string &buffer, RespCommand &cmd,
                           size_t &bytes_read) {
    if (buffer.empty())
      return ParseStatus::INCOMPLETE;
    if (buffer[0] != '*')
      return ParseStatus::ERROR;

    size_t pos = 0;
    size_t line_end = buffer.find("\r\n", pos);
    if (line_end == std::string::npos)
      return ParseStatus::INCOMPLETE;

    int num_args = 0;
    try {
      num_args = std::stoi(buffer.substr(1, line_end - 1));
    } catch (...) {
      return ParseStatus::ERROR;
    }
    pos = line_end + 2;

    std::vector<std::string> tokens;
    for (int i = 0; i < num_args; ++i) {
      if (pos >= buffer.size())
        return ParseStatus::INCOMPLETE;
      if (buffer[pos] != '$')
        return ParseStatus::ERROR;

      size_t str_len_end = buffer.find("\r\n", pos);
      if (str_len_end == std::string::npos)
        return ParseStatus::INCOMPLETE;

      int str_len = 0;
      try {
        str_len = std::stoi(buffer.substr(pos + 1, str_len_end - (pos + 1)));
      } catch (...) {
        return ParseStatus::ERROR;
      }
      pos = str_len_end + 2;

      // Check if full string argument + trailing \r\n is present
      if (pos + str_len + 2 > buffer.size()) {
        return ParseStatus::INCOMPLETE;
      }

      tokens.push_back(buffer.substr(pos, str_len));
      pos += str_len + 2;
    }

    if (tokens.empty())
      return ParseStatus::ERROR;

    cmd.name = tokens[0];
    std::transform(cmd.name.begin(), cmd.name.end(), cmd.name.begin(),
                   ::toupper);
    cmd.args.assign(tokens.begin() + 1, tokens.end());

    bytes_read = pos;
    return ParseStatus::SUCCESS;
  }

  static std::string encode_simple_string(const std::string &s) {
    return "+" + s + "\r\n";
  }

  static std::string encode_bulk_string(const std::string &str) {
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
  }

  static std::string encode_integer(int64_t val) {
    return ":" + std::to_string(val) + "\r\n";
  }

  static std::string encode_double(double val) {
    std::string s = std::to_string(val);
    return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
  }

  static std::string encode_error(const std::string &err) {
    return "-ERR " + err + "\r\n";
  }

  static std::string encode_null() { return "$-1\r\n"; }

  static std::string encode_array(const std::vector<std::string> &list) {
    std::string res = "*" + std::to_string(list.size()) + "\r\n";
    for (const auto &item : list) {
      res += encode_bulk_string(item);
    }
    return res;
  }
};

template <Memtable_tree_interfaces TreeType = Memtable_tree_interfaces::AVL>
class Dispatch_Executor {
private:
  using MemtableType =
      Memtable<TreeType, DBKey, Wrap_object *, DBKeyComparator>;
  MemtableType memtable;

  // Helper to fetch key from memtable or dynamically instantiate it
  Wrap_object *get_or_create(const std::string &key_str,
                             Wrap_object_type type) {
    DBKey key(key_str);
    auto found = memtable.find(key);

    if (found.has_value() && found.value().second != nullptr) {
      return found.value().second;
    }

    Wrap_object *obj = new Wrap_object();
    if (type == Wrap_object_type::STRING) {
      std::string empty = "";
      obj->create_string(empty);
    } else if (type == Wrap_object_type::HASH) {
      obj->create_hashMap();
    } else if (type == Wrap_object_type::SORTED_SET) {
      obj->create_ZSET();
    } else if (type == Wrap_object_type::LIST) {
      std::vector<std::string> empty_vec;
      obj->create_list(empty_vec);
    }

    memtable.insert(key, obj);
    return obj;
  }

  Wrap_object *get(const std::string &key_str) {
    DBKey key(key_str);
    auto found = memtable.find(key);
    if (found.has_value()) {
      return found.value().second;
    }
    return nullptr;
  }

public:
  Wrap_object *get_private_wrap_obj(const std::string &key_str) {
    return this->get(key_str);
  }
  std::string execute_command(const RespCommand &cmd) {
    const std::string &op = cmd.name;
    const auto &args = cmd.args;

    // ==========================================
    // 1. STRING OPERATIONS
    // ==========================================
    if (op == "SET") {
      if (args.size() < 2)
        return RespParser::encode_error("wrong number of arguments for 'set'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::STRING);
      std::string val = args[1];
      obj->create_string(val);
      return RespParser::encode_simple_string("OK");
    } else if (op == "GET") {
      if (args.size() != 1)
        return RespParser::encode_error("wrong number of arguments for 'get'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      return RespParser::encode_bulk_string(obj->get_string());
    } else if (op == "INCRBY") {
      if (args.size() != 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'incrby'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::STRING);
      int64_t val = obj->incr_by(std::stoll(args[1]));
      return RespParser::encode_integer(val);
    }

    // ==========================================
    // 2. HASH OPERATIONS
    // ==========================================
    else if (op == "HSET") {
      if (args.size() != 3)
        return RespParser::encode_error("wrong number of arguments for 'hset'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::HASH);
      bool is_new = obj->set_field(args[1], args[2]);
      return RespParser::encode_integer(is_new ? 1 : 0);
    } else if (op == "HGET") {
      if (args.size() != 2)
        return RespParser::encode_error("wrong number of arguments for 'hget'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      std::string res = obj->get_field(args[1]);
      if (res == "not fund ..")
        return RespParser::encode_null();
      return RespParser::encode_bulk_string(res);
    } else if (op == "HDEL") {
      if (args.size() != 2)
        return RespParser::encode_error("wrong number of arguments for 'hdel'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_integer(0);
      return RespParser::encode_integer(obj->delete_field(args[1]) ? 1 : 0);
    }

    // ==========================================
    // 3. SORTED SET (ZSET) OPERATIONS
    // ==========================================
    else if (op == "ZADD") {
      if (args.size() < 3 || (args.size() - 1) % 2 != 0) {
        return RespParser::encode_error("wrong number of arguments for 'zadd'");
      }
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::SORTED_SET);
      int count = 0;
      for (size_t i = 1; i < args.size(); i += 2) {
        double score = std::stod(args[i]);
        std::string member = args[i + 1];
        if (obj->zset_add(score, member))
          count++;
      }
      return RespParser::encode_integer(count);
    } else if (op == "ZSCORE") {
      if (args.size() != 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'zscore'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      double score = obj->get_score(args[1]);
      return RespParser::encode_double(score);
    } else if (op == "ZRANK") {
      if (args.size() != 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'zrank'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      return RespParser::encode_integer(obj->get_rank(args[1]));
    } else if (op == "ZRANGE") {
      if (args.size() < 3)
        return RespParser::encode_error(
            "wrong number of arguments for 'zrange'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_array({});

      int64_t start = std::stoll(args[1]);
      int64_t end = std::stoll(args[2]);
      bool with_scores = (args.size() > 3 && args[3] == "WITHSCORES");

      auto range = obj->get_range_by_rank(start, end);
      std::vector<std::string> resp_list;
      for (const auto &[member, score] : range) {
        resp_list.push_back(member);
        if (with_scores)
          resp_list.push_back(std::to_string(score));
      }
      return RespParser::encode_array(resp_list);
    } else if (op == "ZRANGEBYSCORE") {
      if (args.size() < 3)
        return RespParser::encode_error(
            "wrong number of arguments for 'zrangebyscore'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_array({});

      double min_score = std::stod(args[1]);
      double max_score = std::stod(args[2]);

      auto range = obj->get_range_by_score(min_score, max_score);
      std::vector<std::string> resp_list;
      for (const auto &[member, score] : range) {
        resp_list.push_back(member);
      }
      return RespParser::encode_array(resp_list);
    } else if (op == "ZINCRBY") {
      if (args.size() != 3)
        return RespParser::encode_error(
            "wrong number of arguments for 'zincrby'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::SORTED_SET);
      double delta = std::stod(args[1]);
      double new_score = obj->update_increase_by_delta(delta, args[2]);
      return RespParser::encode_double(new_score);
    } else if (op == "ZREM") {
      if (args.size() < 2)
        return RespParser::encode_error("wrong number of arguments for 'zrem'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_integer(0);

      int removed = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        if (obj->remove(args[i]))
          removed++;
      }
      return RespParser::encode_integer(removed);
    }

    // ==========================================
    // 4. LIST OPERATIONS
    // ==========================================
    else if (op == "LPUSH") {
      if (args.size() < 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'lpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        len = obj->push_left_list(args[i]);
      }
      return RespParser::encode_integer(len);
    } else if (op == "RPUSH") {
      if (args.size() < 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'rpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        len = obj->push_right_list(args[i]);
      }
      return RespParser::encode_integer(len);
    } else if (op == "LPOP") {
      if (args.size() != 1)
        return RespParser::encode_error("wrong number of arguments for 'lpop'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      std::string val = obj->pop_left_list();
      if (val == "Error:empty")
        return RespParser::encode_null();
      return RespParser::encode_bulk_string(val);
    }
    // ==========================================
    // 4. LIST OPERATIONS
    // ==========================================
    else if (op == "LPUSH") {
      if (args.size() < 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'lpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        len = obj->push_left_list(args[i]);
      }
      return RespParser::encode_integer(len);
    } else if (op == "RPUSH") {
      if (args.size() < 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'rpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        len = obj->push_right_list(args[i]);
      }
      return RespParser::encode_integer(len);
    } else if (op == "LPOP") {
      if (args.size() != 1)
        return RespParser::encode_error("wrong number of arguments for 'lpop'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();
      std::string val = obj->pop_left_list();
      if (val == "Error:empty")
        return RespParser::encode_null();
      return RespParser::encode_bulk_string(val);
    } else if (op == "RPOP") {
      if (args.size() != 1)
        return RespParser::encode_error("wrong number of arguments for 'rpop'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();

      // Assuming this method exists in Wrap_object
      std::string val = obj->pop_right_list();
      if (val == "Error:empty")
        return RespParser::encode_null();
      return RespParser::encode_bulk_string(val);
    } else if (op == "LINDEX") {
      if (args.size() != 2)
        return RespParser::encode_error(
            "wrong number of arguments for 'lindex'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_null();

      try {
        int64_t index = std::stoll(args[1]);
        // Assuming you have a get_list_index or similar method
        std::string val = obj->get_by_index_list(index);

        // You'll need to check how your wrapper returns an out-of-bounds error
        if (val == "Error:out_of_bounds" || val == "not found") {
          return RespParser::encode_null();
        }
        return RespParser::encode_bulk_string(val);
      } catch (...) {
        return RespParser::encode_error(
            "value is not an integer or out of range");
      }
    } else if (op == "LLEN") {
      if (args.size() != 1)
        return RespParser::encode_error("wrong number of arguments for 'llen'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_integer(0);
      return RespParser::encode_integer(obj->length());
    } else if (op == "LRANGE") {
      if (args.size() != 3)
        return RespParser::encode_error(
            "wrong number of arguments for 'lrange'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return RespParser::encode_array({});

      try {
        int64_t start = std::stoll(args[1]);
        int64_t end = std::stoll(args[2]);
        auto range = obj->get_range_list(start, end);
        if (range.has_value()) {
          return RespParser::encode_array(range.value());
        }
      } catch (...) {
        return RespParser::encode_error(
            "value is not an integer or out of range");
      }
    }

    return RespParser::encode_error("unknown command '" + op + "'");
  }
};
