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

// Network parsing statuses
enum class ParseStatus {
  SUCCESS,
  INCOMPLETE, // Network buffer split — wait for more socket bytes
  ERROR       // Invalid RESP syntax
};

struct RespCommand {
  std::string name;              // e.g., "ZADD", "HSET", "GET"
  std::vector<std::string> args; // e.g., ["myzset", "100", "Alice"]
};

// ==========================================
// RESPONSE OBJECT & STATE DEFINITION
// ==========================================
enum class ResponseStatus {
  SUCCESS,
  NOT_FOUND,
  INVALID_ARGS,
  WRAP_OBJ_ERROR,
  RUNTIME_ERROR
};

struct CmdResponse {
  ResponseStatus status{ResponseStatus::SUCCESS};
  std::string err_message;

  // Encapsulates possible success data representations
  enum class DataType {
    NIL,
    SIMPLE_STRING,
    BULK_STRING,
    INTEGER,
    DOUBLE,
    ARRAY
  } data_type{DataType::NIL};

  std::string string_val;
  int64_t int_val{0};
  double double_val{0.0};
  std::vector<std::string> array_val;

  // --- Constructors ---
  CmdResponse() = default;

  static CmdResponse Ok() {
    CmdResponse r;
    r.status = ResponseStatus::SUCCESS;
    r.data_type = DataType::SIMPLE_STRING;
    r.string_val = "OK";
    return r;
  }

  static CmdResponse Nil() {
    CmdResponse r;
    r.status = ResponseStatus::NOT_FOUND;
    r.data_type = DataType::NIL;
    return r;
  }

  static CmdResponse Bulk(std::string val) {
    CmdResponse r;
    r.status = ResponseStatus::SUCCESS;
    r.data_type = DataType::BULK_STRING;
    r.string_val = std::move(val);
    return r;
  }

  static CmdResponse Integer(int64_t val) {
    CmdResponse r;
    r.status = ResponseStatus::SUCCESS;
    r.data_type = DataType::INTEGER;
    r.int_val = val;
    return r;
  }

  static CmdResponse Double(double val) {
    CmdResponse r;
    r.status = ResponseStatus::SUCCESS;
    r.data_type = DataType::DOUBLE;
    r.double_val = val;
    return r;
  }

  static CmdResponse Array(std::vector<std::string> arr) {
    CmdResponse r;
    r.status = ResponseStatus::SUCCESS;
    r.data_type = DataType::ARRAY;
    r.array_val = std::move(arr);
    return r;
  }

  static CmdResponse InvalidArgs(const std::string &msg) {
    CmdResponse r;
    r.status = ResponseStatus::INVALID_ARGS;
    r.err_message = msg;
    return r;
  }

  static CmdResponse WrapError(const std::string &msg) {
    CmdResponse r;
    r.status = ResponseStatus::WRAP_OBJ_ERROR;
    r.err_message = msg;
    return r;
  }

  static CmdResponse RuntimeError(const std::string &msg) {
    CmdResponse r;
    r.status = ResponseStatus::RUNTIME_ERROR;
    r.err_message = msg;
    return r;
  }

  // --- RESP Serialization Helper ---
  [[nodiscard]] std::string to_resp() const {
    if (status != ResponseStatus::SUCCESS &&
        status != ResponseStatus::NOT_FOUND) {
      return "-ERR " + err_message + "\r\n";
    }

    if (status == ResponseStatus::NOT_FOUND && data_type == DataType::NIL) {
      return "$-1\r\n";
    }

    switch (data_type) {
    case DataType::SIMPLE_STRING:
      return "+" + string_val + "\r\n";
    case DataType::BULK_STRING:
      return "$" + std::to_string(string_val.size()) + "\r\n" + string_val +
             "\r\n";
    case DataType::INTEGER:
      return ":" + std::to_string(int_val) + "\r\n";
    case DataType::DOUBLE: {
      std::string s = std::to_string(double_val);
      return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
    }
    case DataType::ARRAY: {
      std::string res = "*" + std::to_string(array_val.size()) + "\r\n";
      for (const auto &item : array_val) {
        res += "$" + std::to_string(item.size()) + "\r\n" + item + "\r\n";
      }
      return res;
    }
    case DataType::NIL:
    default:
      return "$-1\r\n";
    }
  }
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
};

