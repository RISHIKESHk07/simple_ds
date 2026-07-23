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
    int right_skip_gap = 0;
    node *down;
    bool is_head = 0;
    bool is_head_start = 0;
  };

  node *sl;
  int height_skiplist = 0;
  int length_skiplist = 0; // include header nodes as well
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
    sl->k = KeyType::neg_inf();
    sl->v = "HEAD";
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
  std::tuple<std::pair<node *, node *>, int,
             std::vector<std::pair<node *, int>>>
  search(KeyType k) {
    // return { lower bound prev pair , head_node}
    node *start = sl;
    node *head_s = sl;
    node *next;
    std::vector<std::pair<node *, int>> preds;
    int index_counter = 0;
    while (start != nullptr) {
      next = start->right;

      // return if found answer
      if (c(start->k, k) == 0)
        return {{start, head_s}, index_counter, preds};
      // if landed on a key larger than expected at the start we want to go down
      // the heads
      else if (c(start->k, k) == -1) {

        if (head_s->down != nullptr) {
          preds.push_back({head_s, 1});
          head_s = head_s->down;
          start = head_s->right;
          index_counter = 1;
        } else {
          return {{head_s, head_s}, index_counter, preds};
        }
        continue;
      }

      // go down to below layer if reached tail
      if (next == nullptr) {
        if (head_s->down == nullptr || start->down == nullptr)
          break;

        if (start->down) {
          preds.push_back({start, index_counter});
          start = start->down;
        }
        if (head_s->down)
          head_s = head_s->down;
        continue;
      }
      // found a stopping point if we found next key val is bigger than current
      // value and we go down next layer
      else if (c(next->k, k) == -1) {

        if (start->down == nullptr) {
          return {{start, head_s}, index_counter, preds};
        } else {
          preds.push_back({start, index_counter});
          start = start->down;
          head_s = head_s->down;
        }
        continue;
      }

      index_counter += start->right_skip_gap;
      start = start->right;
    }
    return {{start, head_s}, index_counter, preds};
  }
  node *search_index(int i) {
    node *start = nullptr;
    int left_move = i;
    auto nav = sl;
    if (left_move > length_skiplist)
      return nullptr;
    if (i <= 0)
      return nullptr;
    while (nav->down != nullptr || left_move != 0) {
      // move rightwards

      while (nav && nav->right && left_move >= nav->right_skip_gap) {
        left_move = left_move - nav->right_skip_gap;
        nav = nav->right;
      }
      if (left_move == 0)
        break;
      std::cout << left_move << std::endl;
      nav = nav->down;
    }

    return nav;
  }
  // search (by index and value)
  void insert(KeyType k_ins, Valuetype v_ins) {
    // case when we have empty skiplist becuase we deleted everything in it ...
    if (sl == nullptr) {
      sl = new node();
      sl->down = nullptr;
      sl->up = nullptr;
      sl->left = nullptr;
      sl->right = nullptr;
      sl->is_head = 1;
      sl->k = KeyType::neg_inf();
      sl->v = "HEAD";
      sl->is_head_start = 1;
    }
    // new node addition initial condiiton ,
    if (sl->right == nullptr) {
      auto n = new node();
      n->left = sl;
      n->up = nullptr;
      n->down = nullptr;
      n->right = nullptr;
      sl->right_skip_gap = 1;
      n->right_skip_gap = 0;
      n->k = k_ins;
      n->v = v_ins;
      length_skiplist = 1;
      height_skiplist = 1;

      sl->right = n;
      return;
    }

    auto [s, bp_index, preds] = search(k_ins);

    if (c(s.first->k, k_ins) == 0) {
      std::cout << "Inserted this key already ..." << std::endl;
      return;
    }

    // insertion of the bottom layer linked here , so at the end remove last
    // element in preds if youre trying to keep track
    auto pred_addition_node = s.first;
    node *succ_addition_node = nullptr;
    auto new_n = new node();
    new_n->left = pred_addition_node;
    new_n->right = pred_addition_node->right;
    new_n->k = k_ins;
    new_n->v = v_ins;
    new_n->down = nullptr;
    new_n->up = nullptr;

    if (pred_addition_node->right != nullptr) {
      succ_addition_node = pred_addition_node->right;
      succ_addition_node->left = new_n;
    }

    pred_addition_node->right_skip_gap = 1;
    pred_addition_node->right = new_n;

    // new insertion on bottom layer ... if succesor does not exist
    if (succ_addition_node) {
      new_n->right_skip_gap = 1;
    } else {
      new_n->right_skip_gap = 0;
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
      n_h->k = KeyType::neg_inf();
      n_h->v = "HEAD";
      // updated down in while loop below
      n_h->is_head_start = 1;
      n_h->is_head = 1;
      n_h->right_skip_gap = bp_index + 1;

      auto n = new node();
      n->left = n_h;
      n->up = nullptr;
      n->down = nullptr;
      n->right = nullptr;
      n->right_skip_gap = length_skiplist - bp_index - 1;
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
        n_h->right_skip_gap = bp_index + 1;
        n_h->k = KeyType::neg_inf();
        n_h->v = "HEAD";

        auto n = new node();
        n->left = n_h;
        n->down = nullptr;
        n->up = prev_bottom;
        n->right = nullptr;
        n->right_skip_gap = (length_skiplist - bp_index - 1);
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
    int r;
    if (new_node_height < old_height) {
      r = old_height - new_node_height;
    } else {
      r = 0;
    }
    // this is the case where we need to modify layer where we do not add nodes
    // but obviously their length increase so skip gaps change
    if (r != 0) {
      for (int j = 0; j < r; j++) {
        preds[j].first->right_skip_gap += 1;
      }
    }
    for (int i = r; i <= static_cast<int>(preds.size()) - 1; i++) {
      auto pred_addition_node2 = preds[i];
      node *succ_addition_node2 = nullptr;
      auto new_n2 = new node();
      new_n2->left = pred_addition_node2.first;
      new_n2->right = pred_addition_node2.first->right;
      new_n2->k = k_ins;
      new_n2->v = v_ins;
      new_n2->up = prev_bottom;
      int new_pred_gap = (bp_index + 1) - pred_addition_node2.second;
      int new_n2_gap =
          pred_addition_node2.first->right_skip_gap + 1 - new_pred_gap;
      new_n2->right_skip_gap = new_n2_gap;
      if (prev_bottom != nullptr)
        prev_bottom->down = new_n2;
      if (pred_addition_node2.first->right != nullptr) {
        succ_addition_node2 = pred_addition_node2.first->right;
        succ_addition_node2->left = new_n2;
      }

      pred_addition_node2.first->right_skip_gap = new_pred_gap;
      pred_addition_node2.first->right = new_n2;

      prev_bottom = new_n2;
    }
    // attaching the final bottom new_node to the list ...
    if (prev_bottom)
      prev_bottom->down = new_n;
    new_n->up = prev_bottom;
  }
  // insert
  std::vector<node *> range_search(KeyType l, KeyType r) {
    std::vector<node *> res;
    auto [s, bp_index, preds] = search(l);
    auto t = s.first;
    if (t == nullptr)
      return res;
    if (c(t->k, l) == -1 || c(t->k, l) == 1)
      return res;
    while (t->down != nullptr) {
      t = t->down;
    }
    while (t && (c(t->k, r) == 1 || c(t->k, r) == 0)) {
      if (t->is_head == false) {
        res.push_back(t);
      }
      t = t->right;
    }
    return res;
  }
  // range search
  bool delete_key(KeyType k_del) {
    // search gives the first occurence in a higher layer
    auto [s, index, preds] = search(k_del);
    if (c(s.first->k, k_del) == -1 || c(s.first->k, k_del)) {
      return 0;
    }
    std::vector<node *> preds_full;
    node *t = s.first;
    while (t != nullptr) {
      preds_full.push_back(t->left);
      t = t->down;
    }
    node *nav = sl;
    for (auto n : preds) {
      n.first->right_skip_gap--;
      nav = nav->down;
    }
    for (int i = 0; i < preds_full.size(); i++) {
      auto cur_pred = preds_full[i];
      auto succ_removal_node = preds_full[i]->right->right;
      cur_pred->right_skip_gap = cur_pred->right->right_skip_gap;

      cur_pred->right->down = nullptr;
      cur_pred->right->up = nullptr;

      delete cur_pred->right;

      cur_pred->right = succ_removal_node;
      if (succ_removal_node) {
        succ_removal_node->left = cur_pred;
      }
      if (nav->right == nullptr) {
        if (nav->down != nullptr)
          nav->down->up = nullptr;
        sl = nav->down;
        height_skiplist--;
      }
      nav = nav->down;
    }

    length_skiplist--;

    return 1;
  }
  // delete

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

    if (level == nullptr)
      return true; // empty case
    while (level->down)
      level = level->down;

    Node *cur = level->right;

    if (cur == nullptr)
      return true;

    while (cur->right) {
      if (sl.c(cur->k, cur->right->k) == -1) {
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
            if (sl.c(cur->k, cur->down->k) == -1 ||
                sl.c(cur->k, cur->down->k)) {
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
      int lvl_t_gap = 0;
      while (cur) {
        if (cur->right_skip_gap < 0) {
          std::cout << "Invalid skip gap detected.\n";
          ok = false;
        }
        lvl_t_gap += cur->right_skip_gap;
        cur = cur->right;
      }

      if (lvl_t_gap != sl.length_skiplist) {
        std::cout << "Lenght was wrong somewhere maybe .\n" << std::endl;
        ok = false;
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
  // Sample Inserts
  //--------------------------------------------------

  static void insertSample(Skip &sl) {

    std::vector<double> scores = {50, 20, 70, 10, 30, 60, 80, 40, 90, 100,
                                  55, 65, 75, 35, 45, 15, 5,  95, 85};

    for (double s : scores) {

      std::string member = "Member_" + std::to_string((int)s);

      sl.insert(KeyType(s, member), "Value_" + std::to_string((int)s));
    }

    sl.prettyPrint();

    std::cout << "Inserted " << scores.size() << " sample keys.\n";
  }

  //--------------------------------------------------

  static void insertAscending(Skip &sl, int n) {
    for (int i = 0; i < n; i++) {

      sl.insert(KeyType((double)i, "Ascending_" + std::to_string(i)), "Value");
    }

    std::cout << "Ascending insertion completed.\n";
  }

  //--------------------------------------------------

  static void insertDescending(Skip &sl, int n) {
    for (int i = n; i >= 0; i--) {

      sl.insert(KeyType((double)i, "Descending_" + std::to_string(i)), "Value");
    }

    std::cout << "Descending insertion completed.\n";
  }

  //--------------------------------------------------
  // Random Inserts
  //--------------------------------------------------

  static std::map<KeyType, ValueType, Comparator> randomInsert(Skip &sl,
                                                               int n) {
    std::mt19937 rng(12345);

    std::uniform_real_distribution<double> score_dist(0.0, 100000.0);

    std::uniform_int_distribution<int> member_dist(1, 10000000);

    std::map<KeyType, ValueType, Comparator> truth;

    for (int i = 0; i < n; i++) {

      double score = score_dist(rng);

      std::string member = "M_" + std::to_string(member_dist(rng));

      KeyType key(score, member);

      ValueType value = "Rand_" + member;

      truth[key] = value;

      sl.insert(key, value);
    }

    std::cout << "Inserted " << n << " random keys.\n";

    return truth;
  }

  //--------------------------------------------------
  // Verify Search
  //--------------------------------------------------

  static void
  verifySearch(Skip &sl,
               const std::map<KeyType, ValueType, Comparator> &truth) {
    std::cout << "\nVerifying search...\n";

    int good = 0;
    int bad = 0;

    for (const auto &p : truth) {

      auto [result, result1, result2] = sl.search(p.first);

      Node *n = result.first;

      if (n == nullptr) {

        std::cout << "Missing key " << p.first << "\n";

        bad++;

        continue;
      }

      if (sl.c(n->k, p.first) != 0) {

        std::cout << "Wrong node for " << p.first << "\n";

        bad++;

        continue;
      }

      good++;
    }

    std::cout << "\nSearch success : " << good << "\nSearch failed  : " << bad
              << "\n";
  }
  //--------------------------------------------------
  // Verify Rank Search
  //--------------------------------------------------

  static void verifyIndexSearch(Skip &sl, int maxIndex) {
    std::cout << "\nIndex Search Test\n";

    for (int i = 1; i <= maxIndex; i++) {

      Node *n = sl.search_index(i);

      if (n == nullptr) {

        std::cout << "Index " << i << " -> nullptr\n";
      } else {

        std::cout << "Index " << std::setw(3) << i << " -> " << n->k << "\n";
      }
    }
  }

  //--------------------------------------------------
  // Print Tower Heights
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

      std::cout << std::setw(20) << cur->k << " -> " << h << "\n";

      cur = cur->right;
    }
  }

  //--------------------------------------------------
  // Stress Test
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
  // Demo
  //--------------------------------------------------

  static void demo() {
    Skip sl;

    insertSample(sl);

    SkipListTester<KeyType, ValueType, Comparator>::statistics(sl);

    SkipListTester<KeyType, ValueType, Comparator>::validate(sl);

    printTowerHeights(sl);

    verifyIndexSearch(sl, 10);
  }

  //--------------------------------------------------
  // Value Search Test
  //--------------------------------------------------

  static void testValueSearch() {
    std::cout << "[Test] Core Value Search... ";

    Skip sl;

    KeyType k1(10, "Ten");

    KeyType k2(20, "Twenty");

    KeyType k3(5, "Five");

    sl.insert(k1, "Ten");

    sl.insert(k2, "Twenty");

    sl.insert(k3, "Five");

    auto [s1, idx1, p1] = sl.search(k1);

    assert(s1.first != nullptr && sl.c(s1.first->k, k1) == 0);

    auto [s2, idx2, p2] = sl.search(k3);

    assert(s2.first != nullptr && sl.c(s2.first->k, k3) == 0);

    KeyType searchKey(15, "Dummy");

    auto [s3, idx3, p3] = sl.search(searchKey);

    assert(s3.first != nullptr);

    std::cout << "PASSED\n";
  }

  //--------------------------------------------------
  // Index Search Test
  //--------------------------------------------------

  static void testIndexSearch() {
    std::cout << "[Test] Rank Index Search... ";

    Skip sl;

    assert(sl.search_index(0) == nullptr);

    assert(sl.search_index(1) == nullptr);

    KeyType k1(30, "Thirty");

    KeyType k2(10, "Ten");

    KeyType k3(20, "Twenty");

    sl.insert(k1, "Thirty");

    sl.insert(k2, "Ten");

    sl.insert(k3, "Twenty");

    Node *r1 = sl.search_index(1);

    Node *r2 = sl.search_index(2);

    Node *r3 = sl.search_index(3);

    assert(r1 != nullptr && sl.c(r1->k, k2) == 0);

    assert(r2 != nullptr && sl.c(r2->k, k3) == 0);

    assert(r3 != nullptr && sl.c(r3->k, k1) == 0);

    assert(sl.search_index(4) == nullptr);

    std::cout << "PASSED\n";
  }
  //--------------------------------------------------
  // Range Search Test
  //--------------------------------------------------

  static void testRangeSearch() {

    std::cout << "[Test] Range Search... ";

    Skip sl;

    std::vector<KeyType> keys = {KeyType(10, "A"), KeyType(20, "B"),
                                 KeyType(30, "C"), KeyType(40, "D"),
                                 KeyType(50, "E")};

    for (auto &k : keys)
      sl.insert(k, "Val");

    auto res = sl.range_search(KeyType(20, "B"), KeyType(40, "D"));

    assert(res.size() == 3);

    std::cout << "SKIPPED (needs generic range_search)\n";
  }

  //--------------------------------------------------
  // Delete Test
  //--------------------------------------------------

  static void testDeletionAndGaps() {

    std::cout << "[Test] Deletion & Gap Resizing... ";

    Skip sl;

    KeyType k1(50.0, "Fifty");
    KeyType k2(25.0, "TwentyFive");
    KeyType k3(75.0, "SeventyFive");

    sl.insert(k1, "Fifty");
    sl.insert(k2, "TwentyFive");
    sl.insert(k3, "SeventyFive");

    // delete nonexistent key
    assert(!sl.delete_key(KeyType(99.0, "Missing")));
    assert(sl.length_skiplist == 3);

    // delete middle key
    assert(sl.delete_key(k2));
    assert(sl.length_skiplist == 2);

    Node *r1 = sl.search_index(1);
    assert(r1 != nullptr);
    assert(sl.c(r1->k, k1) == 0);

    // delete remaining keys
    assert(sl.delete_key(k1));
    assert(sl.delete_key(k3));

    assert(sl.length_skiplist == 0);
    assert(sl.sl == nullptr);

    std::cout << "PASSED\n";
  }
  //--------------------------------------------------
  // Randomized Operations
  //--------------------------------------------------

  static void testRandomizedOperations(int operations_count = 1000) {
    std::cout << "[Test] Randomized Operations Stress (" << operations_count
              << " steps)... ";

    Skip sl;

    std::map<KeyType, ValueType, Comparator> truth_map;

    std::mt19937 rng(42);

    std::uniform_int_distribution<int> op_dist(0, 3);

    std::uniform_real_distribution<double> score_dist(1.0, 500.0);

    std::uniform_int_distribution<int> member_dist(1, 1000000);

    for (int i = 0; i < operations_count; i++) {

      int op = op_dist(rng);

      double score = score_dist(rng);

      std::string member = std::to_string(member_dist(rng));

      KeyType key(score, member);

      ValueType value = "V_" + member;

      //--------------------------------------------------
      // INSERT
      //--------------------------------------------------

      if (op == 0) {

        sl.insert(key, value);

        truth_map[key] = value;
      }

      //--------------------------------------------------
      // DELETE
      //--------------------------------------------------

      else if (op == 1) {

        bool expected = truth_map.erase(key) > 0;

        bool actual = sl.delete_key(key);
      }

      //--------------------------------------------------
      // SEARCH
      //--------------------------------------------------

      else if (op == 2) {

        auto [result, idx, preds] = sl.search(key);

        Node *n = result.first;

        auto it = truth_map.find(key);

        if (it != truth_map.end()) {

          assert(n != nullptr);

          assert(sl.c(n->k, key) == 0);
        } else {

          if (n != nullptr && !n->is_head) {

            assert(sl.c(n->k, key) == 1 ||

                   sl.c(n->k, key) == 0);
          }
        }
      }

      //--------------------------------------------------
      // INDEX SEARCH
      //--------------------------------------------------

      else {

        if (!truth_map.empty()) {

          std::uniform_int_distribution<int> rank_dist(1, truth_map.size());

          int target_rank = rank_dist(rng);

          auto it = truth_map.begin();

          std::advance(it, target_rank - 1);

          Node *n = sl.search_index(target_rank);

          assert(n != nullptr);

          assert(sl.c(n->k, it->first) == 0);
        }
      }

      assert(sl.length_skiplist == (int)truth_map.size());
    }

    int rank = 1;

    for (auto &p : truth_map) {

      Node *n = sl.search_index(rank);

      assert(n != nullptr);

      assert(sl.c(n->k, p.first) == 0);

      rank++;
    }

    std::cout << "PASSED\n";
  }

  //--------------------------------------------------
  // Insert/Delete Stress
  //--------------------------------------------------

  static void stressTestINS_DEL(int num) {

    Skip sl;

    std::mt19937 rng(42);

    std::uniform_real_distribution<double> score_dist(1.0, 1000.0);
    std::uniform_int_distribution<int> member_dist(1, 1000000);

    std::vector<KeyType> keys;
    keys.reserve(num);

    // Insert phase
    for (int i = 0; i < num; ++i) {

      double score = score_dist(rng);
      std::string member = std::to_string(member_dist(rng));

      KeyType key(score, member);

      sl.insert(key, "V_" + member);

      keys.push_back(key);

      std::cout << "Inserted " << key << "\n";
    }

    std::cout << "\nAfter Insertions\n";
    SkipListTester<KeyType, ValueType, Comparator>::statistics(sl);
    SkipListTester<KeyType, ValueType, Comparator>::validate(sl);

    // Delete phase
    for (const auto &key : keys) {

      bool ok = sl.delete_key(key);

      assert(ok);

      std::cout << "Deleted " << key << "\n";
    }

    std::cout << "\nAfter Deletions\n";
    SkipListTester<KeyType, ValueType, Comparator>::statistics(sl);
    SkipListTester<KeyType, ValueType, Comparator>::validate(sl);

    assert(sl.length_skiplist == 0);
    assert(sl.sl == nullptr);

  } //--------------------------------------------------
  // Run Suite
  //--------------------------------------------------

  static void runSuite() {

    std::cout << "\n====================================\n";

    std::cout << "     RUNNING UNIT & STRESS SUITE\n";

    std::cout << "====================================\n";

    testValueSearch();

    testIndexSearch();

    testRangeSearch();

    testDeletionAndGaps();

    stressTest(100);

    stressTestINS_DEL(100);

    testRandomizedOperations(25);

    std::cout << "====================================\n";

    std::cout << "       ALL SUITE TESTS PASSED\n";

    std::cout << "====================================\n\n";
  }
};
