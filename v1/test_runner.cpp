#include "IN-MEM_Helper.hpp"
#include "IN-MEM_Interface.hpp"
#include "RESP_parser.hpp"
#include <iostream>
#include <string>
#include <vector>

template <Memtable_tree_interfaces TreeType = Memtable_tree_interfaces::AVL>
class MemtableTestHarness {
private:
  Dispatch_Executor<TreeType> executor;
  std::string rx_buffer;

public:
  // Helper to turn command vector into wire format, parse via RespParser, &
  // execute
  std::string run_cmd(const std::vector<std::string> &cmd_tokens) {
    std::string wire_bytes = RespParser::encode_array(cmd_tokens);
    rx_buffer.append(wire_bytes);

    RespCommand cmd;
    size_t bytes_read = 0;

    ParseStatus status = RespParser::parse(rx_buffer, cmd, bytes_read);

    if (status == ParseStatus::SUCCESS) {
      rx_buffer.erase(0, bytes_read);
      // Calls your executor passing the parsed RespCommand
      return executor.execute_command(cmd);
    } else if (status == ParseStatus::INCOMPLETE) {
      return "-ERR Incomplete command buffer\r\n";
    } else {
      rx_buffer.clear();
      return "-ERR Parse protocol error\r\n";
    }
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
};

// ============================================================================
// MAIN EXECUTION SUITE
// ============================================================================

int main() {
  MemtableTestHarness<> harness;

  std::cout << "====================================================\n";
  std::cout << "   RUNNING MEMTABLE RESP PARSER & EXECUTOR TESTS    \n";
  std::cout << "====================================================\n\n";

  // ------------------------------------------------------------------------
  // 1. STRINGS (SET, GET, INCRBY)
  // ------------------------------------------------------------------------
  std::cout << ">>> [1] STRING SCHEME TESTS <<<\n";
  std::cout << "SET user:score 100    -> "
            << harness.run_cmd({"SET", "user:score", "100"});
  std::cout << "INCRBY user:score 50  -> "
            << harness.run_cmd({"INCRBY", "user:score", "50"});
  std::cout << "GET user:score        -> "
            << harness.run_cmd({"GET", "user:score"});

  harness.inspect_key("user:score");

  // ------------------------------------------------------------------------
  // 2. HASHES (HSET, HGET, HDEL)
  // ------------------------------------------------------------------------
  std::cout << ">>> [2] HASH SCHEME TESTS <<<\n";
  std::cout << harness.run_cmd({"HSET", "user:1000", "name", "Alice"})
            << harness.run_cmd(
                   {"HSET", "user:1000", "email", "alice@example.com"})
            << harness.run_cmd({"HSET", "user:1000", "role", "admin"})
            << std::endl;

  std::cout << "HGET user:1000 email -> "
            << harness.run_cmd({"HGET", "user:1000", "email"});

  std::cout << "\n--- State after insertions ---\n";
  harness.inspect_key("user:1000");

  std::cout << "HDEL user:1000 email -> "
            << harness.run_cmd({"HDEL", "user:1000", "email"});

  std::cout << "--- State after deleting 'email' field ---\n";
  harness.inspect_key("user:1000");

  // ------------------------------------------------------------------------
  // 3. LISTS (LPUSH, RPUSH, LPOP, RPOP, LINDEX, LLEN, LRANGE)
  // ------------------------------------------------------------------------
  std::cout << ">>> [3] LIST SCHEME TESTS <<<\n";
  harness.run_cmd({"RPUSH", "queue", "job1"});
  harness.run_cmd({"RPUSH", "queue", "job2"});
  harness.run_cmd({"LPUSH", "queue", "urgent_job0"});

  std::cout << "LLEN queue      -> " << harness.run_cmd({"LLEN", "queue"});
  std::cout << "LINDEX queue 0  -> "
            << harness.run_cmd({"LINDEX", "queue", "0"});
  std::cout << "LRANGE queue 0 2-> "
            << harness.run_cmd({"LRANGE", "queue", "0", "2"});

  std::cout << "\n--- State after LPUSH & RPUSH ---\n";
  harness.inspect_key("queue");

  std::cout << "LPOP queue      -> " << harness.run_cmd({"LPOP", "queue"});
  std::cout << "RPOP queue      -> " << harness.run_cmd({"RPOP", "queue"});

  std::cout << "--- State after LPOP & RPOP ---\n";
  harness.inspect_key("queue");

  // ------------------------------------------------------------------------
  // 4. SORTED SETS (ZADD, ZSCORE, ZRANK, ZRANGE, ZREM)
  // ------------------------------------------------------------------------
  std::cout << ">>> [4] SORTED SET (ZSET) TESTS <<<\n";
  harness.run_cmd({"ZADD", "highscores", "100.5", "Player1"});
  harness.run_cmd({"ZADD", "highscores", "250.0", "Player2"});
  harness.run_cmd({"ZADD", "highscores", "75.0", "Player3"});

  std::cout << "ZSCORE highscores Player2 -> "
            << harness.run_cmd({"ZSCORE", "highscores", "Player2"});
  std::cout << "ZRANK highscores Player1  -> "
            << harness.run_cmd({"ZRANK", "highscores", "Player1"});
  std::cout << "ZRANGE highscores 0 2     -> "
            << harness.run_cmd({"ZRANGE", "highscores", "1", "3"});

  std::cout << "\n--- State after ZADD ---\n";
  harness.inspect_key("highscores");

  std::cout << "ZREM highscores Player3   -> "
            << harness.run_cmd({"ZREM", "highscores", "Player3"});

  std::cout << "--- State after ZREM Player3 ---\n";
  harness.inspect_key("highscores");

  std::cout << "====================================================\n";
  std::cout << "           ALL COMMAND TESTS EXECUTED               \n";
  std::cout << "====================================================\n";

  return 0;
}
