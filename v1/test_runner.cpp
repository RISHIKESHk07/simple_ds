#include "IN-MEM_Helper.hpp"
#include "IN-MEM_Interface.hpp"
#include "RESP_parser.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

template <Memtable_tree_interfaces TreeType = Memtable_tree_interfaces::AVL>
class MemtableTestHarness {
private:
  Dispatch_Executor<TreeType> executor;
  std::string rx_buffer;

public:
  // Runs a command through the network wire parser + executor (returns raw
  // RESP)
  std::string run_cmd_wire(const std::vector<std::string> &cmd_tokens) {
    // std::string wire_bytes = RespParser::(cmd_tokens);
    // rx_buffer.append(wire_bytes);

    RespCommand cmd;
    size_t bytes_read = 0;

    ParseStatus status = RespParser::parse(rx_buffer, cmd, bytes_read);

    if (status == ParseStatus::SUCCESS) {
      rx_buffer.erase(0, bytes_read);
      return executor.execute_command(cmd);
    } else if (status == ParseStatus::INCOMPLETE) {
      return "-ERR Incomplete command buffer\r\n";
    } else {
      rx_buffer.clear();
      return "-ERR Parse protocol error\r\n";
    }
  }

  // Directly executes command returning the structured CmdResponse
  CmdResponse run_cmd_direct(const std::vector<std::string> &cmd_tokens) {
    if (cmd_tokens.empty()) {
      return CmdResponse::InvalidArgs("Empty command");
    }
    RespCommand cmd;
    cmd.name = cmd_tokens[0];
    cmd.args.assign(cmd_tokens.begin() + 1, cmd_tokens.end());
    return executor.execute_command_internal(cmd);
  }

  // Pretty-print state inspector directly from memtable
  void inspect_key(const std::string &key_str) {
    Wrap_object *obj = executor.get_private_wrap_obj(key_str);
    std::cout << "----------------------------------------\n";
    std::cout << " INSPECTING KEY: " << key_str << "\n";
    if (!obj) {
      std::cout << " [KEY DOES NOT EXIST / DELETED]\n";
      std::cout << "----------------------------------------\n\n";
      return;
    }
    obj->pretty_print(std::cout);
    std::cout << "\n";
  }

  Wrap_object *get_raw_object(const std::string &key) {
    return executor.get_private_wrap_obj(key);
  }
  void print_memtable() { executor.print_memtable(); }
};

// ============================================================================
// MAIN EXECUTION & ASSERTION SUITE
// ============================================================================

