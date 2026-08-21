#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

enum class error_codes {
  NONE,
  RUNTIME_ERROR,
  CORRUPTION_DETECTED,
  SYNC_FAILED,
  SHUTDOWN_ERROR
};

class error_object {
  error_codes ec = error_codes::NONE;
  std::optional<std::string> message;

public:
  void set_message(const std::string &mes) { message = mes; }
  void set_code(error_codes c) { ec = c; }
  error_codes get_code() const { return ec; }
  std::optional<std::string> get_message() const { return message; }
  bool failed() const { return ec != error_codes::NONE; }
  void reset() {
    message = std::nullopt;
    ec = error_codes::NONE;
  }
};

class ILogger {
public:
  virtual ~ILogger() = default;
  virtual void log(const std::string &level, const std::string &message) = 0;
};

class FileSink : public ILogger {
  std::ofstream log_file;
  std::mutex sink_mutex;

public:
  explicit FileSink(const std::string &filename) {
    log_file.open(filename, std::ios::out | std::ios::app);
  }

  void log(const std::string &level, const std::string &message) override {
    std::lock_guard<std::mutex> lock(sink_mutex);
    if (log_file.is_open()) {
      auto now = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());
      char buf[32];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                    std::localtime(&now));
      log_file << "[" << buf << "] [" << level << "] " << message << std::endl;
    }
  }
};

enum class sync_strategy { SYNC, SYNC_THROUGH, GROUP_COMMIT, NONE, ALWAYS };
enum class compression_strategy { NONE };

struct WAL_config {
  std::string Identity = "WAL_INSTANCE";
  std::string base_dir = "./wal_logs";
  sync_strategy ss = sync_strategy::ALWAYS;
  compression_strategy cs = compression_strategy::NONE;
  uint32_t MAX_SEG_SIZE = 100;
  uint32_t RECORD_HEADER_SIZE = 28;
  uint32_t GROUP_COMMIT_R = 30;
  uint32_t GROUP_COMMIT_T = 60; // ms
  std::shared_ptr<ILogger> logger =
      std::make_shared<FileSink>(base_dir + "/debug.log");
};

class WAL {
public:
#pragma pack(push, 1)
  enum class Record_type : uint16_t {
    SEGMENT_HEADER = 0x01,
    SEGMENT_SEAL = 0x02,
    CHECKPOINT = 0x03,
    SHUTDOWN = 0x04,
    ROW = 0x05
  };

  struct checkpoint_payload {
    char Identity[64];
    uint64_t segment_id;
    uint64_t timestamp;
    uint64_t checkpoint_lsn;
  };

  struct segment_header_payload {
    char Identity[64];
    uint64_t segment_id;
    char base_dir[128];
    uint64_t base_lsn;
    uint32_t number_of_records;
  };

  struct segment_seal {
    uint64_t segment_id;
    uint64_t final_lsn;
    uint64_t total_records;
  };

  struct shutdown_payload {
    char Identity[64];
    uint64_t timestamp;
    uint64_t final_lsn;
  };

  struct frame_record_header {
    uint64_t LSN;
    uint16_t version_number;
    Record_type record_type;
    uint64_t header_length;
    uint64_t payload_length;
  };

  struct frame_record {
    frame_record_header header;
    std::vector<unsigned char> payload;
    uint32_t checksum;
  };
#pragma pack(pop)

  struct SegmentInfo {
    uint64_t segment_id;
    std::string base_file_path;
    uint64_t base_lsn;
    uint64_t last_lsn;
    uint32_t number_of_records;
  };

  struct CheckpointInfo {
    uint64_t checkpoint_lsn;
    uint64_t timestamp;
    uint64_t segment_id;
  };

private:
  WAL_config config;
  int active_fd = -1;
  uint64_t next_lsn = 0;
  uint64_t durable_lsn = 0;
  uint64_t active_segment_space = 0;
  uint64_t current_base_lsn = 0;
  uint64_t segment_id = 0;
  uint32_t lookup_table[256];
  std::atomic<bool> is_shutdown{false};

  std::vector<SegmentInfo> segment_list;
  std::vector<CheckpointInfo> checkpoint_list;
  std::unordered_map<int, uint64_t> registered_readers;

  mutable std::mutex wal_mutex;
  std::condition_variable tail_cv;

