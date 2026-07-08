#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <utility>
#include <vector>
template <typename KeyType, typename Valuetype, typename Comparator>
class SkipList {
public:
  struct node {
    KeyType k;
    Valuetype v;
    node *right;
    node *up;
    node *left;
    int right_skip_gap = 1;
    node *down;
    bool is_head = 0;
    bool is_head_start = 0;
  };

  node *sl;
  int height_skiplist = 0;
  int length_skiplist = 1; // include header nodes as well
  Comparator c;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_real_distribution<double> dist{0.0, 1.0};

  SkipList() : gen(rd()) {
    sl = new node();
    sl->down = nullptr;
    sl->up = nullptr;
    sl->left = nullptr;
    sl->right = nullptr;
    sl->is_head = 1;
    sl->is_head_start = 1;
  }
  // probability generator
  int p_gen() {
    if (dist(gen) < 0.5)
      return 0;
    return 1;
  };
  // height calculator
  int height_calc() {
    int h = 1;
    while (p_gen() == 1) {
      h++;
    }
    return h;
  }
  // delete
  std::pair<std::pair<node *, node *>, std::vector<node *>> search(KeyType k) {
    // return { lower bound prev pair , head_node}
    node *start = sl->right;
    node *head_s = sl;
    node *next;
    std::vector<node *> preds;
    while (start != nullptr) {
      next = start->right;

      // return if found answer
      if (start->k == k)
        return {{start, head_s}, preds};
      // if landed on a key larger than expected at the start we want to go down
      // the heads
      else if (start->k > k) {

        if (head_s->down != nullptr) {
          preds.push_back(head_s);
          head_s = head_s->down;
          start = head_s->right;
        } else {
          return {{head_s, head_s}, preds};
        }
        continue;
      }

      // go down to below layer if reached tail
      if (next == nullptr) {
        if (head_s->down == nullptr || start->down == nullptr)
          break;

        if (start->down) {
          preds.push_back(start);
          start = start->down;
        }
        if (head_s->down)
          head_s = head_s->down;
        continue;
      }
      // found a stopping point if we found next key val is bigger than current
      // value and we go down next layer
      else if (next->k > k) {

        if (start->down == nullptr) {
          return {{start, head_s}, preds};
        } else {
          preds.push_back(start);
          start = start->down;
          head_s = head_s->down;
        }
        continue;
      }

      start = start->right;
    }
    return {{start, head_s}, preds};
  }
  node *search_index(int i) {
    node *start = sl->right;
    int left_move = 1 + i;
    auto nav = sl;
    while (nav->down != nullptr || left_move != 0) {
      // move rightwards
      while (left_move >= nav->right_skip_gap) {
        left_move = left_move - nav->right_skip_gap;
        nav = nav->right;
      }
      nav = nav->down;
    }

    return nav;
  }
  // search (index and value)
  void insert(KeyType k_ins, Valuetype v_ins) {

    // new node addition initial condiiton ,
    if (sl->right == nullptr) {
      auto n = new node();
      n->left = sl;
      n->up = nullptr;
      n->down = nullptr;
      n->right = nullptr;
      sl->right_skip_gap = 1;
      n->k = k_ins;
      n->v = v_ins;
      length_skiplist = 1;
      height_skiplist = 1;

      sl->right = n;
      return;
    }

    auto s = search(k_ins);
    // insertion of the bottom layer linked here , so at the end remove last
    // element in preds if youre trying to keep track
    auto pred_addition_node = s.first.first;
    node *succ_addition_node = nullptr;
    auto new_n = new node();
    new_n->left = pred_addition_node;
    new_n->right = pred_addition_node->right;
    new_n->k = k_ins;
    new_n->v = v_ins;
    new_n->down = nullptr;
    new_n->up = nullptr;
    new_n->right_skip_gap = 1;

    if (pred_addition_node->right != nullptr) {
      succ_addition_node = pred_addition_node->right;
      succ_addition_node->left = new_n;
    }

    pred_addition_node->right_skip_gap = 1;
    pred_addition_node->right = new_n;

    int left_span_length = 0;

    // calculate the left span of the SkipList
    auto temp = pred_addition_node->left;
    while (temp != nullptr) {
      left_span_length += temp->right_skip_gap;
      temp = temp->left;
    }

    length_skiplist += 1;

    node *prev_bottom = nullptr;
    node *prev_header = sl;
    // cacl the height
    auto new_node_height = height_calc();

    if (new_node_height > height_skiplist) {
      int diff = new_node_height - height_skiplist;
      // new highhest level
      auto n_h = new node();
      n_h->left = nullptr;
      n_h->up = nullptr;
      // updated down in while loop below
      n_h->is_head_start = 1;
      n_h->is_head = 1;
      n_h->right_skip_gap = left_span_length;

      auto n = new node();
      n->left = n_h;
      n->up = nullptr;
      n->down = nullptr;
      n->right = nullptr;
      n->right_skip_gap = length_skiplist - left_span_length;
      n->k = k_ins;
      n->v = v_ins;

      n_h->right = n;
      sl = n_h;
      prev_bottom = n;

      // remaing additional levels requied
      node *prev_h = n_h;
      while (diff > 1) {
        auto n_h = new node();
        n_h->left = nullptr;
        n_h->up = prev_h;
        n_h->is_head = 1;
        n_h->right_skip_gap = left_span_length;

        auto n = new node();
        n->left = n_h;
        n->down = nullptr;
        n->up = prev_bottom;
        n->right = nullptr;
        n->right_skip_gap = (length_skiplist - left_span_length);
        n->k = k_ins;
        n->v = v_ins;

        prev_bottom->down = n;
        n_h->right = n;
        prev_h->down = n_h;
        prev_h = n_h;
        prev_bottom = n;

        diff--;
      }
      // attach original header_start
      // ...
      prev_header->up = prev_h;
      prev_h->down = prev_header;
    }
    auto old_height = height_skiplist;
    height_skiplist = std::max(new_node_height, height_skiplist);
    int i;
    if (new_node_height < old_height)
      i = s.second.size() - 1 - (old_height - new_node_height) - 1;
    else
      i = s.second.size() - 1;
    for (; i >= 0; i--) {
      auto pred_addition_node2 = s.second[i];
      node *succ_addition_node2 = nullptr;
      auto new_n2 = new node();
      new_n2->left = pred_addition_node2;
      new_n2->right = pred_addition_node2->right;
      new_n2->k = k_ins;
      new_n2->v = v_ins;
      new_n2->up = prev_bottom;
      new_n2->right_skip_gap = pred_addition_node2->right_skip_gap;
      if (prev_bottom != nullptr)
        prev_bottom->down = new_n;
      if (pred_addition_node2->right != nullptr) {
        succ_addition_node2 = pred_addition_node2->right;
        succ_addition_node2->left = new_n2;
      }

      pred_addition_node2->right_skip_gap = 1;
      pred_addition_node2->right = new_n2;

      prev_bottom = new_n2;
    }
    // attaching the final bottom new_node to the list ...
    if (prev_bottom)
      prev_bottom->down = new_n;
    new_n->up = prev_bottom;
  }
  // insert

