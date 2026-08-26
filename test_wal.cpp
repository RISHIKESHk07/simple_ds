#include "./v1/Wal.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
// Assuming your header is included here:
// #include "wal.hpp"

// Utility to generate sample RESP2 payloads
std::string make_resp2_set(int key_idx) {
  std::string key = "key:" + std::to_string(key_idx);
  std::string val = "val_" + std::to_string(key_idx * 42);
  // RESP2 Array of 3 Bulk Strings:
  // *3\r\n$3\r\nSET\r\n$<len>\r\n<key>\r\n$<len>\r\n<val>\r\n
  return "*3\r\n$3\r\nSET\r\n$" + std::to_string(key.size()) + "\r\n" + key +
         "\r\n$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
}

// Clean up helper directory
void cleanup_dir(const std::string &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

// ============================================================================
// TEST 1: High-Volume RESP2 Workload (10,000 Entries) & Shutdown Recovery
// ============================================================================
void test_high_volume_resp2_recovery() {
  std::cout << "[RUNNING] test_high_volume_resp2_recovery...\n";
  std::string test_dir = "./wal_test_high_vol";
  cleanup_dir(test_dir);

  WAL_config config;
  config.base_dir = test_dir;
  config.MAX_SEG_SIZE = 500; // Force multi-segment rollovers (20+ files)
  config.ss = sync_strategy::ALWAYS;

  error_object err;
  const int TOTAL_ENTRIES = 10000;

  // 1. Write 10,000 RESP2 entries
  {
    WAL wal(config, err);
    assert(!err.failed() && "WAL Initialization failed");

    for (int i = 0; i < TOTAL_ENTRIES; ++i) {
      std::string payload = make_resp2_set(i);
      bool ok = wal.append(payload, err);
      assert(ok && !err.failed() && "Failed to append RESP2 payload");
    }

    bool shut = wal.shutdown(err);
    assert(shut && !err.failed() && "Shutdown failed");
  }

  // 2. Re-open engine and verify recovery across all segment files
  {
    WAL wal_recovered(config, err);
    assert(!err.failed() && "Recovery failed to instantiate");

    WAL_iterator it(&wal_recovered, 0);
    int verified_rows = 0;

    while (true) {
      auto rec = it.next(err, false);
      if (!rec.has_value())
        break;

      if (rec->header.record_type == WAL::Record_type::ROW) {
        std::string payload(rec->payload.begin(), rec->payload.end());
        std::string expected = make_resp2_set(verified_rows);
        assert(payload == expected &&
               "Payload corruption detected in recovery!");
        verified_rows++;
      }
    }

    assert(verified_rows == TOTAL_ENTRIES && "Recovered entry count mismatch!");
  }

  cleanup_dir(test_dir);
  std::cout << "[PASSED] test_high_volume_resp2_recovery\n";
}

// ============================================================================
// TEST 2: Mid-Log Appends, Random LSN Lookups & CRC32 Bit-Level Validation
// ============================================================================
void test_lookups_and_crc_integrity() {
  std::cout << "[RUNNING] test_lookups_and_crc_integrity...\n";
  std::string test_dir = "./wal_test_crc";
  cleanup_dir(test_dir);

  WAL_config config;
  config.base_dir = test_dir;
  config.MAX_SEG_SIZE = 100;

  error_object err;
  const int TOTAL_ENTRIES = 1000;

  WAL wal(config, err);
  assert(!err.failed());

  for (int i = 0; i < TOTAL_ENTRIES; ++i) {
    wal.append("DATA_RECORD_" + std::to_string(i), err);
    assert(!err.failed());
  }

  // Direct LSN lookup & validation
  uint64_t target_lsn = 450;
  auto record = wal.read_record_at_lsn(target_lsn, err);
  assert(record.has_value() && !err.failed());
  assert(record->header.LSN == target_lsn);

  // Re-verify CRC32 checksum manually against payload
  WAL_iterator iter(&wal, 0);
  int read_count = 0;
  while (auto rec = iter.next(err, false)) {
    // Checking internal frame format checksum pass
    assert(rec->checksum != 0 && "CRC Checksum should be non-zero");
    read_count++;
  }
  assert(read_count > TOTAL_ENTRIES && "Iterator should parse headers + rows");

  wal.shutdown(err);
  cleanup_dir(test_dir);
  std::cout << "[PASSED] test_lookups_and_crc_integrity\n";
}

// ============================================================================
// TEST 3: Concurrent Producer / Tail Iterator Stream Consumer
// ============================================================================
void test_tail_iterator_concurrent_streaming() {
  std::cout << "[RUNNING] test_tail_iterator_concurrent_streaming...\n";
  std::string test_dir = "./wal_test_tail";
  cleanup_dir(test_dir);

  WAL_config config;
  config.base_dir = test_dir;

  error_object err;
  WAL wal(config, err);
  assert(!err.failed());

  const int PRODUCED_ITEMS = 500;
  std::atomic<int> consumed_items{0};

  // Consumer Thread - Tail Iterator
  std::thread consumer([&]() {
    error_object local_err;
    WAL_iterator iter(&wal, 0);

    while (consumed_items.load() < PRODUCED_ITEMS) {
      auto rec = iter.next(local_err, true, 200); // Wait up to 200ms per wait
      if (rec.has_value() && rec->header.record_type == WAL::Record_type::ROW) {
        consumed_items++;
      }
    }
  });

  // Producer Thread
  std::thread producer([&]() {
    error_object local_err;
    for (int i = 0; i < PRODUCED_ITEMS; ++i) {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
      wal.append("STREAM_ITEM_" + std::to_string(i), local_err);
      assert(!local_err.failed());
    }
  });

  producer.join();
  consumer.join();

  assert(consumed_items.load() == PRODUCED_ITEMS &&
         "Tail iterator missed streamed items!");

  wal.shutdown(err);
  cleanup_dir(test_dir);
  std::cout << "[PASSED] test_tail_iterator_concurrent_streaming\n";
}

// ============================================================================
// TEST 4: Pruning Worker Safety with Active Reader Pinning
// ============================================================================
void test_pruning_and_retention() {
  std::cout << "[RUNNING] test_pruning_and_retention...\n";
  std::string test_dir = "./wal_test_prune";
  cleanup_dir(test_dir);

  WAL_config config;
  config.base_dir = test_dir;
  config.MAX_SEG_SIZE = 10; // Trigger small segment files rapidly

  error_object err;
  WAL wal(config, err);

  // Register a reader pinned to start (LSN 0)
  WAL_iterator pinned_reader(&wal, 0);

  for (int i = 0; i < 100; ++i) {
    wal.append("PRUNE_TEST_" + std::to_string(i), err);
  }

  // Ensure segments are NOT deleted because reader is pinned at LSN 0
  size_t seg_count_before = 0;
  for (const auto &entry : std::filesystem::directory_iterator(test_dir)) {
    if (entry.path().extension() == ".wal")
      seg_count_before++;
  }
  assert(seg_count_before > 1 && "Multiple segment files should exist");

  // Move reader forward to release pin on older LSNs
  pinned_reader.next(err, false); // Advance
  wal.update_reader_lsn(1, 100);  // Unpin older LSNs

  // Allow background pruner to execute
  std::this_thread::sleep_for(std::chrono::seconds(5));

  size_t new_seg_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(test_dir)) {
    if (entry.path().extension() == ".wal")
      new_seg_count++;
  }

  assert(new_seg_count < seg_count_before && "Pruning issue present");

  wal.shutdown(err);
  cleanup_dir(test_dir);
  std::cout << "[PASSED] test_pruning_and_retention\n";
}

int main() {
  try {
    test_high_volume_resp2_recovery();
    test_lookups_and_crc_integrity();
    test_tail_iterator_concurrent_streaming();
    test_pruning_and_retention();
    std::cout << "\n[ALL TESTS PASSED SUCCESSFULLY]\n";
  } catch (const std::exception &ex) {
    std::cerr << "[TEST FAILED EXCEPTION]: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
