// ┌─────────────────────────────────────────────────────────────┐
// │ SSTable Header                                              │
// │                                                             │
// │  sstable_id                                                 │
// │  file_path[128]                                             │
// │  Identity[128]                                              │
// │  timestamp                                                  │
// ├─────────────────────────────────────────────────────────────┤
// │ Data Block 0                                                │
// │                                                             │
// │  ┌───────────────────────────────────────────────────────┐  │
// │  │ db_row                                                │  │
// │  │ flags | key_size | value_size | key | value           │  │
// │  ├───────────────────────────────────────────────────────┤  │
// │  │ db_row                                                │  │
// │  │ flags | key_size | value_size | key | value           │  │
// │  ├───────────────────────────────────────────────────────┤  │
// │  │ ...                                                   │  │
// │  ├───────────────────────────────────────────────────────┤  │
// │  │ row offsets                                           │  │
// │  ├───────────────────────────────────────────────────────┤  │
// │  │ db_id                                                 │  │
// │  │ num_rows                                              │  │
// │  │ data_bytes                                            │  │
// │  └───────────────────────────────────────────────────────┘  │
// ├─────────────────────────────────────────────────────────────┤
// │ Data Block 1                                                │
// │ ...                                                         │
// ├─────────────────────────────────────────────────────────────┤
// │ ...                                                         │
// ├─────────────────────────────────────────────────────────────┤
// │ Data-block metadata                                         │
// │  block offsets                                              │
// │ *data start                                                 │
// │  metadata information                                       │
// ├─────────────────────────────────────────────────────────────┤
// │ Index Block                                                 │
// │                                                             │
// │  first key of each data block                               │
// │  key offsets                                                │
// │ *index start                                                │
// │  key-offset start                                           │
// │  number of index entries                                    │
// ├─────────────────────────────────────────────────────────────┤
// │ First / Last key metadata                                   │
// ├─────────────────────────────────────────────────────────────┤
// │ Bloom Filter                                                 │
// │ *bloom bits                                                  │
// │  bloom size                                                  │
// │  bloom start                                                 │
// ├─────────────────────────────────────────────────────────────┤
// │ Footer                                                      │
// │                                                             │
// │  data metadata start                                        │
// │  index metadata start                                       │
// │  first/last key metadata                                    │
// │  bloom metadata                                             │
// └─────────────────────────────────────────────────────────────┘

/*
 LSM engine stores a batch/unit of changes as a sstable , and slowly flushs it
 down to disk , we then call a certain key or item out of the disk . Here we
 need to iterate over bunch of sstables , and use bloom filters to help with
 lookup values . Also we need to make a reference counter which helps make sure
 important sstables are not compacted without looking these details , will be
 using tiered compaction strategy here . As for merging of sstables in
 compaction we will be using tournament tree . Finally we need a small look
 aside LRU cache which for now helps in keeping all the hot data avaiable
 without any encryption or compression . Make compactore class as a worker like
 entity as this allows for slatedb inspired feature of distributed compaction
 later on .
 */

#include "LSM_Compactor_coordinator.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

class LAC {

public:
  void get();
  bool set();
};

struct LSMEngine_config {
  char Identity[128];
  char base_dir[128];

  uint64_t SIZE_OF_SSTABLE =
      1000; // count of mmetable items in a single sstable

  uint64_t SIZE_OF_DATABLOCK = 25;
};

enum class error_codes { RUNTIME, NONE, ERROR };

class error_obj {
  error_codes error_code = error_codes::NONE;
  std::optional<std::string> message = std::nullopt;

public:
  bool has_error() {
    if (error_code != error_codes::NONE) {
      return true;
    }

    return false;
  }

  void reset() {
    this->error_code = error_codes::NONE;
    this->message = std::nullopt;
  }

  void set_message(const std::string &msg) { message = msg; }

  void set_error_code(const error_codes ec) { error_code = ec; }
};

class bloom_filter {
  int filter_size = 958;
  int hash_count = 7;

  std::vector<bool> bloom_bits;

  size_t fnv1a_hash(const std::string &key) {
    size_t hash = 14695981039346656037ULL;

    for (char c : key) {
      hash ^= static_cast<size_t>(c);
      hash *= 1099511628211ULL;
    }

    return hash;
  }

  void hash_inserting_key(const std::string &key) {

    auto hash_one = std::hash<std::string>{}(key);
    auto hash_two = fnv1a_hash(key);

    for (int i = 0; i < hash_count; i++) {
      auto final_hash =
          (hash_one + static_cast<size_t>(i) * hash_two) % filter_size;

      bloom_bits[final_hash] = true;
    }
  }

  bool check_hash_key(const std::string &key) {

    auto hash_one = std::hash<std::string>{}(key);
    auto hash_two = fnv1a_hash(key);

    for (int i = 0; i < hash_count; i++) {

      auto final_hash =
          (hash_one + static_cast<size_t>(i) * hash_two) % filter_size;

      if (!bloom_bits[final_hash]) {
        return false;
      }
    }

    return true;
  }

public:
  bloom_filter(int m = 958, int k = 7) : filter_size(m), hash_count(k) {
    bloom_bits.assign(filter_size, false);
  }

  int get_size() { return filter_size; }

  void set_M_arry(std::vector<unsigned char> vec) {

    this->clear();

    int count = 0;

    for (auto i : vec) {

      if (count >= filter_size) {
        break;
      }

      if (i & 1) {
        bloom_bits[count] = 1;
      } else {
        bloom_bits[count] = 0;
      }

      count++;
    }
  }

  void append_key(const std::string &key) { hash_inserting_key(key); }

  bool has_key(const std::string &key) { return check_hash_key(key); }

  void print_bloom() {}

  std::vector<unsigned char> get_bloom_filter() {

    std::vector<unsigned char> result;

    int current_bit = 0;

    while (current_bit < filter_size) {
      result.push_back(static_cast<unsigned char>(bloom_bits[current_bit]));

      current_bit++;
    }

    return result;
  }

  void clear() { bloom_bits.assign(filter_size, false); }
};

