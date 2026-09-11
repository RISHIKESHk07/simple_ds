#pragma once
#include <chrono>
#include <iostream>

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

  bool compacting_job = false;
  void start_compactor(compact_type &ct) {
    if (comp_algo != compact_type::NONE)
      return;
    comp_algo = ct;
    start_timpestamp = std::chrono::steady_clock::now();
  }
  void set_running_state(bool ty) { compacting_job = ty; }
  bool is_active() { return compacting_job; }
  void collect_metadata_job();
  bool merge_sstables();
  void condition_variable_lock();
  void notify_condition_variable();
};