  std::thread pruning_thread;
  std::atomic<bool> prune_var{false};
  std::mutex prune_mutex;
  std::condition_variable prune_cv;

  void log_internal(const std::string &lvl, const std::string &msg) {
    if (config.logger) {
      config.logger->log(lvl, msg);
    }
  }

  void table_preprocess() {
    for (int i = 0; i < 256; i++) {
      uint32_t crc = static_cast<uint32_t>(i);
      for (int j = 0; j < 8; j++) {
        if (crc & 1)
          crc = (crc >> 1) ^ 0x82F63B78;
        else
          crc >>= 1;
      }
      lookup_table[i] = crc;
    }
  }

  uint32_t compute_checksum_crc32c(const std::vector<unsigned char> &vr) {
    uint32_t s = 0xFFFFFFFF;
    for (auto i : vr) {
      s = (s >> 8) ^ lookup_table[static_cast<uint8_t>(s ^ i)];
    }
    return s ^ 0xFFFFFFFF;
  }

  template <typename T>
  static void append_bytes(std::vector<unsigned char> &v, T val) {
    for (int i = sizeof(val) - 1; i >= 0; i--) {
      v.push_back((val >> (8 * i)) & 0xFF);
    }
  }

  template <typename T>
  static T read_bytes(const std::vector<unsigned char> &v, size_t &offset) {
    T ret = 0;
    size_t sz = sizeof(T);
    for (size_t i = 0; i < sz; i++) {
      ret = (ret << 8) | v[offset++];
    }
    return ret;
  }

  std::vector<unsigned char> compute_byte_frame(const frame_record &fr) {
    std::vector<unsigned char> res;
    append_bytes(res, fr.header.LSN);
    append_bytes(res, fr.header.version_number);
    append_bytes(res, static_cast<uint16_t>(fr.header.record_type));
    append_bytes(res, fr.header.header_length);
    append_bytes(res, fr.header.payload_length);
    res.insert(res.end(), fr.payload.begin(), fr.payload.end());
    return res;
  }

  bool flush_sync_controller(int fd, const std::vector<unsigned char> &v) {
    if (fd < 0)
      return false;
    size_t total_written = 0;
    while (total_written < v.size()) {
      ssize_t bytes =
          write(fd, v.data() + total_written, v.size() - total_written);
      if (bytes < 0) {
        if (errno == EINTR)
          continue;
        return false;
      }
      total_written += bytes;
    }
    return true;
  }

  bool truncate_file(const std::string &path, off_t valid_length) {
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0)
      return false;
    if (ftruncate(fd, valid_length) != 0) {
      close(fd);
      return false;
    }
    fsync(fd);
    close(fd);
    return true;
  }

