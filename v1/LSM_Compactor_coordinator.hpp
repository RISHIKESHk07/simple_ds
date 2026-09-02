#pragma once
#include "./LSM_compactor.hpp"
#include <queue>

class Compactor_coordinator {
  enum class Job_status { ERROR, QUEUED, COMPLETED, RETRY };
  struct Job {
    Job_status jbs;
    char sstable_1[128];
    char sstable_2[128];
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char sstable_res[128];
  };
  std::mutex metadata_lock;
  std::mutex job_mutex;
  bool active = false;
  std::vector<Compactor *> compactor_list;
  std::queue<Job> job_list; // a list of jobs which are given to compactors;

public:
  Compactor_coordinator(std::vector<Run> &, std::vector<Bucket> &,
                        std::vector<sstable_info> &,
                        std::vector<Reader_info> &) {};
  bool register_compactor_worker(Compactor *c) {
    if (c.is_active) {
      {
        std::lock_guard<std::mutex> lock(metadata_lock);
        compactor_list.push_back(c);
      }
      return true;
    }

    return false;
  }
  bool unregister_compactor_worker(Compactor *c) {
    if (c.is_active) {
      {
        std::lock_guard<std::mutex> lock(metadata_lock);
        auto it = std::find(compactor_list.begin(), compactor_list.end(), c);
        compactor_list.erase(it);
      }
      return true;
    }

    return false;
  }
  bool set_active() {
    {
      std::lock_guard<std::mutex> lock(metadata_lock);
      active = true;
    }
    if (active)
      return true;
    return false;
  }
  void start_job_proxying() {
    // select any idle compactor thread which is registered
    // assign it a job of compaction which is essentially one of merge operation
    // between two runs , for now we will assign all jobs to single compactor
    // *the two stateless workers only know that merging operation , state
    // managed by coordinator like checkingif compaction is allowed , any
    // readers are present and is it safe to remove ssatble , which compactor is
    // avaible for next work to be given too*
  };
  void find_overlaps_in_runs();

  void shutdown();
};