struct Run {
  uint64_t run_id;
  static inline std::atomic<int> id_counter{1};
  std::vector<std::string> paths_to_files;
  uint64_t tombstone_number;
  uint64_t total_data_rows;
  uint64_t total_run_size;

  Run() {
    run_id = id_counter++;
    tombstone_number = 0;
    total_run_size = 0;
    total_data_rows = 0;
  }
};

struct Bucket {
  uint64_t bucket_id;
  static inline std::atomic<int> id_counter{1};
  size_t file_size = 0;
  std::vector<Run *> runs_list;
  static inline std::atomic<size_t> max_counter{1};
  size_t threshold_size =
      1024 * 1024; // constrainst number of runs here , this essential for
                   // differenitating buckets , 1MB , 10 MB , 100 MB , 1 GB

  Bucket() {
    bucket_id = id_counter++;
    threshold_size = max_counter * threshold_size;
    max_counter = 10 * max_counter;
  }
};

class LSMEngine {

  LSMEngine(LSMEngine_config &config, Compactor_coordinator *cc_)
      : config(config), cc(cc_) {
    std::cout << "Config loaded" << std::endl;
    std::cout << "Added a bucket into list" << std::endl;
    Bucket *ini_bucket = new Bucket();
    bucket_list.push_back(ini_bucket);
    // Add a single compactor coordinator here for working with various
    // compaction threads or stateless workers
    if (cc->set_active()) {
      std::cout << "Loaded active comapction coordinator" << std::endl;
    }
  };

  ~LSMEngine() {
    // send shutdown cmd to cc_
    // clear any backlog work here by flushing completly to a bucket
    // clear all metadata files
  };

  LSMEngine_config config;

  int active_file_descriptor = -1;

  uint32_t lookup_table[256];

  Compactor_coordinator *cc;

  std::vector<Bucket *> bucket_list;

  bloom_filter current_sstable_bloom_filter;

#pragma pack(push, 1)

  struct db_row {
    uint8_t flags; // 0x1 is for tombstone values
    uint64_t key_size;
    uint64_t value_size;
    char payload[];
  };

  struct sstable_db {
    uint64_t db_id;
    uint64_t num_rows;
    uint64_t data_row_offsets_start;

    // we push db_rows right before this here on disk , and also you need to use
    // the end start pointer .
  };

  struct sstable_ib_head {
    uint64_t num_of_data_blocks;
    uint64_t offsets_start;

    // first push keys directly , then push the offsets of the first keys
    // payload , then push db block offsets , then push a pointer to start of
    // the block allowing for payload navigation
  };

  struct sstable_bf {
    uint64_t bloom_filter_db;
  };

  struct sstable_footer {
    uint64_t bloom_filter_db_start_offset;
    uint64_t index_db_start_offset;
    uint64_t data_block_start_offset;
  };

#pragma pack(pop)

  struct sstable {
    uint64_t sstable_id;

    char sstable_file_path[128];

    char Identity[128];

    uint64_t timestamp;

    std::vector<uint64_t> data_block_offsets;

    // below cannot be written as both have variable runtime length , so
    // compiler will not allow two such values to exist without being placed at
    // the bottom of the list , we can directly deal within disk, while reading
    // we make sure to use temp structures to map them

    // data blocks will contain all data blocks , and each data block will have
    // id , num_row , offset_list , raw payload added when writing to disk index
    // blocks will contain num_of_data blocks and offsets list , finally payload
    // of first keys in disk

    // add first element and last element on disk directly here

    uint64_t first_key_size;
    uint64_t last_key_size;

    uint64_t sstable_num_rows;

    sstable_bf bloom_filter;

    sstable_footer footer_block;
  };

  void table_preprocess() {

    for (int i = 0; i < 256; i++) {

      uint32_t crc = static_cast<uint32_t>(i);

      for (int j = 0; j < 8; j++) {

        if (crc & 1) {
          crc = (crc >> 1) ^ 0x82F63B78;
        } else {
          crc = (crc >> 1);
        }
      }

      lookup_table[i] = crc;
    }
  }

  uint32_t compute_checksum(std::vector<unsigned char> &bytes) {

    uint32_t crc = 0xFFFFFFFF;

    for (auto byte : bytes) {
      crc = (crc >> 8) ^ lookup_table[static_cast<uint8_t>(byte ^ crc)];
    }

    return crc ^ 0xFFFFFFFF;
  };

  template <typename T>
  void append_bytes(std::vector<unsigned char> &bytes, T object) {

    size_t size = sizeof(T);

    for (size_t i = size; i > 0; --i) {

      size_t shift = 8 * (i - 1);

      bytes.push_back(static_cast<unsigned char>((object >> shift) & 0xFF));
    }
  }

  template <typename T>
  bool read_bytes(const std::vector<unsigned char> &bytes, size_t &offset,
                  T &object) {

    if (offset + sizeof(T) > bytes.size()) {
      return false;
    }

    uint64_t value = 0;

    for (size_t i = 0; i < sizeof(T); ++i) {

      value <<= 8;

      value |= static_cast<T>(bytes[offset + i]);
    }

    object = static_cast<T>(value);
    offset += sizeof(T);

    return true;
  }

  struct key_value {
    std::string key;
    std::string value;
    bool tombstone = false;
  };

  sstable read_sstable_from_bytes(std::vector<unsigned char> &bytes) {

    sstable file_sstable{};

    // read all the bytes into existing values , and remaining payloads need to
    // be stored seperataly , could use a tuple based output

    if (bytes.size() < 32) {
      return file_sstable;
    }

    size_t footer_offset = bytes.size() - 32;

    size_t offset = footer_offset;

    uint64_t data_block_metadata_start = 0;
    uint64_t index_metadata_start = 0;
    uint64_t first_key_metadata_start = 0;
    uint64_t bloom_metadata_start = 0;

    if (!read_bytes(bytes, offset, data_block_metadata_start)) {
      return file_sstable;
    }

    if (!read_bytes(bytes, offset, index_metadata_start)) {
      return file_sstable;
    }

    if (!read_bytes(bytes, offset, first_key_metadata_start)) {
      return file_sstable;
    }

    if (!read_bytes(bytes, offset, bloom_metadata_start)) {
      return file_sstable;
    }

    file_sstable.footer_block.bloom_filter_db_start_offset =
        bloom_metadata_start;

    file_sstable.footer_block.index_db_start_offset = index_metadata_start;

    file_sstable.footer_block.data_block_start_offset =
        data_block_metadata_start;

    return file_sstable;
  }

