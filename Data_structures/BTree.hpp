#include <algorithm>
#include <iterator>
#include <vector>
class BTree {
  /*
    size/order is 4
    insertion
    deletion
  */
public:
  int order = 4;
  typedef struct BNode {
    BNode *parent = nullptr;
    std::vector<int> keys;
    std::vector<BNode *> vnode;
  } BNode;

  BNode *tree = nullptr;

  std::pair<int, BNode *> search(BNode *tree, int k, BNode *parent = nullptr) {
    // search current node if found stop or else ..
    if (tree == nullptr)
      return {-1, parent};

    int index_res = -1;
    for (int i = 0; i < tree->keys.size(); i++) {
      if (tree->keys[i] == k)
        return {k, tree};
      if (tree->keys[i] < k) {
        index_res = i;
      }
    }
    if (index_res == -1) {
      return search(tree->vnode[0], k, tree);

    } else if (index_res == tree->keys.size() - 1) {
      return search(tree->vnode[tree->vnode.size() - 1], k, tree);
    }
    return search(tree->vnode[index_res + 1], k, tree);
  };
  void rearrange_root_on_insertion(BNode *tr, int k,
                                   BNode *right_child = nullptr) {
    int cur_node_size = tr->keys.size();
    if (cur_node_size + 1 <= order) {

      auto insert_it = std::lower_bound(tr->keys.begin(), tr->keys.end(), k);

      int insert_idx = std::distance(tr->keys.begin(), insert_it);

      tr->keys.insert(insert_it, k);

      if (tr->vnode.empty()) {

        tr->vnode.resize(tr->keys.size() + 1, nullptr);

      } else {

        tr->vnode.insert(tr->vnode.begin() + insert_idx + 1, right_child);
      }
      // update keys vector
      // update keys vnode as well
    } else {
      auto insert_it = std::lower_bound(tr->keys.begin(), tr->keys.end(), k);
      int insert_idx = std::distance(tr->keys.begin(), insert_it);
      tr->keys.insert(insert_it, k);

      tr->vnode.insert(tr->vnode.begin() + insert_idx + 1, right_child);

      int median_index = tr->keys.size() / 2;
      int median_value = tr->keys[median_index];

      BNode *right_node = new BNode();
      right_node->parent = tr->parent;

      right_node->keys.assign(tr->keys.begin() + median_index + 1,
                              tr->keys.end());
      right_node->vnode.assign(tr->vnode.begin() + median_index + 1,
        tr->vnode.end());

      for (auto *child : right_node->vnode) {
        if (child)
          child->parent = right_node;
      }

      tr->keys.erase(tr->keys.begin() + median_index, tr->keys.end());
      tr->vnode.erase(tr->vnode.begin() + median_index + 1, tr->vnode.end());

      if (tr->parent == nullptr) {
        // Current node was root; create a new root
        BNode *new_root = new BNode();
        new_root->keys.push_back(median_value);
        new_root->vnode.push_back(tr);
        new_root->vnode.push_back(right_node);
        tr->parent = new_root;
        right_node->parent = new_root;
        this->tree = new_root;

      } else {

        rearrange_root_on_insertion(tr->parent, median_value, right_node);
      }
    }
  }
  void insertion(int k) {
    auto search_pos = search(tree, k);
    if (search_pos.first >= 0)
      return;
    if (tree == nullptr) {
      BNode *new_root = new BNode();
      new_root->keys.push_back(k);
      new_root->vnode.resize(2, nullptr);
      new_root->parent = nullptr;
      tree = new_root;
      return;
    }
    rearrange_root_on_insertion(search_pos.second, k);
  };
  void deletion(int k) {

    auto pos_ser = search(tree, k);

    if (pos_ser.second == nullptr)
      return;

    auto item_it = std::lower_bound(pos_ser.second->keys.begin(),
                                    pos_ser.second->keys.end(), k);

    if (item_it == pos_ser.second->keys.end() || *item_it != k)
      return;

    auto item_index = std::distance(pos_ser.second->keys.begin(), item_it);

    BNode *start_point = nullptr;

    bool is_leaf = true;

    for (auto child : pos_ser.second->vnode) {
      if (child != nullptr) {
        is_leaf = false;
        break;
      }
    }

    if (is_leaf) {

      pos_ser.second->keys.erase(item_it);

      if (!pos_ser.second->vnode.empty())
        pos_ser.second->vnode.erase(pos_ser.second->vnode.begin() + item_index +
                                    1);

      // root became empty
      if (pos_ser.second == tree && pos_ser.second->keys.empty()) {

        delete tree;
        tree = nullptr;
        return;
      }

      start_point = pos_ser.second;
    }

    else {

      // predecessor
      if (item_index > 0 && pos_ser.second->vnode[item_index] != nullptr) {

        auto X = pos_ser.second->vnode[item_index];

        while (!X->vnode.empty() && X->vnode.back() != nullptr) {
          X = X->vnode.back();
        }

        int pred = X->keys.back();

        pos_ser.second->keys[item_index] = pred;

        X->keys.pop_back();

        if (!X->vnode.empty())
          X->vnode.pop_back();

        start_point = X;
      }

      // successor
      else if (item_index + 1 < pos_ser.second->vnode.size() &&
               pos_ser.second->vnode[item_index + 1] != nullptr) {

        auto X = pos_ser.second->vnode[item_index + 1];

        while (!X->vnode.empty() && X->vnode.front() != nullptr) {
          X = X->vnode.front();
        }

        int succ = X->keys.front();

        pos_ser.second->keys[item_index] = succ;

        X->keys.erase(X->keys.begin());

        if (!X->vnode.empty())
          X->vnode.erase(X->vnode.begin());

        start_point = X;
      }
    }

    int min_keys = (order + 1) / 2 - 1;

    auto X = start_point;

    while (X != nullptr) {

      // root  case where we only have single node
      if (X == tree) {

        if (X->keys.empty() && !X->vnode.empty()) {

          tree = X->vnode[0];

          if (tree)
            tree->parent = nullptr;

          delete X;
        }

        return;
      }

      // node valid
      if (X->keys.size() >= min_keys)
        break;

      auto P = X->parent;

      if (P == nullptr)
        break;

      int start_point_index = -1;

      for (int i = 0; i < P->vnode.size(); i++) {

        if (P->vnode[i] == X) {
          start_point_index = i;
          break;
        }
      }

      bool has_left = start_point_index > 0;

      bool has_right = start_point_index + 1 < P->vnode.size();

      if (has_left && P->vnode[start_point_index - 1] &&
          P->vnode[start_point_index - 1]->keys.size() > min_keys) {

        auto l = P->vnode[start_point_index - 1];
        auto def = P->vnode[start_point_index];

        int sep = P->keys[start_point_index - 1];

        int borrowed = l->keys.back();

        auto moved_child = l->vnode.back();

        l->keys.pop_back();

        if (!l->vnode.empty())
          l->vnode.pop_back();

        P->keys[start_point_index - 1] = borrowed;

        def->keys.insert(def->keys.begin(), sep);

        bool leaf = true;

        for (auto c : def->vnode) {
          if (c != nullptr) {
            leaf = false;
            break;
          }
        }

        if (leaf) {
          def->vnode.insert(def->vnode.begin(), nullptr);

        } else {

          def->vnode.insert(def->vnode.begin(), moved_child);

          if (moved_child)
            moved_child->parent = def;
        }

        break;
      }

      else if (has_right && P->vnode[start_point_index + 1] &&
               P->vnode[start_point_index + 1]->keys.size() > min_keys) {

        auto r = P->vnode[start_point_index + 1];
        auto def = P->vnode[start_point_index];

        int sep = P->keys[start_point_index];

        int borrowed = r->keys.front();

        auto moved_child = r->vnode.front();

        r->keys.erase(r->keys.begin());

        if (!r->vnode.empty())
          r->vnode.erase(r->vnode.begin());

        P->keys[start_point_index] = borrowed;

        def->keys.push_back(sep);

        bool leaf = true;

        for (auto c : def->vnode) {
          if (c != nullptr) {
            leaf = false;
            break;
          }
        }

        if (leaf) {
          def->vnode.push_back(nullptr);

        } else {

          def->vnode.push_back(moved_child);

          if (moved_child)
            moved_child->parent = def;
        }

        break;
      }

      else {

        BNode *left = nullptr;
        BNode *right = nullptr;

        int sep_index = -1;

        if (has_left) {

          left = P->vnode[start_point_index - 1];
          right = P->vnode[start_point_index];

          sep_index = start_point_index - 1;

        } else {

          left = P->vnode[start_point_index];
          right = P->vnode[start_point_index + 1];

          sep_index = start_point_index;
        }

        BNode *merged = new BNode();

        merged->keys.insert(merged->keys.end(), left->keys.begin(),
                            left->keys.end());

        merged->keys.push_back(P->keys[sep_index]);

        merged->keys.insert(merged->keys.end(), right->keys.begin(),
                            right->keys.end());

        merged->vnode.insert(merged->vnode.end(), left->vnode.begin(),
                             left->vnode.end());

        merged->vnode.insert(merged->vnode.end(), right->vnode.begin(),
                             right->vnode.end());

        for (auto child : merged->vnode) {

          if (child)
            child->parent = merged;
        }

        merged->parent = P;

        P->keys.erase(P->keys.begin() + sep_index);

        P->vnode[sep_index] = merged;

        P->vnode.erase(P->vnode.begin() + sep_index + 1);

        delete left;
        delete right;

        X = P;
      }
    }
  }
};
// --- Testes start here ----
#include <iostream>
#include <queue>
#include <random>
#include <set>