public:
  explicit WAL(WAL_config &conf, error_object &err) : config(conf) {
    err.reset();
    try {
      table_preprocess();
      std::filesystem::create_directories(config.base_dir);

      recover_and_rebuild(err);
      if (err.failed())
        return;

      pruning_thread = std::thread([this]() {
        // start_prune_process();
      });
      log_internal("INFO", "WAL engine initialized successfully.");
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(std::string("Constructor Exception: ") + e.what());
      log_internal("ERROR", err.get_message().value());
    } catch (...) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message("Unknown exception during WAL initialization.");
      log_internal("ERROR", err.get_message().value());
    }
  }

  ~WAL() {
    error_object err;
    shutdown(err);
  }

  bool shutdown(error_object &err) {
    err.reset();
    if (is_shutdown.exchange(true))
      return true;

    try {
      {
        std::lock_guard<std::mutex> lock(prune_mutex);
        prune_var.store(true);
      }
      prune_cv.notify_all();
      if (pruning_thread.joinable())
        pruning_thread.join();

      std::lock_guard<std::mutex> lock(wal_mutex);
      if (active_fd >= 0) {
        // Emit Shutdown Seal Payload
        shutdown_payload sp{};
        std::strncpy(sp.Identity, config.Identity.c_str(),
                     sizeof(sp.Identity) - 1);
        auto now = std::chrono::system_clock::now().time_since_epoch();
        sp.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        sp.final_lsn = next_lsn;

        frame_record sd_fr;
        sd_fr.header.header_length = config.RECORD_HEADER_SIZE;
        sd_fr.header.LSN = next_lsn;
        sd_fr.header.payload_length = sizeof(sp);
        sd_fr.header.record_type = Record_type::SHUTDOWN;

        auto *byte_ptr = reinterpret_cast<unsigned char *>(&sp);
        sd_fr.payload.assign(byte_ptr, byte_ptr + sizeof(sp));

        auto v = compute_byte_frame(sd_fr);
        sd_fr.checksum = compute_checksum_crc32c(v);
        append_bytes(v, sd_fr.checksum);

        flush_sync_controller(active_fd, v);
        fsync(active_fd);
        close(active_fd);
        active_fd = -1;

        log_internal(
            "INFO",
            "Shutdown seal successfully written to active WAL segment.");
      }
      return true;
    } catch (const std::exception &e) {
      err.set_code(error_codes::SHUTDOWN_ERROR);
      err.set_message(std::string("Shutdown Exception: ") + e.what());
      log_internal("ERROR", err.get_message().value());
      return false;
    } catch (...) {
      err.set_code(error_codes::SHUTDOWN_ERROR);
      err.set_message("Unknown error occurred during WAL shutdown.");
      log_internal("ERROR", err.get_message().value());
      return false;
    }
  }

  bool sync(error_object &err) {
    err.reset();
    try {
      std::lock_guard<std::mutex> lock(wal_mutex);
      if (active_fd < 0)
        return true;
      if (fsync(active_fd) < 0) {
        err.set_code(error_codes::SYNC_FAILED);
        err.set_message("fsync system call failed on active FD.");
        log_internal("ERROR", err.get_message().value());
        return false;
      }
      durable_lsn = (next_lsn > 0) ? next_lsn - 1 : 0;
      return true;
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(e.what());
      log_internal("ERROR", err.get_message().value());
      return false;
    }
  }

  bool sync_through(error_object &err) { return sync(err); }

  bool checkpoint(error_object &err) {
    err.reset();
    try {
      std::lock_guard<std::mutex> lock(wal_mutex);
      std::string seg_path = config.base_dir + "/seg_" +
                             std::to_string(segment_id) + "_lsn" +
                             std::to_string(current_base_lsn) + ".wal";
      int active_fid =
          open(seg_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
      if (active_fid < 0) {
        err.set_code(error_codes::RUNTIME_ERROR);
        err.set_message("Failed to open file for checkpoint writing.");
        log_internal("ERROR", err.get_message().value());
        return false;
      }

      auto now = std::chrono::system_clock::now().time_since_epoch();
      auto mili = std::chrono::duration_cast<std::chrono::milliseconds>(now);

      checkpoint_payload chp{};
      std::strncpy(chp.Identity, config.Identity.c_str(),
                   sizeof(chp.Identity) - 1);
      chp.segment_id = segment_id;
      chp.checkpoint_lsn = (next_lsn > 0) ? next_lsn - 1 : 0;
      chp.timestamp = mili.count();

      frame_record ch_fr;
      ch_fr.header.header_length = config.RECORD_HEADER_SIZE;
      ch_fr.header.LSN = next_lsn;
      ch_fr.header.payload_length = sizeof(chp);
      ch_fr.header.record_type = Record_type::CHECKPOINT;

      auto *byte_ptr = reinterpret_cast<unsigned char *>(&chp);
      ch_fr.payload.assign(byte_ptr, byte_ptr + sizeof(chp));

      auto v = compute_byte_frame(ch_fr);
      ch_fr.checksum = compute_checksum_crc32c(v);
      append_bytes(v, ch_fr.checksum);

      flush_sync_controller(active_fid, v);
      close(active_fid);

      checkpoint_list.push_back(
          {chp.checkpoint_lsn, chp.timestamp, chp.segment_id});
      log_internal("INFO", "Checkpoint record created at LSN " +
                               std::to_string(chp.checkpoint_lsn));
      return true;
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(e.what());
      log_internal("ERROR", err.get_message().value());
      return false;
    }
  }

  bool recover_and_rebuild(error_object &err) {
    err.reset();
    try {
      std::lock_guard<std::mutex> lock(wal_mutex);
      segment_list.clear();

      std::vector<std::filesystem::path> wal_files;
      for (const auto &entry :
           std::filesystem::directory_iterator(config.base_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wal") {
          wal_files.push_back(entry.path());
        }
      }

      if (wal_files.empty())
        return true;

      std::sort(wal_files.begin(), wal_files.end());

      for (const auto &filePath : wal_files) {
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd < 0)
          continue;

        SegmentInfo sgi;
        sgi.base_file_path = filePath.string();
        sgi.number_of_records = 0;

        frame_record fr;
        bool has_header = false;
        bool is_sealed = false;

        off_t valid_offset = 0;

        while (true) {
          if (!decode_frame_from_fd(fd, fr)) {
            off_t file_size = lseek(fd, 0, SEEK_END);
            if (valid_offset < file_size) {
              std::string msg =
                  "[WAL RECOVERY] Corrupt tail frame detected at offset " +
                  std::to_string(valid_offset) + " in " + filePath.string() +
                  ". Truncating damaged bytes.";
              log_internal("WARN", msg);

              close(fd);
              truncate_file(filePath.string(), valid_offset);
              fd = -1;
            }
            break;
          }

          if (fr.header.record_type == Record_type::SEGMENT_HEADER) {
            if (fr.payload.size() >= sizeof(segment_header_payload)) {
              auto *shp = reinterpret_cast<const segment_header_payload *>(
                  fr.payload.data());
              sgi.segment_id = shp->segment_id;
              sgi.base_lsn = shp->base_lsn;
              has_header = true;
            }
          } else if (fr.header.record_type == Record_type::SEGMENT_SEAL ||
                     fr.header.record_type == Record_type::SHUTDOWN) {
            is_sealed = true;
          }

          sgi.last_lsn = fr.header.LSN;
          sgi.number_of_records++;
          next_lsn = std::max(next_lsn, fr.header.LSN + 1);

          valid_offset = lseek(fd, 0, SEEK_CUR);
        }

        if (fd >= 0)
          close(fd);

        if (has_header) {
          segment_list.push_back(sgi);
          segment_id = std::max(segment_id, sgi.segment_id);

          if (!is_sealed) {
            active_fd =
                open(sgi.base_file_path.c_str(), O_WRONLY | O_APPEND, 0644);
            active_segment_space = sgi.number_of_records;
            current_base_lsn = sgi.base_lsn;
          }
        }
      }

      if (active_fd < 0 && !segment_list.empty()) {
        segment_id++;
        current_base_lsn = next_lsn;
      }

      durable_lsn = (next_lsn > 0) ? next_lsn - 1 : 0;
      log_internal("INFO", "Recovery rebuild completed. next_lsn=" +
                               std::to_string(next_lsn));
      return true;
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(std::string("Recovery Failure: ") + e.what());
      log_internal("ERROR", err.get_message().value());
      return false;
    }
  }

  void register_reader(int id, uint64_t start_lsn) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    registered_readers[id] = start_lsn;
  }

  void unregister_reader(int id) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    registered_readers.erase(id);
  }

  void update_reader_lsn(int id, uint64_t lsn) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    registered_readers[id] = lsn;
  }

  uint64_t get_min_retention_lsn() const {
    uint64_t min_lsn = next_lsn;
    for (auto &[id, lsn] : registered_readers) {
      min_lsn = std::min(min_lsn, lsn);
    }
    return min_lsn;
  }

  bool decode_frame_from_fd(int fd, frame_record &final_fr) {
    uint8_t hdr_buf[28];
    ssize_t r = read(fd, hdr_buf, 28);
    if (r < 28)
      return false;

    std::vector<unsigned char> hdr_vec(hdr_buf, hdr_buf + 28);
    size_t off = 0;
    final_fr.header.LSN = read_bytes<uint64_t>(hdr_vec, off);
    final_fr.header.version_number = read_bytes<uint16_t>(hdr_vec, off);
    final_fr.header.record_type =
        static_cast<Record_type>(read_bytes<uint16_t>(hdr_vec, off));
    final_fr.header.header_length = read_bytes<uint64_t>(hdr_vec, off);
    final_fr.header.payload_length = read_bytes<uint64_t>(hdr_vec, off);

    final_fr.payload.resize(final_fr.header.payload_length);
    r = read(fd, final_fr.payload.data(), final_fr.header.payload_length);
    if (r < static_cast<ssize_t>(final_fr.header.payload_length))
      return false;

    uint8_t crc_buf[4];
    r = read(fd, crc_buf, 4);
    if (r < 4)
      return false;

    std::vector<unsigned char> crc_vec(crc_buf, crc_buf + 4);
    off = 0;
    final_fr.checksum = read_bytes<uint32_t>(crc_vec, off);

    auto byte_frame = compute_byte_frame(final_fr);
    if (compute_checksum_crc32c(byte_frame) != final_fr.checksum) {
      log_internal("ERROR", "CRC checksum mismatch detected on frame LSN " +
                                std::to_string(final_fr.header.LSN));
      return false;
    }
    return true;
  }

  std::optional<frame_record> read_record_at_lsn(uint64_t target_lsn,
                                                 error_object &err) {
    err.reset();
    try {
      std::lock_guard<std::mutex> lock(wal_mutex);
      std::string target_path;

      for (const auto &seg : segment_list) {
        if (seg.base_lsn <= target_lsn && seg.last_lsn >= target_lsn) {
          target_path = seg.base_file_path;
          break;
        }
      }
      std::cout << target_path << std::endl;
      if (target_path.empty())
        return std::nullopt;

      int fd = open(target_path.c_str(), O_RDONLY);
      if (fd < 0)
        return std::nullopt;

      frame_record fr;
      while (decode_frame_from_fd(fd, fr)) {
        if (fr.header.LSN == target_lsn) {
          close(fd);
          return fr;
        }
      }
      close(fd);
      return std::nullopt;
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(e.what());
      log_internal("ERROR", err.get_message().value());
      return std::nullopt;
    }
  }

  bool append(const std::string &str_, error_object &err) {
    err.reset();
    try {
      std::lock_guard<std::mutex> lock(wal_mutex);
      if (is_shutdown.load()) {
        err.set_code(error_codes::SHUTDOWN_ERROR);
        err.set_message("Cannot append record: WAL is already shut down.");
        return false;
      }

      std::vector<unsigned char> py(str_.begin(), str_.end());

      if (active_fd == -1) {
        std::string seg_path = config.base_dir + "/seg_" +
                               std::to_string(segment_id) + "_lsn" +
                               std::to_string(current_base_lsn) + ".wal";
        active_fd = open(seg_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (active_fd < 0) {
          err.set_code(error_codes::RUNTIME_ERROR);
          err.set_message("Failed to open segment file for append.");
          log_internal("ERROR", err.get_message().value());
          return false;
        }

        segment_header_payload sh_pay{};
        std::strncpy(sh_pay.Identity, config.Identity.c_str(),
                     sizeof(sh_pay.Identity) - 1);
        sh_pay.segment_id = segment_id;
        sh_pay.base_lsn = current_base_lsn;

        frame_record sh_fr;
        sh_fr.header.header_length = config.RECORD_HEADER_SIZE;
        sh_fr.header.LSN = next_lsn;
        sh_fr.header.payload_length = sizeof(sh_pay);
        sh_fr.header.record_type = Record_type::SEGMENT_HEADER;

        auto *byte_ptr = reinterpret_cast<unsigned char *>(&sh_pay);
        sh_fr.payload.assign(byte_ptr, byte_ptr + sizeof(sh_pay));

        auto sv = compute_byte_frame(sh_fr);
        sh_fr.checksum = compute_checksum_crc32c(sv);
        append_bytes(sv, sh_fr.checksum);

        flush_sync_controller(active_fd, sv);

        SegmentInfo sgi;
        sgi.base_lsn = next_lsn;
        sgi.segment_id = segment_id;
        sgi.base_file_path = seg_path;
        sgi.last_lsn = next_lsn;
        sgi.number_of_records = 1;
        segment_list.push_back(sgi);

        next_lsn++;
        active_segment_space++;
      }

      frame_record fr;
      fr.header.header_length = config.RECORD_HEADER_SIZE;
      fr.header.LSN = next_lsn;
      fr.header.payload_length = py.size();
      fr.header.record_type = Record_type::ROW;
      fr.payload = py;

      auto v = compute_byte_frame(fr);
      fr.checksum = compute_checksum_crc32c(v);
      append_bytes(v, fr.checksum);

      if (!flush_sync_controller(active_fd, v)) {
        err.set_code(error_codes::RUNTIME_ERROR);
        err.set_message("Failed to write frame payload to file descriptor.");
        log_internal("ERROR", err.get_message().value());
        return false;
      }

      segment_list.back().last_lsn = next_lsn;
      segment_list.back().number_of_records++;

      next_lsn++;
      active_segment_space++;

      if (active_segment_space >= config.MAX_SEG_SIZE) {
        segment_seal ss_pay{segment_id, next_lsn, active_segment_space};
        frame_record ss_fr;
        ss_fr.header.header_length = config.RECORD_HEADER_SIZE;
        ss_fr.header.LSN = next_lsn;
        ss_fr.header.payload_length = sizeof(ss_pay);
        ss_fr.header.record_type = Record_type::SEGMENT_SEAL;

        auto *seal_ptr = reinterpret_cast<unsigned char *>(&ss_pay);
        ss_fr.payload.assign(seal_ptr, seal_ptr + sizeof(ss_pay));

        auto ss_v = compute_byte_frame(ss_fr);
        ss_fr.checksum = compute_checksum_crc32c(ss_v);
        append_bytes(ss_v, ss_fr.checksum);

        flush_sync_controller(active_fd, ss_v);
        segment_list.back().last_lsn = next_lsn;
        segment_list.back().number_of_records++;

        if (config.ss == sync_strategy::ALWAYS) {
          if (fsync(active_fd) < 0) {
            err.set_code(error_codes::SYNC_FAILED);
            err.set_message("fsync failed during segment seal rollover.");
            log_internal("ERROR", err.get_message().value());
            close(active_fd);
            active_fd = -1;
            return false;
          }
          durable_lsn = next_lsn;
        }
        close(active_fd);

        current_base_lsn = next_lsn + 1;
        active_segment_space = 0;
        active_fd = -1;
        segment_id++;
        next_lsn++;
      }

      tail_cv.notify_all();
      return true;
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(e.what());
      log_internal("ERROR", err.get_message().value());
      return false;
    }
  }

  uint64_t get_next_lsn() const {
    std::lock_guard<std::mutex> lock(wal_mutex);
    return next_lsn;
  }

  void wait_for_new_data(uint64_t current_lsn,
                         std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(wal_mutex);
    tail_cv.wait_for(lock, timeout,
                     [this, current_lsn] { return next_lsn > current_lsn; });
  }

private:
  void start_prune_process() {
    while (!prune_var.load()) {
      {
        std::unique_lock<std::mutex> lock(prune_mutex);
        prune_cv.wait_for(lock, std::chrono::seconds(10),
                          [this] { return prune_var.load(); });
      }

      if (prune_var.load())
        break;
      execute_prune_process();
    }
  }

  void execute_prune_process() {
    std::lock_guard<std::mutex> lock(wal_mutex);
    uint64_t min_pin = get_min_retention_lsn();

    auto it = segment_list.begin();
    while (it != segment_list.end()) {
      if (it->last_lsn < min_pin && segment_list.size() > 1) {
        std::error_code ec;
        if (std::filesystem::remove(it->base_file_path, ec)) {
          log_internal("INFO",
                       "Prune worker deleted segment: " + it->base_file_path);
          it = segment_list.erase(it);
          continue;
        }
      }
      it++;
    }
  }
};