  struct sstable_metadata_info {

    std::string file_address;

    std::string first_string_entry;

    std::string last_string_entry;

    uint64_t num_rows_in_sstable;

    uint64_t timestamp;

    sstable_metadata_info() {
      file_address = "";
      first_string_entry = "";
      last_string_entry = "";
      num_rows_in_sstable = 0;
      timestamp = 0;
    }
  };

  error_obj error_state;

  sstable current_sstable;

  uint64_t current_sstable_id = 1;

  std::vector<sstable_metadata_info> sstable_list;

  void compute_db_bytes(std::vector<unsigned char> &bytes,
                        std::vector<key_value> &records, int data_block_id,
                        int default_flags = 0) {

    int row_number = 0;

    std::vector<uint64_t> row_offsets;

    for (auto &record : records) {

      std::string key = record.key;

      std::string value = record.value;

      uint8_t flags = record.tombstone ? 0x1 : default_flags;

      uint64_t key_size = static_cast<uint64_t>(key.size());

      uint64_t value_size = static_cast<uint64_t>(value.size());

      append_bytes<uint8_t>(bytes, flags);

      append_bytes<uint64_t>(bytes, key_size);

      append_bytes<uint64_t>(bytes, value_size);

      auto key_bytes = reinterpret_cast<const unsigned char *>(key.data());

      bytes.insert(bytes.end(), key_bytes, key_bytes + key_size);

      auto value_bytes = reinterpret_cast<const unsigned char *>(value.data());

      bytes.insert(bytes.end(), value_bytes, value_bytes + value_size);

      row_offsets.push_back(row_number);

      row_number += static_cast<int>(1 + sizeof(uint64_t) + sizeof(uint64_t) +
                                     key_size + value_size);
    }

    /*
     * We store the offsets for every row except the last row.
     * The reader can derive the final row boundary from data_bytes.
     */
    for (size_t i = 0; i + 1 < row_offsets.size(); i++) {

      append_bytes<uint64_t>(bytes, row_offsets[i]);
    }

    append_bytes<uint64_t>(bytes, static_cast<uint64_t>(data_block_id));

    append_bytes<uint64_t>(bytes, static_cast<uint64_t>(row_offsets.size()));

    append_bytes<uint64_t>(bytes, static_cast<uint64_t>(row_number));
  }

  void compute_ib_bytes(std::vector<std::string> first_keys,
                        std::vector<uint64_t> key_offsets,
                        std::vector<unsigned char> &bytes) {

    // append all the keys first , append the offsets , append number_of_rows ,
    // append start_offset
    //
    size_t key_payload_size = 0;

    for (auto &key : first_keys) {

      size_t key_size = key.size();

      auto byte_ptr = reinterpret_cast<const unsigned char *>(key.data());

      bytes.insert(bytes.end(), byte_ptr, byte_ptr + key_size);

      key_payload_size += key_size;
    }

    for (auto offset : key_offsets) {

      append_bytes<uint64_t>(bytes, offset);
    }

    append_bytes<uint64_t>(bytes, key_offsets.size());

    append_bytes<uint64_t>(bytes, key_payload_size);
  }

  bool flush_file(int file_descriptor, std::vector<unsigned char> &bytes) {

    if (file_descriptor < 0) {
      return false;
    }

    size_t bytes_written = 0;

    while (bytes_written < bytes.size()) {

      ssize_t written = write(file_descriptor, bytes.data() + bytes_written,
                              bytes.size() - bytes_written);

      if (written < 0) {

        if (errno == EINTR) {
          continue;
        }

        return false;
      }

      if (written == 0) {
        return false;
      }

      bytes_written += static_cast<size_t>(written);
    }

    return true;
  }

  struct Iterator_info {
    uint64_t id;
    std::string path_to_file;
    uint64_t current_pos;
  };

  sstable_metadata_info current_sstable_metadata;

  std::vector<uint64_t> data_block_offsets;

  std::vector<std::string> first_keys_sstable_list;

  std::mutex readers_lock;
  std::unordered_map<int, Iterator_info> readers;

  int previous_db_id = 0;

  int data_block_id = 0;

  Run *active_run = new Run();

  std::condition_variable compact_cv;
  std::thread compactor_thread;

public:
  bool register_iterator(uint64_t id_, std::string path_to_file_,
                         uint64_t cur_Pos) {
    try {
      Iterator_info Inf;
      Inf.id = id_;
      Inf.path_to_file = path_to_file_;
      Inf.current_pos = cur_Pos;

      // search path to file once for sanity check here
      if (std::filesystem::exists(Inf.path_to_file)) {
        error_state.set_error_code(error_codes::ERROR);
        error_state.set_message("File does not exist ");
        return false;
      }

      {
        std::lock_guard<std::mutex> lock(readers_lock);
        readers[Inf.id] = Inf;
      }
      return true;
    } catch (...) {
      error_state.set_error_code(error_codes::ERROR);
      error_state.set_message("Internal error");
      return false;
    }
  }

  bool un_register_iterator(uint64_t id_) {
    error_state.reset();
    try {
      size_t bytes_erased;
      {
        std::lock_guard<std::mutex> lock(readers_lock);
        bytes_erased = readers.erase(id_);
      }
      if (bytes_erased != 0) {
        error_state.set_error_code(error_codes::ERROR);
        error_state.set_message("List was not erased , so item not found ");
        return false;
      } else
        return true;
    } catch (...) {
      error_state.set_error_code(error_codes::ERROR);
      error_state.set_message("Internal error");
      return false;
    }
  }