// ========================================
// PRETTY TREE PRINTER
// ========================================

void print_tree(BTree::BNode *root) {

  if (!root) {
    std::cout << "EMPTY TREE\n";
    return;
  }

  std::queue<BTree::BNode *> q;
  q.push(root);

  int level = 0;

  while (!q.empty()) {

    int sz = q.size();

    std::cout << "LEVEL " << level << " : ";

    for (int i = 0; i < sz; i++) {

      auto *cur = q.front();
      q.pop();

      std::cout << "[ ";

      for (auto k : cur->keys)
        std::cout << k << " ";

      std::cout << "] ";

      for (auto child : cur->vnode) {
        if (child)
          q.push(child);
      }
    }

    std::cout << "\n";
    level++;
  }
}

// ========================================
// VALIDATOR
// ========================================

bool validate_node(BTree::BNode *node, BTree::BNode *parent, int order) {

  if (!node)
    return true;

  // --------------------------------
  // parent ptr validation
  // --------------------------------

  if (node->parent != parent) {
    std::cout << "PARENT PTR FAILED\n";
    return false;
  }

  // --------------------------------
  // sorted keys validation
  // --------------------------------

  for (int i = 1; i < node->keys.size(); i++) {

    if (node->keys[i - 1] >= node->keys[i]) {

      std::cout << "KEY ORDER FAILED\n";

      return false;
    }
  }

  // --------------------------------
  // vnode invariant
  // --------------------------------

  if (!node->vnode.empty()) {

    if (node->vnode.size() != node->keys.size() + 1) {

      std::cout << "VNODE SIZE FAILED\n";

      std::cout << "keys : " << node->keys.size()
                << " vnode : " << node->vnode.size() << "\n";

      return false;
    }
  }

  // --------------------------------
  // order validation
  // --------------------------------

  if (node->keys.size() > order) {

    std::cout << "ORDER VIOLATION\n";

    return false;
  }

  // --------------------------------
  // recursive validation
  // --------------------------------

  for (auto child : node->vnode) {

    if (!validate_node(child, node, order))
      return false;
  }

  return true;
}

