#pragma once
#include "LSM_Engine.hpp"
#include <chrono>
#include <iostream>
#include <memory>

template <typename T>
class MessagingQueue : std::enable_shared_from_this<MessagingQueue<T>> {
  int counter_of_items = 0;
  std::shared_ptr<std::queue<T>> MQ;

public:
  bool has_next_Items() {
    if (!MQ->empty())
      return true;
    else
      return false;
  }
  T pull_data() {
    auto t = MQ->front();
    MQ.pop();
    return t;
  }
  std::vector<T> pull_data_grp(int size) {
    if (size > MQ->size())
      return {};

    std::vector<T> temp_vec;
    for (int i = 0; i < size; i++) {
      auto t1 = pull_data();
      temp_vec.push_back(t1);
    }

    return temp_vec;
  }
  bool push_data(T v) {
    MQ->push(v);
    return true;
  }
};

struct Data_chunk {
  int job_id;
  int active_job_id;
  int num_rows;
  int sf;
  int ef;
  std::vector<key_value> kvroup;
};

struct Response_chunk {
  int job_id;
  int num_rows_total_processed;
  int response_timestamp;
  std::string final_res_file_path;
};

class Compactor {
  // As of now we will be following the incremental compaction strategy , which
  // is a simple variant of the STCS which also uses some metadata inorder to
  // control the various issues when we blindy use the above strategies
  // Implmentation will be kept as decoupled as possible , allowing for future
  // reuse especially merging process will with sstables as units
  //
  // Th main crux here is we will be using a unit of work called fragment which
  // is our sstable , we will always compact a group of ordered sstables which
  // is a logical abstraction called a run , when we compact runs and place
  // their results into subsequent buckets , bucket willl be using file_size as
  // metrics as its more easier attribute to check if compaction is immediately
  // needed .

  Compactor() { std::cout << "Initialised compactor" << std::endl; };

public:
  enum class compact_type { ICS, NONE };
  int id_;
  std::string compactor_code;
  compact_type comp_algo;
  std::chrono::steady_clock::time_point start_timpestamp;
  std::chrono::steady_clock::time_point end_timestamp;
  std::chrono::steady_clock::time_point active_job_start;
  std::chrono::steady_clock::time_point active_job_end;
  std::shared_ptr<MessagingQueue<Data_chunk>> sendQ;
  std::shared_ptr<MessagingQueue<Response_chunk>> resultQ;
  bool compacting_job = false;
  void start_compactor(compact_type &ct) {
    if (comp_algo != compact_type::NONE)
      return;
    comp_algo = ct;
    start_timpestamp = std::chrono::steady_clock::now();
  }
  void set_running_state(bool ty) { compacting_job = ty; }
  bool is_active() { return compacting_job; }
  void collect_metadata_j2ob(); // write it in too a file for compactors ,
                                // append the copactor 's id_ to all messages
  Response_chunk merge_sstables();
  void set_message_queues(std::shared_ptr<MessagingQueue<Data_chunk>> s,
                          std::shared_ptr<MessagingQueue<Response_chunk>> r) {

    sendQ = s;
    resultQ = r;
  }; // set the recieving message queue
     // and result message queue
};