  bool create_metadata_for_iteratoring(std::string path_to_file,
                                       uint64_t &data_block_metadata_s,
                                       uint64_t &index_metadata_start,
                                       uint64_t &bloom_block_start,
                                       bloom_filter &bf,
                                       bool calculate_bloom_flag) {
    error_state.reset();
    if (!std::filesystem::exists(path_to_file)) {
      error_state.set_error_code(error_codes::ERROR);
      error_state.set_message("File does not exist");
      return false;
    }

    std::vector<unsigned char> bytes;

    int file_descriptor = open(path_to_file.c_str(), O_RDONLY);

    if (file_descriptor < 0) {
      return false;
    }

    struct stat file_metadata;

    if (fstat(file_descriptor, &file_metadata) == -1) {

      close(file_descriptor);
      return false;
    }

    size_t file_end = static_cast<size_t>(file_metadata.st_size);
    size_t bytes_metadata_sstable_size = 4 * sizeof(uint64_t);
    off_t new_position =
        lseek(file_descriptor, bytes_metadata_sstable_size, SEEK_END);
    if (new_position == 0) {
      int error_code = errno;
      error_state.set_error_code(error_codes::ERROR);
      error_state.set_message("LSEEK issue" + std::to_string(error_code) +
                              std::strerror(error_code));
      close(file_descriptor);
      return false;
    }

    bytes.resize(bytes_metadata_sstable_size);
    ssize_t bytes_reads =
        read(file_descriptor, bytes.data(), bytes_metadata_sstable_size);
    if (bytes_reads != bytes_metadata_sstable_size) {
      close(file_descriptor);
      return false;
    }
    uint64_t data_block_metadata_start = 0;
    uint64_t index_block_start = 0;
    uint64_t first_last_key_metadata_start = 0;
    uint64_t bloom_metadata_start = 0;

    size_t read_offset = 0;

    if (!read_bytes(bytes, read_offset, data_block_metadata_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, index_block_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, first_last_key_metadata_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, bloom_metadata_start)) {
      return false;
    }

    // clear bytes for reuse , ensuring we never import the sstable into the
    // memory
    bytes.clear();
    read_offset = 0;

    // read bloom filter
    size_t bloom_metadata_size =
        bloom_metadata_start - (file_end - bytes_metadata_sstable_size) - 1;
    bytes.resize(bloom_metadata_size);
    off_t np2 = lseek(file_descriptor, bloom_metadata_start, SEEK_SET);
    ssize_t br2 = read(file_descriptor, bytes.data(), bloom_metadata_size);
    if (br2 == 0) {
      return false;
    }
    uint64_t bloom_filter_size = 0;
    uint64_t bllom_filter_start = 0;
    if (!read_bytes(bytes, read_offset, bloom_filter_size)) {
      return false;
    }
    if (!read_bytes(bytes, read_offset, bloom_metadata_start)) {
      return false;
    }

    // reading bloom filter payload here
    bytes.clear();
    read_offset = 0;
    bytes.resize(bloom_filter_size);
    if (calculate_bloom_flag) {
      off_t np3 = lseek(file_descriptor,
                        static_cast<size_t>(bloom_metadata_start), SEEK_SET);

      ssize_t br3 = read(file_descriptor, bytes.data(), bloom_filter_size);
      if (br3 == 0) {
        return false;
      }

      bf.set_M_arry(bytes);
    }
    data_block_metadata_s = data_block_metadata_start;
    index_block_start = index_metadata_start;
    bloom_block_start = bloom_metadata_start;

    close(file_descriptor);

    return true;
  }

  int search_through_index_block(std::string key, std::string path_to_file) {

    uint64_t dbms = 0;
    uint64_t ims = 0;
    uint64_t bbs = 0;
    bloom_filter bf;

    auto res =
        create_metadata_for_iteratoring(path_to_file, dbms, ims, bbs, bf, true);

    if (res == false) {
      return -1;
    }

    if (!bf.has_key(key))
      return -1;

    std::vector<unsigned char> bytes;
    int file_descriptor = open(path_to_file.c_str(), O_RDONLY);

    if (file_descriptor < 0) {
      return -1;
    }

    // decode the index block here for a simple binary search for the data_block
    // index
    size_t size_index_metadata = 3 * sizeof(uint64_t);
    size_t read_offset = 0;
    uint64_t ibs = 0;
    uint64_t ibo = 0;
    uint64_t ibn = 0;

    bytes.resize(size_index_metadata);
    if (!read_bytes(bytes, read_offset, ibs)) {
      close(file_descriptor);
      return -1;
    }
    if (!read_bytes(bytes, read_offset, ibo)) {
      close(file_descriptor);
      return -1;
    }
    if (!read_bytes(bytes, read_offset, ibn)) {
      close(file_descriptor);
      return -1;
    }
    bytes.clear();

    // reading offsets first
    auto ls = lseek(file_descriptor, ibo, SEEK_SET);
    std::vector<uint64_t> offsets_;
    for (int i = 0; i < ibn - 1; i++) {
      uint64_t temp;
      size_t temp_size = sizeof(uint64_t);
      ssize_t br4 = read(file_descriptor, &temp, temp_size);
      if (br4 == 0) {
        close(file_descriptor);
        return -1;
      }
      offsets_.push_back(temp);
    }
    // reading payload values
    std::vector<std::string> values_;
    for (int j = 0; j <= ibn - 2; j++) {
      std::string temp_str;
      size_t temp_str_size = offsets_[j + 1] - offsets_[j];
      ssize_t br_4 = read(file_descriptor, temp_str.data(), temp_str_size);
      values_.push_back(temp_str);
    }
    std::string temp_str;
    ssize_t br_5 =
        read(file_descriptor, temp_str.data(), ims - ibo - offsets_.back());

    auto udx = std::lower_bound(values_.begin(), values_.end(), key);
    int index = udx - values_.begin();
    // close file entirely
    close(file_descriptor);

    if (std::strcmp(values_[index].data(), key.data()) > 0) {
      return -1;
    }
    return index;
  };