  // span for RANK search
  // range search
  void printNode(node *n) {
    if (n == nullptr) {
      std::cout << "nullptr";
      return;
    }

    if (n->is_head) {
      std::cout << "[HEAD]";
      return;
    }

    std::cout << "(" << n->k << "," << n->v << ")";
  }
  void prettyPrint() {
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "                SKIP LIST DUMP\n";
    std::cout << "====================================================\n";

    node *level = sl;
    int levelNo = height_skiplist;

    while (level) {
      std::cout << "\nLEVEL " << levelNo-- << "\n";
      std::cout << "----------------------------------------------------\n";

      node *cur = level;

      while (cur) {
        printNode(cur);

        std::cout << "\n";

        std::cout << "    left      : ";
        printNode(cur->left);

        std::cout << "\n";

        std::cout << "    right     : ";
        printNode(cur->right);

        std::cout << "\n";

        std::cout << "    up        : ";
        printNode(cur->up);

        std::cout << "\n";

        std::cout << "    down      : ";
        printNode(cur->down);

        std::cout << "\n";

        std::cout << "    skip_gap  : " << cur->right_skip_gap << "\n";

        std::cout << "\n";

        cur = cur->right;
      }

      level = level->down;
    }

    std::cout << "====================================================\n";
  }

  // print / return all values
};

