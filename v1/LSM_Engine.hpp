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

#pragma once
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string>
#include <sys/stat.h>

#include <unistd.h>
#include <vector>

class LAC {

public:
  void get();
  bool set();
};

class Compactor {
  Compactor() {

  };

  enum class compact_type { TIERED };

  void start_compactor(); // @params: compact_type
  void merge_sstables();  // @params:  sstable_id_1 , sstable_id_2
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

class LSMEngine {

  LSMEngine(LSMEngine_config &config) : config(config) {
    std::cout << "Config loaded" << std::endl;
  };

  ~LSMEngine() {

  };

  LSMEngine_config config;

  int active_file_descriptor = -1;

  uint32_t lookup_table[256];

  std::vector<Compactor> compactor_list;

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

#include <type_traits>

  template <typename T>
  void append_bytes(std::vector<unsigned char> &bytes, T object) {

    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>,
                  "append_bytes only supports unsigned integer types");

    uint64_t value = static_cast<uint64_t>(object);

    for (size_t i = sizeof(T); i > 0; --i) {

      size_t shift = 8 * (i - 1);

      bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xFF));
    }
  }

  template <typename T>
  bool read_bytes(const std::vector<unsigned char> &bytes, size_t &offset,
                  T &object) {

    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>,
                  "read_bytes only supports unsigned integer types");

    if (offset + sizeof(T) > bytes.size()) {
      return false;
    }

    uint64_t value = 0;

    for (size_t i = 0; i < sizeof(T); ++i) {

      value <<= 8;

      value |= static_cast<uint64_t>(bytes[offset + i]);
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

  bool set_compactor(Compactor &compactor) {

    try {

      compactor_list.push_back(std::move(compactor));

      return true;

    } catch (...) {

      return false;
    }
  }

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

  sstable_metadata_info current_sstable_metadata;

  std::vector<uint64_t> data_block_offsets;

  std::vector<std::string> first_keys_sstable_list;

  int previous_db_id = 0;

  int data_block_id = 0;

public:
  sstable get_key_value();

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