  bool read_metadata_data_blocks(
      std::string path_to_file, uint64_t index_block_start,
      uint64_t data_block_metadata_start,
      std::vector<uint64_t> data_block_offsets_from_disk) {
    int file_descriptor = open(path_to_file.c_str(), O_RDONLY);

    if (file_descriptor < 0) {
      close(file_descriptor);
      return -1;
    }

    std::vector<unsigned char> bytes;
    if (index_block_start <= data_block_metadata_start) {
      close(file_descriptor);
      return false;
    }

    size_t data_metadata_size =
        static_cast<size_t>(index_block_start - data_block_metadata_start);

    if (data_metadata_size < 3 * sizeof(uint64_t)) {
      close(file_descriptor);
      return false;
    }

    size_t metadata_field_offset =
        static_cast<size_t>(index_block_start - 3 * sizeof(uint64_t));

    uint64_t metadata_start_check = 0;
    uint64_t number_of_data_blocks = 0;
    uint64_t total_rows = 0;

    size_t temp_offset = 0;

    bytes.resize(metadata_field_offset);

    off_t np5 = lseek(file_descriptor, metadata_field_offset, SEEK_SET);
    size_t metadata_size_temp = 3 * sizeof(uint64_t);
    ssize_t br5 = read(file_descriptor, bytes.data(), metadata_size_temp);
    if (br5 == 0) {
      close(file_descriptor);
      return false;
    }

    if (!read_bytes(bytes, temp_offset, metadata_start_check)) {
      close(file_descriptor);
      return false;
    }

    if (!read_bytes(bytes, temp_offset, number_of_data_blocks)) {
      close(file_descriptor);
      return false;
    }

    if (!read_bytes(bytes, temp_offset, total_rows)) {
      close(file_descriptor);
      return false;
    }

    if (metadata_start_check != data_block_metadata_start) {
      close(file_descriptor);
      return false;
    }

    if (number_of_data_blocks == 0) {
      close(file_descriptor);
      return false;
    }
    temp_offset = 0;
    bytes.clear();

    size_t offsets_offset = static_cast<size_t>(data_block_metadata_start);
    size_t offsets_temp_size = number_of_data_blocks * (sizeof(uint64_t));
    bytes.resize(offsets_temp_size);
    off_t np6 = lseek(file_descriptor, offsets_offset, SEEK_SET);
    ssize_t br6 = read(file_descriptor, bytes.data(), offsets_temp_size);
    if (br6 == 0) {
      close(file_descriptor);
      return false;
    }

    for (uint64_t i = 0; i < number_of_data_blocks; ++i) {

      uint64_t block_offset = 0;

      if (!read_bytes(bytes, offsets_offset, block_offset)) {
        close(file_descriptor);
        return false;
      }

      data_block_offsets_from_disk.push_back(block_offset);
    }
    close(file_descriptor);
    bytes.clear();
    return true;
  }

  std::vector<key_value> read_data_block(std::string path_to_file,
                                         uint64_t data_block_start,
                                         uint64_t data_block_end) {
    std::vector<key_value> kv_pairs;
    std::vector<unsigned char> bytes;
    int file_descriptor = open(path_to_file.c_str(), O_RDONLY);

    if (file_descriptor < 0) {
      close(file_descriptor);
      return kv_pairs;
    }

    uint64_t db_end = static_cast<size_t>(data_block_end);
    size_t db_block_md_temp = 3 * sizeof(uint64_t);
    off_t np6 = lseek(file_descriptor, db_block_md_temp, SEEK_END);
    ssize_t br7 = read(file_descriptor, bytes.data(), db_block_md_temp);
    if (br7 == 0) {
      close(file_descriptor);
    }
    size_t temp_offset = 0;
    uint64_t metadata_db_id;
    uint64_t metadata_db_num_rows = 0;
    uint64_t metadata_db_start_offset = 0;

    if (!read_bytes(bytes, temp_offset, metadata_db_id)) {
      close(file_descriptor);
    }
    if (!read_bytes(bytes, temp_offset, metadata_db_num_rows)) {
      close(file_descriptor);
    }
    if (!read_bytes(bytes, temp_offset, metadata_db_start_offset)) {
      close(file_descriptor);
    }

    bytes.clear();

    size_t dn_block_start = static_cast<size_t>(data_block_start);
    off_t np7 = lseek(file_descriptor, dn_block_start, SEEK_SET);

    for (uint64_t row = 0; row < metadata_db_num_rows; ++row) {

      uint8_t flags = 0;
      uint64_t key_size = 0;
      uint64_t value_size = 0;

      size_t row_cursor = 0;
      size_t db_row_metadata_size = 3 * sizeof(uint64_t);
      bytes.resize(db_row_metadata_size);
      ssize_t br8 = read(file_descriptor, bytes.data(), db_row_metadata_size);

      if (!read_bytes(bytes, row_cursor, flags)) {
      }

      if (!read_bytes(bytes, row_cursor, key_size)) {
      }

      if (!read_bytes(bytes, row_cursor, value_size)) {
      }
      bytes.clear();
      bytes.resize(key_size + value_size);
      size_t db_row_kv = static_cast<size_t>(key_size + value_size);
      ssize_t br9 = read(file_descriptor, bytes.data(), db_row_kv);

      std::string key(reinterpret_cast<const char *>(bytes.data()), key_size);

      row_cursor += key_size;

      std::string value(reinterpret_cast<const char *>(bytes.data() + key_size),
                        value_size);

      row_cursor += value_size;

      key_value record;

      record.key = std::move(key);

      record.value = std::move(value);

      record.tombstone = (flags & 0x1) != 0;

      kv_pairs.push_back(std::move(record));
      bytes.clear();
    }

    return kv_pairs;
  }