template <typename KeyType, typename ValueType, typename Comparator>
class SkipListTester {
public:
  using Skip = SkipList<KeyType, ValueType, Comparator>;
  using Node = typename Skip::node;

  //----------------------------------------------------------
  // Print one node
  //----------------------------------------------------------
  static void printNode(Node *n) {
    if (n == nullptr) {
      std::cout << "nullptr";
      return;
    }

    if (n->is_head) {
      std::cout << "[HEAD]";
      return;
    }

    std::cout << "(" << n->k << "," << n->v << ")";
  }

  //----------------------------------------------------------
  // Pretty Print Entire SkipList
  //----------------------------------------------------------
  static void prettyPrint(Skip &sl) {
    std::cout << "\n";
    std::cout << "====================================================\n";
    std::cout << "                SKIP LIST DUMP\n";
    std::cout << "====================================================\n";

    Node *level = sl.sl;
    int levelNo = sl.height_skiplist;

    while (level) {
      std::cout << "\nLEVEL " << levelNo-- << "\n";
      std::cout << "----------------------------------------------------\n";

      Node *cur = level;

      while (cur) {
        printNode(cur);

        std::cout << "\n";

        std::cout << "    left      : ";
        printNode(cur->left);

        std::cout << "\n";

        std::cout << "    right     : ";
        printNode(cur->right);

        std::cout << "\n";

        std::cout << "    up        : ";
        printNode(cur->up);

        std::cout << "\n";

        std::cout << "    down      : ";
        printNode(cur->down);

        std::cout << "\n";

        std::cout << "    skip_gap  : " << cur->right_skip_gap << "\n";

        std::cout << "\n";

        cur = cur->right;
      }

      level = level->down;
    }

    std::cout << "====================================================\n";
  }

  //----------------------------------------------------------
  // Validate left/right pointers
  //----------------------------------------------------------
  static bool validateHorizontalPointers(Skip &sl) {
    bool ok = true;

    Node *level = sl.sl;

    while (level) {
      Node *cur = level;

      while (cur) {
        if (cur->right) {
          if (cur->right->left != cur) {
            std::cout << "Broken LEFT pointer\n";
            ok = false;
          }
        }

        if (cur->left) {
          if (cur->left->right != cur) {
            std::cout << "Broken RIGHT pointer\n";
            ok = false;
          }
        }

        cur = cur->right;
      }

      level = level->down;
    }

    return ok;
  }

  //----------------------------------------------------------
  // Validate vertical pointers
  //----------------------------------------------------------
  static bool validateVerticalPointers(Skip &sl) {
    bool ok = true;

    Node *level = sl.sl;

    while (level) {
      Node *cur = level;

      while (cur) {
        if (cur->up) {
          if (cur->up->down != cur) {
            std::cout << "Broken UP pointer\n";
            ok = false;
          }
        }

        if (cur->down) {
          if (cur->down->up != cur) {
            std::cout << "Broken DOWN pointer\n";
            ok = false;
          }
        }

        cur = cur->right;
      }

      level = level->down;
    }

    return ok;
  }

  //----------------------------------------------------------
  // Validate ordering
  //----------------------------------------------------------
  static bool validateSorted(Skip &sl) {
    Node *level = sl.sl;

    while (level->down)
      level = level->down;

    Node *cur = level->right;

    if (cur == nullptr)
      return true;

    while (cur->right) {
      if (cur->k > cur->right->k) {
        std::cout << "Ordering violated: " << cur->k << " > " << cur->right->k
                  << "\n";

        return false;
      }

      cur = cur->right;
    }

    return true;
  }