template <Memtable_tree_interfaces TreeType = Memtable_tree_interfaces::AVL>
class Dispatch_Executor {
private:
  using MemtableType =
      Memtable<TreeType, DBKey, Wrap_object *, DBKeyComparator>;
  MemtableType memtable;

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
  void print_memtable() { return this->memtable.print_memtable(); }
  Wrap_object *get_private_wrap_obj(const std::string &key_str) {
    return this->get(key_str);
  }

  // Returns internal status object for programatic usage
  CmdResponse execute_command_internal(const RespCommand &cmd) {
    const std::string &op = cmd.name;
    const auto &args = cmd.args;

    // ==========================================
    // 1. STRING OPERATIONS
    // ==========================================
    if (op == "SET") {
      if (args.size() < 2)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'set'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::STRING);
      std::string val = args[1];
      obj->create_string(val);
      return CmdResponse::Ok();
    } else if (op == "GET") {
      if (args.size() != 1)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'get'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();

      if (obj->get_string().has_value()) {
        return CmdResponse::Bulk(obj->get_string().value());
      } else {
        return CmdResponse::WrapError(
            obj->get_error().message.value_or("Wrap object error"));
      }
    } else if (op == "INCRBY") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'incrby'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::STRING);
      if (!obj)
        return CmdResponse::Nil();

      try {
        auto val = obj->incr_by(std::stoll(args[1])).value();
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());
        return CmdResponse::Integer(val);
      } catch (...) {
        return CmdResponse::RuntimeError(
            "value is not an integer or out of range");
      }
    }

    // ==========================================
    // 2. HASH OPERATIONS
    // ==========================================
    else if (op == "HSET") {
      if (args.size() != 3)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'hset'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::HASH);
      auto is_new = obj->set_field(args[1], args[2]).value();
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Integer(is_new ? 1 : 0);
    } else if (op == "HGET") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'hget'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto res = obj->get_field(args[1]).value();
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Bulk(res);
    } else if (op == "HDEL") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'hdel'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto res = obj->delete_field(args[1]);
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Integer(res.value());
    } else if (op == "HMSET") {
      std::cout << args.size() << std::endl;
      if (args.size() < 3 || args.size() % 2 != 1)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'hmset'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::HASH);
      std::vector<std::pair<std::string, std::string>> fv;
      for (size_t i = 1; i < args.size(); i += 2) {
        fv.push_back({args[i], args[i + 1]});
      }
      auto v = obj->set_multiple(fv);
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Integer(v.value().size());
    } else if (op == "HMGET") {
      if (args.size() < 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'hmget'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();

      std::vector<std::string> fields(args.begin() + 1, args.end());
      auto v = obj->get_multiple_hash(fields);
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());

      std::vector<std::string> ves;
      for (auto &i : v.value()) {
        ves.push_back(i.has_value() ? i.value() : "");
      }
      return CmdResponse::Array(ves);
    }

    // ==========================================
    // 3. SORTED SET (ZSET) OPERATIONS
    // ==========================================
    else if (op == "ZADD") {
      if (args.size() < 3 || (args.size() - 1) % 2 != 0)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'zadd'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::SORTED_SET);
      int count = 0;
      try {
        for (size_t i = 1; i < args.size(); i += 2) {
          double score = std::stod(args[i]);
          if (obj->zset_add(score, args[i + 1]).has_value())
            count++;
        }
      } catch (...) {
        return CmdResponse::RuntimeError("value is not a valid float");
      }
      return CmdResponse::Integer(count);
    } else if (op == "ZSCORE") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'zscore'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto score = obj->get_score(args[1]);
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Double(score.value());
    } else if (op == "ZRANK") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'zrank'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto r = obj->get_rank(args[1]);
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Integer(r.value());
    } else if (op == "ZRANGE") {
      if (args.size() < 3)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'zrange'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Array({});

      try {
        int64_t start = std::stoll(args[1]);
        int64_t end = std::stoll(args[2]);
        bool with_scores = (args.size() > 3 && args[3] == "WITHSCORES");

        auto range = obj->get_range_by_rank(start, end);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());

        std::vector<std::string> resp_list;
        for (const auto &[member, score] : range.value()) {
          resp_list.push_back(member);
          if (with_scores)
            resp_list.push_back(std::to_string(score));
        }
        return CmdResponse::Array(resp_list);
      } catch (...) {
        return CmdResponse::RuntimeError(
            "value is not an integer or out of range");
      }
    } else if (op == "ZRANGEBYSCORE") {
      if (args.size() < 3)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'zrangebyscore'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Array({});

      try {
        double min_score = std::stod(args[1]);
        double max_score = std::stod(args[2]);

        auto range = obj->get_range_by_score(min_score, max_score);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());

        std::vector<std::string> resp_list;
        for (const auto &[member, score] : range.value()) {
          resp_list.push_back(member);
        }
        return CmdResponse::Array(resp_list);
      } catch (...) {
        return CmdResponse::RuntimeError("min or max is not a float");
      }
    } else if (op == "ZINCRBY") {
      if (args.size() != 3)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'zincrby'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::SORTED_SET);
      try {
        double delta = std::stod(args[1]);
        auto new_score = obj->update_increase_by_delta(delta, args[2]);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());
        return CmdResponse::Double(new_score.value());
      } catch (...) {
        return CmdResponse::RuntimeError("value is not a valid float");
      }
    } else if (op == "ZREM") {
      if (args.size() < 2)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'zrem'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Integer(0);

      int removed = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        if (obj->remove(args[i]))
          removed++;
      }
      return CmdResponse::Integer(removed);
    }

    // ==========================================
    // 4. LIST OPERATIONS
    // ==========================================
    else if (op == "LPUSH") {
      if (args.size() < 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'lpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        auto new_l = obj->push_left_list(args[i]);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());
        len = new_l.value();
      }
      return CmdResponse::Integer(len);
    } else if (op == "RPUSH") {
      if (args.size() < 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'rpush'");
      Wrap_object *obj = get_or_create(args[0], Wrap_object_type::LIST);
      size_t len = 0;
      for (size_t i = 1; i < args.size(); ++i) {
        auto new_l = obj->push_right_list(args[i]);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());
        len = new_l.value();
      }
      return CmdResponse::Integer(len);
    } else if (op == "LPOP") {
      if (args.size() != 1)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'lpop'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto val = obj->pop_left_list();
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Bulk(val.value());
    } else if (op == "RPOP") {
      if (args.size() != 1)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'rpop'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();
      auto val = obj->pop_right_list();
      if (obj->get_error().has_error())
        return CmdResponse::WrapError(obj->get_error().message.value());
      return CmdResponse::Bulk(val.value());
    } else if (op == "LINDEX") {
      if (args.size() != 2)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'lindex'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Nil();

      try {
        int64_t index = std::stoll(args[1]);
        auto val = obj->get_by_index_list(index);
        if (obj->get_error().has_error())
          return CmdResponse::WrapError(obj->get_error().message.value());

        return CmdResponse::Bulk(val.value());
      } catch (...) {
        return CmdResponse::RuntimeError(
            "value is not an integer or out of range");
      }
    } else if (op == "LLEN") {
      if (args.size() != 1)
        return CmdResponse::InvalidArgs("wrong number of arguments for 'llen'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Integer(0);
      return CmdResponse::Integer(obj->length().value());
    } else if (op == "LRANGE") {
      if (args.size() != 3)
        return CmdResponse::InvalidArgs(
            "wrong number of arguments for 'lrange'");
      Wrap_object *obj = get(args[0]);
      if (!obj)
        return CmdResponse::Array({});

      try {
        int64_t start = std::stoll(args[1]);
        int64_t end = std::stoll(args[2]);
        auto range = obj->get_range_list(start, end);
        if (range.has_value()) {
          return CmdResponse::Array(range.value());
        }
        return CmdResponse::WrapError(obj->get_error().message.value());
      } catch (...) {
        return CmdResponse::RuntimeError(
            "value is not an integer or out of range");
      }
    }

    return CmdResponse::RuntimeError("unknown command '" + op + "'");
  }

  // Backwards-compatible primary entrypoint returning raw RESP network string
  std::string execute_command(const RespCommand &cmd) {
    return execute_command_internal(cmd).to_resp();
  }
};