  bool append(std::vector<key_value> &records) {

    error_state.reset();

    try {

      if (records.empty()) {

        error_state.set_error_code(error_codes::ERROR);

        error_state.set_message("Cannot append empty data");

        return false;
      }

      /*
       * If active_file_descriptor == -1 create new file now , add to
       * sstable_list
       */

      if (active_file_descriptor == -1) {

        current_sstable_bloom_filter.clear();

        current_sstable_metadata = sstable_metadata_info();

        previous_db_id = data_block_id;

        std::string sstable_file_path =
            static_cast<std::string>(config.base_dir) + "/sstable_" +
            std::to_string(current_sstable_id) + ".sst";

        if (active_run->paths_to_files.size() < 4)
          active_run->paths_to_files.push_back(sstable_file_path);
        else {
          bucket_list.back()->runs_list.push_back(active_run);
          active_run = new Run();
        }

        auto now = std::chrono::system_clock::now().time_since_epoch();

        auto timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        active_file_descriptor = open(sstable_file_path.c_str(),
                                      O_CREAT | O_WRONLY | O_APPEND, 0644);

        if (active_file_descriptor < 0) {

          error_state.set_error_code(error_codes::RUNTIME);

          error_state.set_message("Unable to create SSTable");

          return false;
        }

        // sstable 's metadata
        current_sstable.sstable_id = current_sstable_id;

        std::strncpy(current_sstable.sstable_file_path,
                     sstable_file_path.c_str(),
                     sizeof(current_sstable.sstable_file_path) - 1);

        std::strncpy(current_sstable.Identity, config.Identity,
                     sizeof(current_sstable.Identity) - 1);

        current_sstable.sstable_num_rows = records.size();

        current_sstable.timestamp = timestamp;

        current_sstable.first_key_size = records[0].key.size();

        current_sstable_metadata.first_string_entry = records[0].key;

        first_keys_sstable_list.clear();

        first_keys_sstable_list.push_back(records[0].key);

        // append bytes of the first metadata into file
        std::vector<unsigned char> temporary_bytes;

        append_bytes<uint64_t>(temporary_bytes, current_sstable.sstable_id);

        auto byte_ptr = reinterpret_cast<unsigned char *>(
            &current_sstable.sstable_file_path);

        temporary_bytes.insert(temporary_bytes.end(), byte_ptr, byte_ptr + 128);

        auto byte_ptr_2 =
            reinterpret_cast<unsigned char *>(&current_sstable.Identity);

        temporary_bytes.insert(temporary_bytes.end(), byte_ptr_2,
                               byte_ptr_2 + 128);

        append_bytes<uint64_t>(temporary_bytes, current_sstable.timestamp);

        /*
         * The first data block begins immediately
         * after this header.
         */
        data_block_offsets.clear();

        data_block_offsets.push_back(temporary_bytes.size());

        // compute bytes here for a data block && add keys to bloom_filter
        compute_db_bytes(temporary_bytes, records, data_block_id);

        for (auto &record : records) {
          current_sstable_bloom_filter.append_key(record.key);
        }

        // write to said file
        if (!flush_file(active_file_descriptor, temporary_bytes)) {

          std::cout << "issue with file writing " << std::endl;

          return false;
        }

        data_block_id++;

        current_sstable_id++;

      } else {

        // add new data block to current sstable

        std::vector<unsigned char> temporary_bytes;

        first_keys_sstable_list.push_back(records[0].key);

        struct stat file_metadata;

        if (fstat(active_file_descriptor, &file_metadata) == -1) {

          return false;
        }

        /*
         * Store the absolute file offset of this
         * new data block.
         */
        data_block_offsets.push_back(
            static_cast<uint64_t>(file_metadata.st_size));

        compute_db_bytes(temporary_bytes, records, data_block_id);

        for (auto &record : records) {
          current_sstable_bloom_filter.append_key(record.key);
        }

        if (!flush_file(active_file_descriptor, temporary_bytes)) {

          std::cout << "issue with file writing " << std::endl;

          return false;
        }

        current_sstable.sstable_num_rows += records.size();

        current_sstable.last_key_size = records.back().key.size();

        data_block_id++;

        if (current_sstable.sstable_num_rows >= config.SIZE_OF_SSTABLE) {

          // add db offsets , some pointers , index block , first and last db
          // block first key rows , their sizes then push number of rows , and
          // then push bloom filter and footer block

          current_sstable_metadata.timestamp = current_sstable.timestamp;

          current_sstable_metadata.last_string_entry = records.back().key;

          current_sstable_metadata.num_rows_in_sstable =
              current_sstable.sstable_num_rows;

          current_sstable_metadata.file_address =
              current_sstable.sstable_file_path;

          sstable_list.push_back(current_sstable_metadata);

          temporary_bytes.clear();

          // db_offsets , pointer to db_block start , pointer to offset_start ,
          // number of db_blocks , number of row

          struct stat final_file_metadata;

          if (fstat(active_file_descriptor, &final_file_metadata) == -1) {

            return false;
          }

          /*
           * Store the absolute offset of every data block.
           */
          for (auto block_offset : data_block_offsets) {

            append_bytes<uint64_t>(temporary_bytes, block_offset);
          }

          uint64_t data_block_metadata_start =
              static_cast<uint64_t>(final_file_metadata.st_size);

          uint64_t number_of_data_blocks = data_block_offsets.size();

          append_bytes<uint64_t>(temporary_bytes, data_block_metadata_start);

          append_bytes<uint64_t>(temporary_bytes, number_of_data_blocks);

          append_bytes<uint64_t>(temporary_bytes,
                                 current_sstable.sstable_num_rows);

          // append index data block here , first keys payload , offsets of
          // these first keys for lookup , pointer to start of index block ,
          // pointer to start of offsets

          uint64_t index_block_start = static_cast<uint64_t>(
              final_file_metadata.st_size + temporary_bytes.size());

          uint64_t current_key_offset = 0;

          std::vector<uint64_t> index_key_offsets;

          for (auto &first_key : first_keys_sstable_list) {

            index_key_offsets.push_back(current_key_offset);

            auto key_bytes =
                reinterpret_cast<const unsigned char *>(first_key.data());

            temporary_bytes.insert(temporary_bytes.end(), key_bytes,
                                   key_bytes + first_key.size());

            current_key_offset += first_key.size();
          }

          for (auto key_offset : index_key_offsets) {

            append_bytes<uint64_t>(temporary_bytes, key_offset);
          }

          uint64_t index_key_offsets_start = static_cast<uint64_t>(
              final_file_metadata.st_size + temporary_bytes.size());

          append_bytes<uint64_t>(temporary_bytes, index_block_start);

          append_bytes<uint64_t>(temporary_bytes, index_key_offsets_start);

          append_bytes<uint64_t>(temporary_bytes,
                                 first_keys_sstable_list.size());

          // add the first key and last last key of forst db block and last db
          // block

          uint64_t first_last_key_metadata_start = static_cast<uint64_t>(
              final_file_metadata.st_size + temporary_bytes.size());

          auto first_key_bytes = reinterpret_cast<const unsigned char *>(
              sstable_list.back().first_string_entry.data());

          temporary_bytes.insert(
              temporary_bytes.end(), first_key_bytes,
              first_key_bytes + sstable_list.back().first_string_entry.size());

          auto last_key_bytes = reinterpret_cast<const unsigned char *>(
              sstable_list.back().last_string_entry.data());

          temporary_bytes.insert(
              temporary_bytes.end(), last_key_bytes,
              last_key_bytes + sstable_list.back().last_string_entry.size());

          append_bytes<uint64_t>(temporary_bytes,
                                 current_sstable.first_key_size);

          append_bytes<uint64_t>(temporary_bytes,
                                 current_sstable.last_key_size);

          append_bytes<uint64_t>(temporary_bytes,
                                 first_last_key_metadata_start);

          // add bloom filter and add its size , and apointer to bloom filter
          // start

          uint64_t bloom_filter_start = static_cast<uint64_t>(
              final_file_metadata.st_size + temporary_bytes.size());

          auto bloom_bytes = current_sstable_bloom_filter.get_bloom_filter();

          temporary_bytes.insert(temporary_bytes.end(), bloom_bytes.begin(),
                                 bloom_bytes.end());

          uint64_t bloom_metadata_start = static_cast<uint64_t>(
              final_file_metadata.st_size + temporary_bytes.size());

          append_bytes<uint64_t>(temporary_bytes,
                                 current_sstable_bloom_filter.get_size());

          append_bytes<uint64_t>(temporary_bytes, bloom_filter_start);

          // footer , points to start of metadata of datablocks , idb blocks ,
          // first key  of first db & first key of last db , bloom_filter , and
          // using these we navigate indpt

          append_bytes<uint64_t>(temporary_bytes, data_block_metadata_start);

          append_bytes<uint64_t>(temporary_bytes, index_block_start);

          append_bytes<uint64_t>(temporary_bytes,
                                 first_last_key_metadata_start);

          append_bytes<uint64_t>(temporary_bytes, bloom_metadata_start);

          // write the entire bytes ...

          if (flush_file(active_file_descriptor, temporary_bytes)) {

            if (close(active_file_descriptor) == -1) {

              std::cout << "error " << std::endl;
            }

            active_file_descriptor = -1;

            data_block_offsets.clear();

            first_keys_sstable_list.clear();

            current_sstable = sstable{};

            current_sstable_bloom_filter.clear();

            // call pruner thread here

          } else {

            std::cout << "issue with file writing " << std::endl;
          };
        }
      }

      return true;

    } catch (const std::exception &error) {

      error_state.set_error_code(error_codes::RUNTIME);

      error_state.set_message("Runtime error " + std::string(error.what()));

      return false;
    }
  }