  //----------------------------------------------------------
  // Validate tower consistency
  //----------------------------------------------------------
  static bool validateTowerConsistency(Skip &sl) {
    bool ok = true;

    Node *level = sl.sl;

    while (level) {
      Node *cur = level;

      while (cur) {
        if (cur->down) {
          if (!cur->is_head) {
            if (cur->k != cur->down->k) {
              std::cout << "Tower mismatch at key " << cur->k << "\n";
              ok = false;
            }
          }
        }

        cur = cur->right;
      }

      level = level->down;
    }

    return ok;
  }

  //----------------------------------------------------------
  // Validate skip gaps
  //----------------------------------------------------------
  static bool validateSkipGaps(Skip &sl) {
    bool ok = true;

    Node *level = sl.sl;

    while (level) {
      Node *cur = level;

      while (cur) {
        if (cur->right_skip_gap <= 0) {
          std::cout << "Invalid skip gap detected.\n";
          ok = false;
        }

        cur = cur->right;
      }

      level = level->down;
    }

    return ok;
  }

  //----------------------------------------------------------
  // Count total nodes
  //----------------------------------------------------------
  static int countNodes(Skip &sl) {
    int cnt = 0;

    Node *level = sl.sl;

    while (level) {
      Node *cur = level;

      while (cur) {
        cnt++;
        cur = cur->right;
      }

      level = level->down;
    }

    return cnt;
  }

  //----------------------------------------------------------
  // Print statistics
  //----------------------------------------------------------
  static void statistics(Skip &sl) {
    std::cout << "\n";
    std::cout << "========== Statistics ==========\n";
    std::cout << "Height : " << sl.height_skiplist << "\n";
    std::cout << "Length : " << sl.length_skiplist << "\n";
    std::cout << "Total allocated nodes : " << countNodes(sl) << "\n";
    std::cout << "================================\n";
  }

  //----------------------------------------------------------
  // Run every validation
  //----------------------------------------------------------
  static bool validate(Skip &sl) {
    bool ok = true;

    std::cout << "\nRunning Validation...\n";

    ok &= validateHorizontalPointers(sl);
    ok &= validateVerticalPointers(sl);
    ok &= validateSorted(sl);
    ok &= validateTowerConsistency(sl);
    ok &= validateSkipGaps(sl);

    if (ok)
      std::cout << "\nALL VALIDATION TESTS PASSED.\n";
    else
      std::cout << "\nVALIDATION FAILED.\n";

    return ok;
  }
};

template <typename KeyType, typename ValueType, typename Comparator>
class SkipListTesterExtra {
public:
  using Skip = SkipList<KeyType, ValueType, Comparator>;
  using Node = typename Skip::node;

  //--------------------------------------------------
  // Insert deterministic sample
  //--------------------------------------------------
  static void insertSample(Skip &sl) {
    std::vector<int> keys = {50, 20, 70, 10, 30, 60, 80, 40, 90, 100,
                             55, 65, 75, 35, 45, 15, 5,  95, 85};

    for (int k : keys) {
      sl.insert(k, "Value_" + std::to_string(k));
    }
    sl.prettyPrint();

    std::cout << "Inserted " << keys.size() << " sample keys.\n";
  }

  //--------------------------------------------------
  // Ascending insertion
  //--------------------------------------------------
  static void insertAscending(Skip &sl, int n) {
    for (int i = 0; i < n; i++) {
      sl.insert(i, "Ascending_" + std::to_string(i));
    }

    std::cout << "Ascending insertion completed.\n";
  }

  //--------------------------------------------------
  // Descending insertion
  //--------------------------------------------------
  static void insertDescending(Skip &sl, int n) {
    for (int i = n; i >= 0; i--) {
      sl.insert(i, "Descending_" + std::to_string(i));
    }

    std::cout << "Descending insertion completed.\n";
  }