class WAL_iterator {
  static inline std::atomic<int> id_counter{1};
  int iterator_id;
  WAL *wal_ref;
  uint64_t current_lsn;

public:
  WAL_iterator(WAL *w, uint64_t start_lsn = 0)
      : iterator_id(id_counter++), wal_ref(w), current_lsn(start_lsn) {
    if (wal_ref) {
      wal_ref->register_reader(iterator_id, current_lsn);
    }
  }

  ~WAL_iterator() {
    if (wal_ref) {
      wal_ref->unregister_reader(iterator_id);
    }
  }

  std::optional<WAL::frame_record>
  next(error_object &err, bool follow_tail = true, uint32_t timeout_ms = 1000) {
    err.reset();
    try {
      while (true) {
        auto record = wal_ref->read_record_at_lsn(current_lsn, err);
        if (err.failed())
          return std::nullopt;

        if (record.has_value()) {
          current_lsn++;
          wal_ref->update_reader_lsn(iterator_id, current_lsn);
          return record;
        }

        if (!follow_tail)
          return std::nullopt;

        wal_ref->wait_for_new_data(current_lsn,
                                   std::chrono::milliseconds(timeout_ms));

        if (wal_ref->get_next_lsn() <= current_lsn) {
          return std::nullopt;
        }
      }
    } catch (const std::exception &e) {
      err.set_code(error_codes::RUNTIME_ERROR);
      err.set_message(e.what());
      return std::nullopt;
    }
  }

  uint64_t get_current_lsn() const { return current_lsn; }
};
