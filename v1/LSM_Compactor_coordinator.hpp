#pragma once
#include "./LSM_Engine.hpp"
#include "./LSM_compactor.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <queue>
#include <set>
// using this data structure for in-machine thread consumption pattern , in case
// of cross-machine we need adapters over some client and server which will allw
// it access this

class Compactor_coordinator {
  enum class Job_status {
    ERROR,
    QUEUED,
    COMPLETED,
    RETRY,
    PRUNED_DUE_SHUTDOWN
  };
  struct Job {
    int id_;
    int acive_job_id_;
    Job_status jbs;
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    std::vector<std::set<std::string>> compaction_set_groups;
    char sstable_res[128];
  }; // compaction job given to a compactors
  struct Active_Job {
    int id_;
    std::vector<int> all_jobs_related_cleared;
    int number_of_compact_jobs;
    int id_bucket_to_compact;
    Job_status jbs;

  }; // This job model what coordinator would recieve when trying to compact a
     // certain bucket , but does look into how tables groups need to be merged
     // or how to paralleize them , that is done by Job

  std::mutex compactorList_mutex;
  std::mutex active_job_mutex;
  std::mutex jobs_mutex;
  std::mutex coordinator_mutex;
  std::condition_variable coordinator_cv;
  bool active = false;
  std::vector<Compactor *> compactor_list;
  std::queue<Active_Job> active_job_list;
  std::queue<Job> jobs_list;

public:
  LSMEngine *lsm;
  std::vector<Bucket> buckets;
  Compactor_coordinator(LSMEngine *lsme, std::vector<Bucket> &bucket_list,
                        std::vector<sstable_metadata_info> &)
      : lsm(lsme), buckets(bucket_list) {};
  bool register_compactor_worker(Compactor *c) {
    if (c->is_active()) {
      {
        std::lock_guard<std::mutex> lock(compactorList_mutex);
        compactor_list.push_back(c);
      }
      return true;
    }

    return false;
  }
  bool unregister_compactor_worker(Compactor *c) {
    if (c->is_active()) {
      {
        std::lock_guard<std::mutex> lock(compactorList_mutex);
        auto it = std::find(compactor_list.begin(), compactor_list.end(), c);
        compactor_list.erase(it);
      }
      return true;
    }

    return false;
  }
  bool set_active() {
    {
      std::lock_guard<std::mutex> lock(coordinator_mutex);
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
    // managed by coordinator like checking if compaction is allowed , any
    // readers are present and is it safe to remove ssatble , which compactor is
    // avaible for next work to be given too*
    int c = 0;
    while (true) {
      // append any incoming compaction requests here
      lsm->condition_var_compaction();

      {
        std::lock_guard<std::mutex> lock(active_job_mutex);
        Active_Job aj;
        aj.id_ = c;
        aj.jbs = Job_status::QUEUED;
        aj.id_bucket_to_compact = buckets[0].bucket_id;
        aj.number_of_compact_jobs = 0;
        active_job_list.push(aj);
      }
      c++;

      while (!active_job_list.empty()) {

        // Compaction jobs assigned here for every level
        for (int i = 0; i < buckets.size(); i++) {
          if (buckets[i].threshold_size <= buckets[i].runs_list.size()) {
            // we append compaction jobs
            if (!buckets[i].is_compacting)
              break;
            // find_overlap and append to job_list queue , add these active_job
            // counter
            std::vector<Job> temp_jobs = find_overlaps_in_runs(
                buckets[i].runs_list, active_job_list.front().id_);
            {
              std::lock_guard<std::mutex> lock(jobs_mutex);
              for (auto i : temp_jobs) {
                jobs_list.push(i);
                active_job_list.front().number_of_compact_jobs++;
                active_job_list.front().all_jobs_related_cleared.push_back(
                    i.id_);
              }
            }

            continue;
          }
          break;
        }

        // compactor assigned here , we wait for all current jobs to be assigned
        // , and done remove the current active job into retry queue or maybe
        // consider completed
      }
    }
  };
  std::vector<Job> find_overlaps_in_runs(std::vector<Run *> v1, int active_id) {

    int i = 0;
    int k = 0;
    std::set<std::string> active_set;
    std::vector<std::pair<std::pair<std::string, std::string>, bool>> vec;
    for (auto i : v1) {
      for (int y = 0; y < i->paths_to_files.size(); y++) {
        vec.push_back({{i->first_key_run[y], i->paths_to_files[y]}, false});
        vec.push_back({{i->last_key_run[y], i->paths_to_files[y]}, true});
      }
    }

    std::sort(vec.begin(), vec.end(),
              [](std::pair<std::pair<std::string, std::string>, bool> &a,
                 std::pair<std::pair<std::string, std::string>, bool> &b) {
                if (a.first.first > b.first.first)
                  return true;
                return false;
              });

    int counter = 0;
    std::vector<Job> j;
    for (int u = 0; u < vec.size(); u++) {

      if (vec[u].second == false) {
        counter++;
      } else {
        active_set.insert(vec[u].first.second);
        counter--;
      }

      if (counter == 0) {
        Job active_j;
        active_j.acive_job_id_ = active_id;
        active_j.jbs = Job_status::QUEUED;
        active_j.compaction_set_groups.push_back(active_set);
        j.push_back(active_j);
        active_set.clear();
      }
    }

    return j;
  };

  void dispatch_workers() {
    // select any idle compactors here
    std::vector<Compactor *> compactor_avaliable;

    {
      std::lock_guard<std::mutex> lock(compactorList_mutex);
      for (auto i : compactor_list) {
        if (i->is_active()) {
          compactor_avaliable.push_back(i);
        }
      }
    }
    // distribute work over them as the respond using their blocking mechanism
    while (!jobs_list.empty()) {
      Compactor *active_comp = nullptr;
      Job jb;
      {
        std::unique_lock<std::mutex> lock(coordinator_mutex);
        coordinator_cv.wait(lock, [&compactor_avaliable] {
          return (compactor_avaliable.size() > 0);
        });
        active_comp = compactor_avaliable.back();
        active_comp->set_running_state(true);
        compactor_avaliable.pop_back();
        lock.unlock();
      }
      {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        jb = jobs_list.front();
        jobs_list.pop();
      }

      stream_data_blocks_using_iterator_per_Job(active_comp, jb);
    }
  }

  void stream_data_blocks_using_iterator_per_Job(Compactor *c, Job j) {
    // stream data to compactor but first using iterators to fetch blocks groups
    // , until all iterators are exhausted , each group is forwarded to a
    // temp_queue for threads on same device / shared memory for workers in same
    // machine / for cross machines we need a message broker to help , add
    // simple start/end datablocks for telling compactors are done , streaming
    // how data will require its own recovery process as well for durability
    auto s = std::make_shared<MessagingQueue<Data_chunk>>();
    auto r = std::make_shared<MessagingQueue<Response_chunk>>();
    c->set_message_queues(s, r);
    int counter_segments_pushed_complete = 0;
    int number_of_iterators = 0;
    std::vector<LSM_Iterator *> lsm_iterators;
    for (auto i : j.compaction_set_groups) {
      for (auto k : i) {
        auto temp = new LSM_Iterator(this->lsm, k);
        lsm_iterators.push_back(temp);
      }
    }
    number_of_iterators = lsm_iterators.size();
    while (counter_segments_pushed_complete != number_of_iterators) {
      for (auto k : lsm_iterators) {
        Data_chunk dc;
        auto it = k->get_iterator();
        auto res2 = it->next();
        if (!res2.first)
          counter_segments_pushed_complete++;

        dc.kvroup = res2.second;
        dc.num_rows = res2.second.size();
        dc.job_id = j.id_;
        dc.active_job_id = j.acive_job_id_;
        s->push_data(dc);
      }
    }
  }

  void shutdown();
};