  void Recover_metadata_from_dire();

  /*
   * Read an SSTable and reconstruct all key/value
   * records.
   *
   * Tombstones are returned as:
   *
   * {
   *     key = "...",
   *     value = "...",
   *     tombstone = true
   * }
   *
   * The reader uses the footer to locate the data
   * block metadata and then uses the data block offsets
   * to locate every data block.
   */
  bool read_sstable(const std::string &sstable_path,
                    std::vector<key_value> &records) {

    records.clear();

    int file_descriptor = open(sstable_path.c_str(), O_RDONLY);

    if (file_descriptor < 0) {
      return false;
    }

    struct stat file_metadata;

    if (fstat(file_descriptor, &file_metadata) == -1) {

      close(file_descriptor);
      return false;
    }

    if (file_metadata.st_size < 32) {

      close(file_descriptor);
      return false;
    }

    size_t file_size = static_cast<size_t>(file_metadata.st_size);

    std::vector<unsigned char> bytes(file_size);

    size_t bytes_read = 0;

    while (bytes_read < file_size) {

      ssize_t current_read = read(file_descriptor, bytes.data() + bytes_read,
                                  file_size - bytes_read);

      if (current_read < 0) {

        if (errno == EINTR) {
          continue;
        }

        close(file_descriptor);
        return false;
      }

      if (current_read == 0) {
        break;
      }

      bytes_read += static_cast<size_t>(current_read);
    }

    close(file_descriptor);

    if (bytes_read != file_size) {
      return false;
    }

    /*
     * Footer is always four uint64_t values:
     *
     * data block metadata start
     * index block start
     * first/last key metadata start
     * bloom metadata start
     */

    if (file_size < 4 * sizeof(uint64_t)) {
      return false;
    }

    size_t footer_offset = file_size - 4 * sizeof(uint64_t);

    size_t read_offset = footer_offset;

    uint64_t data_block_metadata_start = 0;
    uint64_t index_block_start = 0;
    uint64_t first_last_key_metadata_start = 0;
    uint64_t bloom_metadata_start = 0;

    if (!read_bytes(bytes, read_offset, data_block_metadata_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, index_block_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, first_last_key_metadata_start)) {
      return false;
    }

    if (!read_bytes(bytes, read_offset, bloom_metadata_start)) {
      return false;
    }

    /*
     * The data-block metadata starts with:
     *
     *     block_offset[0]
     *     block_offset[1]
     *     ...
     *     block_offset[N-1]
     *     metadata_start
     *     number_of_blocks
     *     total_rows
     *
     */

    if (data_block_metadata_start >= file_size) {
      return false;
    }

    size_t metadata_offset = static_cast<size_t>(data_block_metadata_start);

    /*
     * We need the number of data blocks.
     *
     * It is located at:
     *
     * metadata_start
     * + N * sizeof(uint64_t)
     * + sizeof(uint64_t)
     */

    if (metadata_offset + sizeof(uint64_t) > file_size) {
      return false;
    }

    /*
     * Since number_of_blocks itself occurs
     * after the offset array, determine N by
     * walking backwards from the index block.
     *
     * The final metadata fields are:
     *
     *     metadata_start
     *     number_of_blocks
     *     total_rows
     */

    if (index_block_start <= data_block_metadata_start) {
      return false;
    }

    size_t data_metadata_size =
        static_cast<size_t>(index_block_start - data_block_metadata_start);

    if (data_metadata_size < 3 * sizeof(uint64_t)) {
      return false;
    }

    size_t metadata_field_offset =
        static_cast<size_t>(index_block_start - 3 * sizeof(uint64_t));

    uint64_t metadata_start_check = 0;
    uint64_t number_of_data_blocks = 0;
    uint64_t total_rows = 0;

    size_t temp_offset = metadata_field_offset;

    if (!read_bytes(bytes, temp_offset, metadata_start_check)) {
      return false;
    }

    if (!read_bytes(bytes, temp_offset, number_of_data_blocks)) {
      return false;
    }

    if (!read_bytes(bytes, temp_offset, total_rows)) {
      return false;
    }

    if (metadata_start_check != data_block_metadata_start) {
      return false;
    }

    if (number_of_data_blocks == 0) {
      return true;
    }

    /*
     * Read all absolute data block offsets.
     */

    std::vector<uint64_t> data_block_offsets_from_disk;

    size_t offsets_offset = static_cast<size_t>(data_block_metadata_start);

    for (uint64_t i = 0; i < number_of_data_blocks; ++i) {

      uint64_t block_offset = 0;

      if (!read_bytes(bytes, offsets_offset, block_offset)) {
        return false;
      }

      data_block_offsets_from_disk.push_back(block_offset);
    }

    /*
     * The data blocks end exactly where their
     * metadata begins.
     */
    uint64_t data_blocks_end = data_block_metadata_start;

    /*
     * Read every data block.
     */
    for (size_t block_index = 0;
         block_index < data_block_offsets_from_disk.size(); ++block_index) {

      uint64_t block_start = data_block_offsets_from_disk[block_index];

      uint64_t block_end =
          (block_index + 1 < data_block_offsets_from_disk.size())
              ? data_block_offsets_from_disk[block_index + 1]
              : data_blocks_end;

      if (block_start >= block_end || block_end > file_size) {
        return false;
      }

      /*
       * Every data block ends with:
       *
       *     row offsets
       *     db_id
       *     num_rows
       *     data_bytes
       *
       * We first read the final three uint64_t
       * values.
       */

      if (block_end - block_start < 3 * sizeof(uint64_t)) {
        return false;
      }

      size_t block_footer_offset =
          static_cast<size_t>(block_end - 3 * sizeof(uint64_t));

      uint64_t data_block_id = 0;
      uint64_t number_of_rows = 0;
      uint64_t data_bytes = 0;

      size_t block_footer_cursor = block_footer_offset;

      if (!read_bytes(bytes, block_footer_cursor, data_block_id)) {
        return false;
      }

      if (!read_bytes(bytes, block_footer_cursor, number_of_rows)) {
        return false;
      }

      if (!read_bytes(bytes, block_footer_cursor, data_bytes)) {
        return false;
      }

      /*
       * Row payload starts at block_start.
       *
       * data_bytes tells us exactly how much
       * actual db_row data exists.
       */

      uint64_t row_payload_start = block_start;

      uint64_t row_payload_end = row_payload_start + data_bytes;

      if (row_payload_end > block_footer_offset) {
        return false;
      }

      /*
       * Parse the actual db_rows.
       *
       * We don't need the row offset array because
       * every db_row contains its own key/value sizes.
       */

      size_t row_cursor = static_cast<size_t>(row_payload_start);

      for (uint64_t row = 0; row < number_of_rows; ++row) {

        uint8_t flags = 0;
        uint64_t key_size = 0;
        uint64_t value_size = 0;

        if (!read_bytes(bytes, row_cursor, flags)) {
          return false;
        }

        if (!read_bytes(bytes, row_cursor, key_size)) {
          return false;
        }

        if (!read_bytes(bytes, row_cursor, value_size)) {
          return false;
        }

        if (row_cursor + key_size + value_size > row_payload_end) {
          return false;
        }

        std::string key(
            reinterpret_cast<const char *>(bytes.data() + row_cursor),
            key_size);

        row_cursor += key_size;

        std::string value(
            reinterpret_cast<const char *>(bytes.data() + row_cursor),
            value_size);

        row_cursor += value_size;

        key_value record;

        record.key = std::move(key);

        record.value = std::move(value);

        record.tombstone = (flags & 0x1) != 0;

        records.push_back(std::move(record));
      }
    }

    /*
     * Sanity check.
     *
     * This catches corruption or an incorrect
     * number_of_rows value in the SSTable.
     */

    if (records.size() != total_rows) {
      return false;
    }

    return true;
  }