int main() {
  MemtableTestHarness<> harness;

  std::cout << "====================================================\n";
  std::cout << "   RUNNING MEMTABLE RESP PARSER & EXECUTOR TESTS    \n";
  std::cout << "====================================================\n\n";

  // ------------------------------------------------------------------------
  // 1. STRINGS (SET, GET, INCRBY, TYPE ERRORS)
  // ------------------------------------------------------------------------
  std::cout << ">>> [1] STRING SCHEME TESTS <<<\n";

  // Basic SET & GET
  auto set_res = harness.run_cmd_direct({"SET", "user:score", "100"});
  assert(set_res.status == ResponseStatus::SUCCESS);
  assert(set_res.string_val == "OK");

  auto incr_res = harness.run_cmd_direct({"INCRBY", "user:score", "50"});
  assert(incr_res.status == ResponseStatus::SUCCESS);
  assert(incr_res.int_val == 150);

  auto get_res = harness.run_cmd_direct({"GET", "user:score"});
  assert(get_res.status == ResponseStatus::SUCCESS);
  assert(get_res.string_val == "150");

  // Wire format check
  auto wire_get = harness.run_cmd_direct({"GET", "user:score"});
  assert(wire_get.to_resp() == "$3\r\n150\r\n");

  // Non-existent key lookup
  auto missing_get = harness.run_cmd_direct({"GET", "non_existent_key"});
  assert(missing_get.status == ResponseStatus::NOT_FOUND);
  assert(missing_get.data_type == CmdResponse::DataType::NIL);

  // Argument count validation
  auto err_arg = harness.run_cmd_direct({"SET"});
  assert(err_arg.status == ResponseStatus::INVALID_ARGS);

  harness.inspect_key("user:score");

  // ------------------------------------------------------------------------
  // 2. HASHES (HSET, HGET, HDEL, HMSET, HMGET)
  // ------------------------------------------------------------------------
  std::cout << ">>> [2] HASH SCHEME TESTS <<<\n";

  auto hset_1 = harness.run_cmd_direct({"HSET", "user:1000", "name", "Alice"});
  assert(hset_1.status == ResponseStatus::SUCCESS);
  assert(hset_1.int_val == 1); // New field added

  auto hset_2 =
      harness.run_cmd_direct({"HSET", "user:1000", "name", "Alice_V2"});
  assert(hset_2.status == ResponseStatus::SUCCESS);
  assert(hset_2.int_val == 0); // Field updated, not new

  // Batch Hash Set (HMSET)
  auto hmset_res = harness.run_cmd_direct(
      {"HMSET", "user:1000", "email", "alice@example.com", "role", "admin"});
  assert(hmset_res.status == ResponseStatus::SUCCESS);
  assert(hmset_res.int_val == 2);

  // Batch Hash Get (HMGET)
  auto hmget_res = harness.run_cmd_direct(
      {"HMGET", "user:1000", "name", "email", "non_existent_field"});
  assert(hmget_res.status == ResponseStatus::SUCCESS);
  assert(hmget_res.array_val.size() == 3);
  assert(hmget_res.array_val[0] == "Alice_V2");
  assert(hmget_res.array_val[1] == "alice@example.com");
  assert(hmget_res.array_val[2] ==
         ""); // Missing field maps to empty string payload

  // HDEL
  auto hdel_res = harness.run_cmd_direct({"HDEL", "user:1000", "email"});
  assert(hdel_res.status == ResponseStatus::SUCCESS);
  assert(hdel_res.int_val == 1);

  harness.inspect_key("user:1000");

  // ------------------------------------------------------------------------
  // 3. LISTS (LPUSH, RPUSH, LPOP, RPOP, LINDEX, LLEN, LRANGE)
  // ------------------------------------------------------------------------
  std::cout << ">>> [3] LIST SCHEME TESTS <<<\n";

  assert(harness.run_cmd_direct({"RPUSH", "queue", "job1"}).int_val == 1);
  assert(harness.run_cmd_direct({"RPUSH", "queue", "job2"}).int_val == 2);
  assert(harness.run_cmd_direct({"LPUSH", "queue", "urgent_job0"}).int_val ==
         3);

  // LLEN & LINDEX
  auto llen_res = harness.run_cmd_direct({"LLEN", "queue"});
  assert(llen_res.status == ResponseStatus::SUCCESS && llen_res.int_val == 3);

  auto lindex_res = harness.run_cmd_direct({"LINDEX", "queue", "0"});
  assert(lindex_res.status == ResponseStatus::SUCCESS &&
         lindex_res.string_val == "urgent_job0");

  // Invalid index format error
  auto lindex_err = harness.run_cmd_direct({"LINDEX", "queue", "not_a_number"});
  assert(lindex_err.status == ResponseStatus::RUNTIME_ERROR);

  // LRANGE
  auto lrange_res = harness.run_cmd_direct({"LRANGE", "queue", "0", "2"});
  assert(lrange_res.status == ResponseStatus::SUCCESS);
  assert(lrange_res.array_val.size() == 3);
  assert(lrange_res.array_val[0] == "urgent_job0");

  // LPOP & RPOP
  assert(harness.run_cmd_direct({"LPOP", "queue"}).string_val == "urgent_job0");
  assert(harness.run_cmd_direct({"RPOP", "queue"}).string_val == "job2");
  assert(harness.run_cmd_direct({"LLEN", "queue"}).int_val == 1);

  harness.inspect_key("queue");

  // ------------------------------------------------------------------------
  // 4. SORTED SETS (ZADD, ZSCORE, ZRANK, ZRANGE, ZINCRBY, ZREM)
  // ------------------------------------------------------------------------
  std::cout << ">>> [4] SORTED SET (ZSET) TESTS <<<\n";

  auto zadd_res =
      harness.run_cmd_direct({"ZADD", "highscores", "100.5", "Player1", "250.0",
                              "Player2", "75.0", "Player3"});
  assert(zadd_res.status == ResponseStatus::SUCCESS);
  assert(zadd_res.int_val == 3);

  // Invalid score validation
  auto zadd_err = harness.run_cmd_direct(
      {"ZADD", "highscores", "invalid_score", "Player4"});
  assert(zadd_err.status == ResponseStatus::RUNTIME_ERROR);

  // ZSCORE & ZRANK
  auto zscore_res = harness.run_cmd_direct({"ZSCORE", "highscores", "Player2"});
  assert(zscore_res.status == ResponseStatus::SUCCESS);
  assert(zscore_res.double_val == 250.0);

  // ZINCRBY
  auto zincr_res =
      harness.run_cmd_direct({"ZINCRBY", "highscores", "50.5", "Player1"});
  assert(zincr_res.status == ResponseStatus::SUCCESS);
  assert(zincr_res.double_val == 151.0); // 100.5 + 50.5

  // ZRANGE WITHSCORES , important note we use 1-index , so search starts from 1
  // not 0 .
  auto zrange_res =
      harness.run_cmd_direct({"ZRANGE", "highscores", "1", "3", "WITHSCORES"});
  assert(zrange_res.status == ResponseStatus::SUCCESS);
  assert(!zrange_res.array_val.empty());

  // ZREM
  auto zrem_res = harness.run_cmd_direct({"ZREM", "highscores", "Player3"});
  assert(zrem_res.status == ResponseStatus::SUCCESS);
  assert(zrem_res.int_val == 1);

  harness.inspect_key("highscores");

  // ------------------------------------------------------------------------
  // 5. EDGE CASES & WIRE PROTOCOL VALIDATION
  // ------------------------------------------------------------------------
  std::cout << ">>> [5] WIRE PROTOCOL & ERROR VALIDATION <<<\n";

  // Unknown Command Test
  // std::string unknown_resp = harness.run_cmd_wire({"FLUSHALL"});
  // assert(unknown_resp == "-ERR unknown command buffer\r\n");

  // Invalid Argument Count Test
  auto invalid_args_resp = harness.run_cmd_direct({"HSET", "only_one_arg"});
  assert(invalid_args_resp.err_message ==
         "wrong number of arguments for 'hset'");

  harness.print_memtable();

  std::cout << "====================================================\n";
  std::cout << "   ALL ASSERTIONS PASSED & TEST SUITE COMPLETED    \n";
  std::cout << "====================================================\n";

  return 0;
}