  //--------------------------------------------------
  // Random insertion
  //--------------------------------------------------
  static std::map<int, std::string> randomInsert(Skip &sl, int n,
                                                 int maxKey = 100000) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, maxKey);

    std::map<int, std::string> truth;

    for (int i = 0; i < n; i++) {
      int k = dist(rng);

      std::string v = "Rand_" + std::to_string(k);

      truth[k] = v;

      sl.insert(k, v);
    }

    std::cout << "Inserted " << n << " random keys.\n";

    return truth;
  }

  //--------------------------------------------------
  // Search verification
  //--------------------------------------------------
  static void verifySearch(Skip &sl, const std::map<int, std::string> &truth) {
    std::cout << "\nVerifying search...\n";

    int good = 0;
    int bad = 0;

    for (auto const &p : truth) {
      auto result = sl.search(p.first);

      Node *n = result.first.first;

      if (n == nullptr) {
        std::cout << "Missing key " << p.first << "\n";
        bad++;
        continue;
      }

      if (n->k != p.first) {
        std::cout << "Wrong node for " << p.first << "\n";
        bad++;
        continue;
      }

      good++;
    }

    std::cout << "\n";
    std::cout << "Search success : " << good << "\n";
    std::cout << "Search failed  : " << bad << "\n";
  }

  //--------------------------------------------------
  // Index search verification
  //--------------------------------------------------
  static void verifyIndexSearch(Skip &sl, int maxIndex) {
    std::cout << "\nIndex Search Test\n";

    for (int i = 0; i < maxIndex; i++) {
      Node *n = sl.search_index(i);

      if (n == nullptr) {
        std::cout << "Index " << i << " -> nullptr\n";
      } else {
        std::cout << "Index " << std::setw(3) << i << " -> " << n->k << "\n";
      }
    }
  }

  //--------------------------------------------------
  // Height statistics
  //--------------------------------------------------
  static void printTowerHeights(Skip &sl) {
    std::cout << "\nTower Heights\n";

    Node *bottom = sl.sl;

    while (bottom->down)
      bottom = bottom->down;

    Node *cur = bottom->right;

    while (cur) {
      int h = 1;

      Node *t = cur;

      while (t->up) {
        h++;
        t = t->up;
      }

      std::cout << std::setw(5) << cur->k << " -> " << h << "\n";

      cur = cur->right;
    }
  }

  //--------------------------------------------------
  // Stress test
  //--------------------------------------------------
  static void stressTest(int n) {
    std::cout << "\n===============================\n";
    std::cout << "Running Stress Test (" << n << " inserts)\n";
    std::cout << "===============================\n";

    Skip sl;

    auto start = std::chrono::high_resolution_clock::now();

    auto truth = randomInsert(sl, n);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Insertion time : " << elapsed.count() << " sec\n";

    SkipListTester<KeyType, ValueType, Comparator>::statistics(sl);

    SkipListTester<KeyType, ValueType, Comparator>::validate(sl);

    verifySearch(sl, truth);
  }

  //--------------------------------------------------
  // Complete demonstration
  //--------------------------------------------------
  static void demo() {
    Skip sl;

    insertSample(sl);

    SkipListTester<KeyType, ValueType, Comparator>::statistics(sl);

    SkipListTester<KeyType, ValueType, Comparator>::validate(sl);

    printTowerHeights(sl);

    verifyIndexSearch(sl, 10);
  }
};

//////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////

int main() {
  using MySkipList = SkipList<int, std::string, std::less<int>>;

  std::cout << "===============================\n"
            << " Skip List Test Program\n"
            << "===============================\n\n";

  // Demo on a small list
  SkipListTesterExtra<int, std::string, std::less<int>>::demo();

  // Larger randomized stress tests
  // SkipListTesterExtra<int, std::string, std::less<int>>::stressTest(100);

  // SkipListTesterExtra<int, std::string, std::less<int>>::stressTest(1000);

  // Uncomment once your implementation is stable
  // SkipListTesterExtra<
  //     int,
  //     std::string,
  //     std::less<int>
  // >::stressTest(10000);

  return 0;
}