  void delete_key(const std::string &key) {

    // append a tombstone value
    // let compaction takr care of deletion

    key_value tombstone_record;

    tombstone_record.key = key;

    tombstone_record.value = "";

    tombstone_record.tombstone = true;

    /*
     * The tombstone should eventually be appended
     * through the normal memtable -> SSTable path.
     */
  };
};

class LSM_Iterator {

  LSMEngine *lsm_engine;
  static inline std::atomic<int> id_counter{1};
  uint64_t id;
  std::string path_of_file = "";
  uint64_t current_position = 0;
  uint64_t next_position = 0;
  error_obj ec;

public:
  LSM_Iterator(LSMEngine *lsme, std::string path)
      : lsm_engine(lsme), path_of_file(path), id(id_counter++) {
    std::cout << "LSM Iterator loaded " << std::endl;
    if (lsm_engine) {
      lsm_engine->register_iterator(id, path_of_file, 0);
    }
  }
  ~LSM_Iterator() {
    if (lsm_engine && id) {
      lsm_engine->un_register_iterator(id);
    }
  }

  std::optional<LSM_Iterator> get_iterator() {
    ec.reset();
    try {
      if (path_of_file.size() == 0) {
        return std::nullopt;
      }
      return *this;
    } catch (...) {
      return std::nullopt;
    }
  }

  std::optional<bool> search_for_item() {
    return true;
  } //@params: given a <key,value> pair

  void next() {} //@params: none , uses current_pos file iterator
};