// ========================================
// TREE VALIDATOR
// ========================================

bool validate_tree(BTree &bt) {

  return validate_node(bt.tree, nullptr, bt.order);
}

// ========================================
// INSERT TEST
// ========================================

void test_basic_insertions() {

  std::cout << "\n========================\n";
  std::cout << "BASIC INSERT TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::vector<int> vals = {10, 20, 5, 6, 12, 30, 7, 17};

  for (auto v : vals) {

    std::cout << "\nINSERT : " << v << "\n";

    bt.insertion(v);

    print_tree(bt.tree);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER INSERT " << v << "\n";

      return;
    }
  }

  std::cout << "\nBASIC INSERTIONS PASSED\n";
}

// ========================================
// ASCENDING TEST
// ========================================

void test_ascending_insertions() {

  std::cout << "\n========================\n";
  std::cout << "ASCENDING INSERT TEST\n";
  std::cout << "========================\n";

  BTree bt;

  for (int i = 1; i <= 50; i++) {

    bt.insertion(i);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AT " << i << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nASCENDING TEST PASSED\n";
}

// ========================================
// DESCENDING TEST
// ========================================

void test_descending_insertions() {

  std::cout << "\n========================\n";
  std::cout << "DESCENDING INSERT TEST\n";
  std::cout << "========================\n";

  BTree bt;

  for (int i = 50; i >= 1; i--) {

    bt.insertion(i);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AT " << i << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nDESCENDING TEST PASSED\n";
}

// ========================================
// RANDOM STRESS TEST
// ========================================

void test_random_insertions() {

  std::cout << "\n========================\n";
  std::cout << "RANDOM INSERT TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::mt19937 rng(42);

  std::uniform_int_distribution<int> dist(1, 1000);

  std::set<int> inserted;

  for (int i = 0; i < 100; i++) {

    int val = dist(rng);

    inserted.insert(val);

    bt.insertion(val);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER RANDOM INSERT " << val << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nRANDOM TEST PASSED\n";
}

// ========================================
// DELETE TESTS
// ========================================

void test_basic_deletions() {

  std::cout << "\n========================\n";
  std::cout << "BASIC DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::vector<int> vals = {10, 20, 5, 6, 12, 30, 7, 17};

  for (auto v : vals)
    bt.insertion(v);

  print_tree(bt.tree);

  std::vector<int> dels = {6, 7, 5, 10, 12, 17, 20, 30};

  for (auto d : dels) {

    std::cout << "\nDELETE : " << d << "\n";

    bt.deletion(d);

    print_tree(bt.tree);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER DELETE " << d << "\n";

      return;
    }
  }

  std::cout << "\nBASIC DELETE TEST PASSED\n";
}

// ========================================
// ASCENDING DELETE TEST
// ========================================

void test_ascending_deletions() {

  std::cout << "\n========================\n";
  std::cout << "ASCENDING DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  for (int i = 1; i <= 100; i++)
    bt.insertion(i);

  for (int i = 1; i <= 100; i++) {

    bt.deletion(i);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AT DELETE " << i << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nASCENDING DELETE TEST PASSED\n";
}

// ========================================
// DESCENDING DELETE TEST
// ========================================

void test_descending_deletions() {

  std::cout << "\n========================\n";
  std::cout << "DESCENDING DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  for (int i = 1; i <= 100; i++)
    bt.insertion(i);

  for (int i = 100; i >= 1; i--) {

    bt.deletion(i);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AT DELETE " << i << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nDESCENDING DELETE TEST PASSED\n";
}

// ========================================
// INTERNAL NODE DELETE TEST
// ========================================

void test_internal_deletions() {

  std::cout << "\n========================\n";
  std::cout << "INTERNAL NODE DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::vector<int> vals = {50, 20, 70, 10, 30, 60, 80, 5,
                           15, 25, 35, 55, 65, 75, 85};

  for (auto v : vals)
    bt.insertion(v);

  print_tree(bt.tree);

  std::vector<int> dels = {50, 70, 20, 60, 30};

  for (auto d : dels) {

    std::cout << "\nDELETE INTERNAL : " << d << "\n";

    bt.deletion(d);

    print_tree(bt.tree);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER INTERNAL DELETE " << d << "\n";

      return;
    }
  }

  std::cout << "\nINTERNAL DELETE TEST PASSED\n";
}

// ========================================
// RANDOM DELETE STRESS TEST
// ========================================

void test_random_deletions() {

  std::cout << "\n========================\n";
  std::cout << "RANDOM DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::vector<int> vals;

  for (int i = 1; i <= 500; i++)
    vals.push_back(i);

  std::mt19937 rng(42);

  std::shuffle(vals.begin(), vals.end(), rng);

  for (auto v : vals)
    bt.insertion(v);

  bool ok_insert = validate_tree(bt);

  if (!ok_insert) {

    std::cout << "FAILED AFTER RANDOM INSERT BUILD\n";

    print_tree(bt.tree);

    return;
  }

  std::shuffle(vals.begin(), vals.end(), rng);

  for (auto v : vals) {

    bt.deletion(v);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER RANDOM DELETE " << v << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nRANDOM DELETE TEST PASSED\n";
}

// ========================================
// DUPLICATE DELETE TEST
// ========================================

void test_duplicate_delete_calls() {

  std::cout << "\n========================\n";
  std::cout << "DUPLICATE DELETE TEST\n";
  std::cout << "========================\n";

  BTree bt;

  for (int i = 1; i <= 20; i++)
    bt.insertion(i);

  for (int i = 1; i <= 20; i++) {

    bt.deletion(i);

    bt.deletion(i);

    bt.deletion(i);

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED AFTER DUPLICATE DELETE " << i << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nDUPLICATE DELETE TEST PASSED\n";
}

// ========================================
// MIXED RANDOM OPS TEST
// ========================================

void test_mixed_random_operations() {

  std::cout << "\n========================\n";
  std::cout << "MIXED RANDOM OPS TEST\n";
  std::cout << "========================\n";

  BTree bt;

  std::mt19937 rng(42);

  std::uniform_int_distribution<int> valdist(1, 1000);
  std::uniform_int_distribution<int> opdist(0, 1);

  std::set<int> alive;

  for (int i = 0; i < 2000; i++) {

    int val = valdist(rng);

    int op = opdist(rng);

    if (op == 0) {

      bt.insertion(val);

      alive.insert(val);

    } else {

      bt.deletion(val);

      alive.erase(val);
    }

    bool ok = validate_tree(bt);

    if (!ok) {

      std::cout << "FAILED DURING MIXED OPS\n";

      std::cout << "iteration : " << i << "\n";

      std::cout << "value : " << val << "\n";

      std::cout << "op : " << (op == 0 ? "insert" : "delete") << "\n";

      print_tree(bt.tree);

      return;
    }
  }

  print_tree(bt.tree);

  std::cout << "\nMIXED RANDOM OPS TEST PASSED\n";
}

// ========================================
// MAIN
// ========================================

// int main() {
//
//   test_basic_insertions();
//
//   test_ascending_insertions();
//
//   test_descending_insertions();
//
//   test_random_insertions();
//
//   test_basic_deletions();
//
//   test_ascending_deletions();
//
//   test_descending_deletions();
//
//   test_internal_deletions();
//
//   test_random_deletions();
//
//   test_duplicate_delete_calls();
//
//   test_mixed_random_operations();
//
//   return 0;
// }
