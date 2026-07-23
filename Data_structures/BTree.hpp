#pragma once
#include <algorithm>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

template <typename KeyType, typename ValueType, typename Comparator>
class BTree {
private:
  Comparator cmp;

public:
  int order = 4;

  struct BNode {
    BNode *parent = nullptr;

    std::vector<KeyType> keys;

    std::vector<ValueType> values;

    std::vector<BNode *> vnode;
  };

  BNode *tree = nullptr;

public:
  BTree() = default;

  //------------------------------------------------------------
  // SEARCH
  //------------------------------------------------------------

  std::pair<BNode *, int> search(BNode *root, const KeyType &key,
                                 BNode *parent = nullptr) {
    if (root == nullptr)
      return {parent, -1};

    int child_index = 0;

    for (int i = 0; i < (int)root->keys.size(); i++) {
      int c = cmp(root->keys[i], key);

      if (c == 0)
        return {root, i};

      if (c == -1)
        child_index = i + 1;
      else
        break;
    }

    return search(root->vnode[child_index], key, root);
  }

private:
  //------------------------------------------------------------
  // INSERT INTO NODE
  //------------------------------------------------------------

  void rearrange_root_on_insertion(BNode *node, const KeyType &key,
                                   const ValueType &value,
                                   BNode *right_child = nullptr) {

    auto insert_it = std::lower_bound(
        node->keys.begin(), node->keys.end(), key,
        [&](const KeyType &a, const KeyType &b) { return cmp(a, b) == -1; });

    int insert_index = std::distance(node->keys.begin(), insert_it);

    node->keys.insert(insert_it, key);

    node->values.insert(node->values.begin() + insert_index, value);

    //----------------------------------------------------
    // LEAF
    //----------------------------------------------------

    if (node->vnode.empty()) {
      node->vnode.resize(node->keys.size() + 1, nullptr);
    }

    //----------------------------------------------------
    // INTERNAL
    //----------------------------------------------------

    else {
      node->vnode.insert(node->vnode.begin() + insert_index + 1, right_child);

      if (right_child)
        right_child->parent = node;
    }

    //----------------------------------------------------
    // NO SPLIT
    //----------------------------------------------------

    if ((int)node->keys.size() <= order)
      return;

    //----------------------------------------------------
    // SPLIT
    //----------------------------------------------------

    int median = node->keys.size() / 2;

    KeyType promoted_key = node->keys[median];

    ValueType promoted_value = node->values[median];

    BNode *right_node = new BNode();

    right_node->parent = node->parent;

    //----------------------------------------------------
    // MOVE RIGHT KEYS
    //----------------------------------------------------

    right_node->keys.assign(node->keys.begin() + median + 1, node->keys.end());

    right_node->values.assign(node->values.begin() + median + 1,
                              node->values.end());

    //----------------------------------------------------
    // MOVE CHILDREN
    //----------------------------------------------------

    right_node->vnode.assign(node->vnode.begin() + median + 1,
                             node->vnode.end());

    for (auto child : right_node->vnode) {
      if (child)
        child->parent = right_node;
    }

    //----------------------------------------------------
    // SHRINK LEFT NODE
    //----------------------------------------------------

    node->keys.erase(node->keys.begin() + median, node->keys.end());

    node->values.erase(node->values.begin() + median, node->values.end());

    node->vnode.erase(node->vnode.begin() + median + 1, node->vnode.end());

    //----------------------------------------------------
    // ROOT SPLIT
    //----------------------------------------------------

    if (node->parent == nullptr) {
      BNode *new_root = new BNode();

      new_root->keys.push_back(promoted_key);

      new_root->values.push_back(promoted_value);

      new_root->vnode.push_back(node);
      new_root->vnode.push_back(right_node);

      node->parent = new_root;
      right_node->parent = new_root;

      tree = new_root;

      return;
    }

    //----------------------------------------------------
    // PROPAGATE UP
    //----------------------------------------------------

    rearrange_root_on_insertion(node->parent, promoted_key, promoted_value,
                                right_node);
  }

public:
  //------------------------------------------------------------
  // INSERT
  //------------------------------------------------------------

  void insertion(const KeyType &key, const ValueType &value) {

    if (tree == nullptr) {
      tree = new BNode();

      tree->keys.push_back(key);

      tree->values.push_back(value);

      tree->vnode.resize(2, nullptr);

      return;
    }

    auto result = search(tree, key);

    //----------------------------------------------------
    // DUPLICATE KEY
    //----------------------------------------------------

    if (result.second != -1) {
      result.first->values[result.second] = value;

      return;
    }

    rearrange_root_on_insertion(result.first, key, value);
  }
  //------------------------------------------------------------
  // DELETE
  //------------------------------------------------------------

  void deletion(const KeyType &key) {
    auto pos = search(tree, key);

    if (pos.second == -1)
      return;

    BNode *node = pos.first;
    int key_index = pos.second;

    BNode *rebalance_node = nullptr;

    //--------------------------------------------------------
    // LEAF ?
    //--------------------------------------------------------

    bool is_leaf = true;

    for (auto child : node->vnode) {
      if (child) {
        is_leaf = false;
        break;
      }
    }

    //--------------------------------------------------------
    // CASE 1 : LEAF
    //--------------------------------------------------------

    if (is_leaf) {
      node->keys.erase(node->keys.begin() + key_index);

      node->values.erase(node->values.begin() + key_index);

      if (!node->vnode.empty()) {
        node->vnode.erase(node->vnode.begin() + key_index + 1);
      }

      if (node == tree && node->keys.empty()) {
        delete tree;
        tree = nullptr;
        return;
      }

      rebalance_node = node;
    }

    //--------------------------------------------------------
    // CASE 2 : INTERNAL
    //--------------------------------------------------------

    else {
      //----------------------------------------------------
      // PREDECESSOR
      //----------------------------------------------------

      if (node->vnode[key_index] != nullptr) {
        auto X = node->vnode[key_index];

        while (!X->vnode.empty() && X->vnode.back() != nullptr) {
          X = X->vnode.back();
        }

        node->keys[key_index] = X->keys.back();

        node->values[key_index] = X->values.back();

        X->keys.pop_back();
        X->values.pop_back();

        if (!X->vnode.empty())
          X->vnode.pop_back();

        rebalance_node = X;
      }

      //----------------------------------------------------
      // SUCCESSOR
      //----------------------------------------------------

      else {
        auto X = node->vnode[key_index + 1];

        while (!X->vnode.empty() && X->vnode.front() != nullptr) {
          X = X->vnode.front();
        }

        node->keys[key_index] = X->keys.front();

        node->values[key_index] = X->values.front();

        X->keys.erase(X->keys.begin());
        X->values.erase(X->values.begin());

        if (!X->vnode.empty())
          X->vnode.erase(X->vnode.begin());

        rebalance_node = X;
      }
    }

    //--------------------------------------------------------
    // REBALANCE
    //--------------------------------------------------------

    int min_keys = (order + 1) / 2 - 1;

    BNode *X = rebalance_node;

    while (X != nullptr) {
      //----------------------------------------------------
      // ROOT
      //----------------------------------------------------

      if (X == tree) {
        if (tree->keys.empty() && !tree->vnode.empty()) {
          tree = tree->vnode[0];

          if (tree)
            tree->parent = nullptr;

          delete X;
        }

        return;
      }

      //----------------------------------------------------
      // VALID
      //----------------------------------------------------

      if ((int)X->keys.size() >= min_keys)
        return;

      auto P = X->parent;

      int idx = 0;

      while (P->vnode[idx] != X)
        idx++;

      bool has_left = (idx > 0);
      bool has_right = idx + 1 < P->vnode.size();

      //----------------------------------------------------
      // BORROW LEFT
      //----------------------------------------------------

      if (has_left && P->vnode[idx - 1] &&
          (int)P->vnode[idx - 1]->keys.size() > min_keys) {
        auto L = P->vnode[idx - 1];

        KeyType parent_key = P->keys[idx - 1];

        ValueType parent_value = P->values[idx - 1];

        P->keys[idx - 1] = L->keys.back();

        P->values[idx - 1] = L->values.back();

        L->keys.pop_back();
        L->values.pop_back();

        X->keys.insert(X->keys.begin(), parent_key);

        X->values.insert(X->values.begin(), parent_value);

        auto child = L->vnode.back();

        L->vnode.pop_back();

        X->vnode.insert(X->vnode.begin(), child);

        if (child)
          child->parent = X;

        return;
      }

      //----------------------------------------------------
      // BORROW RIGHT
      //----------------------------------------------------

      if (has_right && P->vnode[idx + 1] &&
          (int)P->vnode[idx + 1]->keys.size() > min_keys) {
        auto R = P->vnode[idx + 1];

        KeyType parent_key = P->keys[idx];

        ValueType parent_value = P->values[idx];

        P->keys[idx] = R->keys.front();

        P->values[idx] = R->values.front();

        R->keys.erase(R->keys.begin());
        R->values.erase(R->values.begin());

        X->keys.push_back(parent_key);
        X->values.push_back(parent_value);

        auto child = R->vnode.front();

        R->vnode.erase(R->vnode.begin());

        X->vnode.push_back(child);

        if (child)
          child->parent = X;

        return;
      }

      //----------------------------------------------------
      // MERGE
      //----------------------------------------------------

      BNode *left;
      BNode *right;
      int sep;

      if (has_left) {
        left = P->vnode[idx - 1];
        right = P->vnode[idx];
        sep = idx - 1;
      } else {
        left = P->vnode[idx];
        right = P->vnode[idx + 1];
        sep = idx;
      }

      left->keys.push_back(P->keys[sep]);
      left->values.push_back(P->values[sep]);

      left->keys.insert(left->keys.end(), right->keys.begin(),
                        right->keys.end());

      left->values.insert(left->values.end(), right->values.begin(),
                          right->values.end());

      left->vnode.insert(left->vnode.end(), right->vnode.begin(),
                         right->vnode.end());

      for (auto child : left->vnode) {
        if (child)
          child->parent = left;
      }

      P->keys.erase(P->keys.begin() + sep);

      P->values.erase(P->values.begin() + sep);

      P->vnode[sep] = left;

      P->vnode.erase(P->vnode.begin() + sep + 1);

      delete right;

      X = P;
    }
  }
  ValueType *lookup(const KeyType &key) {
    auto res = search(tree, key);

    if (res.second == -1)
      return {};

    return res.first->values[res.second];
  }
  void flatten_tree(BNode *node,
                    std::vector<std::pair<KeyType, ValueType>> &out) {
    if (node == nullptr)
      return;

    for (size_t i = 0; i < node->keys.size(); i++) {
      if (i < node->vnode.size())
        flatten_tree(node->vnode[i], out);

      out.emplace_back(node->keys[i], node->values[i]);
    }

    if (node->vnode.size() > node->keys.size())
      flatten_tree(node->vnode.back(), out);
  }
  std::vector<std::pair<KeyType, ValueType>> inorder_full_traversal() {
    std::vector<std::pair<KeyType, ValueType>> res;

    flatten_tree(tree, res);

    return res;
  }
  std::vector<std::pair<KeyType, ValueType>>
  inorder_range_traversal(const KeyType &start, const KeyType &end) {
    auto vec = inorder_full_traversal();

    std::vector<std::pair<KeyType, ValueType>> result;

    for (auto &x : vec) {
      if (cmp(x.first, start) != -1 && cmp(x.first, end) != 1) {
        result.push_back(x);
      }
    }

    return result;
  }
  size_t size_of_tree() { return inorder_full_traversal().size(); }

private:
  void print_internal(BNode *node, std::string indent, bool last) {
    if (node == nullptr) {
      std::cout << indent << (last ? "└── " : "├── ") << "NULL\n";
      return;
    }

    std::cout << indent << (last ? "└── " : "├── ");

    std::cout << "[";

    for (size_t i = 0; i < node->keys.size(); i++) {
      std::cout << node->keys[i];

      if (i + 1 != node->keys.size())
        std::cout << ",";
    }

    std::cout << "]\n";

    for (size_t i = 0; i < node->vnode.size(); i++) {
      print_internal(node->vnode[i], indent + (last ? "    " : "│   "),
                     i == node->vnode.size() - 1);
    }
  }

public:
  void printTree() {
    std::cout << "\n========== B TREE ==========\n";

    if (tree == nullptr) {
      std::cout << "EMPTY\n";
      return;
    }

    print_internal(tree, "", true);

    std::cout << "============================\n";
  }
};

#include <algorithm>
#include <iostream>
#include <queue>
#include <random>
#include <set>
#include <vector>

template <typename KeyType, typename ValueType, typename Comparator>
using Tree = BTree<KeyType, ValueType, Comparator>;

//////////////////////////////////////////////////////////////
// PRINT TREE
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void print_tree(typename Tree<KeyType, ValueType, Comparator>::BNode *root) {
  if (root == nullptr) {
    std::cout << "EMPTY TREE\n";
    return;
  }

  using Node = typename Tree<KeyType, ValueType, Comparator>::BNode;

  std::queue<Node *> q;

  q.push(root);

  int level = 0;

  while (!q.empty()) {
    int sz = q.size();

    std::cout << "LEVEL " << level << " : ";

    while (sz--) {
      auto cur = q.front();

      q.pop();

      std::cout << "[ ";

      for (size_t i = 0; i < cur->keys.size(); i++) {
        std::cout << cur->keys[i] << " ";
      }

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

//////////////////////////////////////////////////////////////
// NODE VALIDATOR
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
bool validate_node(typename Tree<KeyType, ValueType, Comparator>::BNode *node,
                   typename Tree<KeyType, ValueType, Comparator>::BNode *parent,
                   int order, Comparator cmp = Comparator()) {
  if (node == nullptr)
    return true;

  //--------------------------------------------------

  if (node->parent != parent) {
    std::cout << "PARENT PTR FAILED\n";
    return false;
  }

  //--------------------------------------------------

  if (node->keys.size() != node->values.size()) {
    std::cout << "KEY VALUE SIZE MISMATCH\n";
    return false;
  }

  //--------------------------------------------------

  for (size_t i = 1; i < node->keys.size(); i++) {
    if (cmp(node->keys[i - 1], node->keys[i]) != -1) {
      std::cout << "KEY ORDER FAILED\n";
      return false;
    }
  }

  //--------------------------------------------------

  if (!node->vnode.empty()) {
    if (node->vnode.size() != node->keys.size() + 1) {
      std::cout << "CHILD COUNT FAILED\n";

      return false;
    }
  }

  //--------------------------------------------------

  if (node->keys.size() > order) {
    std::cout << "ORDER VIOLATION\n";

    return false;
  }

  //--------------------------------------------------

  for (auto child : node->vnode) {
    if (!validate_node<KeyType, ValueType, Comparator>(child, node, order))
      return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////
// TREE VALIDATOR
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
bool validate_tree(Tree<KeyType, ValueType, Comparator> &tree) {
  return validate_node<KeyType, ValueType, Comparator>(tree.tree, nullptr,
                                                       tree.order);
}

//////////////////////////////////////////////////////////////
// BASIC INSERT TEST
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void test_basic_insertions() {
  std::cout << "\n========================\n";
  std::cout << "BASIC INSERT TEST\n";
  std::cout << "========================\n";

  Tree<KeyType, ValueType, Comparator> bt;

  std::vector<KeyType> vals = {10, 20, 5, 6, 12, 30, 7, 17};

  for (const auto &v : vals) {
    std::cout << "\nINSERT : " << v << "\n";

    bt.insertion(v, ValueType(v));

    print_tree<KeyType, ValueType, Comparator>(bt.tree);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER INSERT " << v << "\n";
      return;
    }
  }

  std::cout << "\nBASIC INSERTIONS PASSED\n";
}

//////////////////////////////////////////////////////////////
// ASCENDING INSERTIONS
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void test_ascending_insertions() {
  std::cout << "\n========================\n";
  std::cout << "ASCENDING INSERT TEST\n";
  std::cout << "========================\n";

  Tree<KeyType, ValueType, Comparator> bt;

  for (KeyType i = 1; i <= 50; i++) {
    bt.insertion(i, ValueType(i));

    if (!validate_tree(bt)) {
      std::cout << "FAILED AT " << i << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  print_tree<KeyType, ValueType, Comparator>(bt.tree);

  std::cout << "\nASCENDING INSERT TEST PASSED\n";
}

//////////////////////////////////////////////////////////////
// DESCENDING INSERTIONS
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void test_descending_insertions() {
  std::cout << "\n========================\n";
  std::cout << "DESCENDING INSERT TEST\n";
  std::cout << "========================\n";

  Tree<KeyType, ValueType, Comparator> bt;

  for (KeyType i = 50; i >= 1; i--) {
    bt.insertion(i, ValueType(i));

    if (!validate_tree(bt)) {
      std::cout << "FAILED AT " << i << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  print_tree<KeyType, ValueType, Comparator>(bt.tree);

  std::cout << "\nDESCENDING INSERT TEST PASSED\n";
}

//////////////////////////////////////////////////////////////
// RANDOM INSERTIONS
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void test_random_insertions() {
  std::cout << "\n========================\n";
  std::cout << "RANDOM INSERT TEST\n";
  std::cout << "========================\n";

  Tree<KeyType, ValueType, Comparator> bt;

  std::mt19937 rng(42);

  std::uniform_int_distribution<int> dist(1, 1000);

  std::set<KeyType> inserted;

  for (int i = 0; i < 100; i++) {
    KeyType val = static_cast<KeyType>(dist(rng));

    inserted.insert(val);

    bt.insertion(val, ValueType(val));

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER RANDOM INSERT " << val << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  print_tree<KeyType, ValueType, Comparator>(bt.tree);

  std::cout << "\nRANDOM INSERT TEST PASSED\n";
}
//////////////////////////////////////////////////////////////
// BASIC DELETE TEST
//////////////////////////////////////////////////////////////

template <typename KeyType, typename ValueType, typename Comparator>
void test_basic_deletions() {
  std::cout << "\n========================\n";
  std::cout << "BASIC DELETE TEST\n";
  std::cout << "========================\n";

  Tree<KeyType, ValueType, Comparator> bt;

  std::vector<KeyType> vals = {10, 20, 5, 6, 12, 30, 7, 17};

  for (auto v : vals)
    bt.insertion(v, ValueType(v));

  print_tree<KeyType, ValueType, Comparator>(bt.tree);

  std::vector<KeyType> dels = {6, 7, 5, 10, 12, 17, 20, 30};

  for (auto d : dels) {
    std::cout << "\nDELETE : " << d << "\n";

    bt.deletion(d);

    print_tree<KeyType, ValueType, Comparator>(bt.tree);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER DELETE " << d << "\n";
      return;
    }
  }

  std::cout << "\nBASIC DELETE TEST PASSED\n";
}

template <typename KeyType, typename ValueType, typename Comparator>
void test_ascending_deletions() {
  Tree<KeyType, ValueType, Comparator> bt;

  for (KeyType i = 1; i <= 100; i++)
    bt.insertion(i, ValueType(i));

  for (KeyType i = 1; i <= 100; i++) {
    bt.deletion(i);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AT DELETE " << i << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "ASCENDING DELETE TEST PASSED\n";
}

template <typename KeyType, typename ValueType, typename Comparator>
void test_descending_deletions() {
  Tree<KeyType, ValueType, Comparator> bt;

  for (KeyType i = 1; i <= 100; i++)
    bt.insertion(i, ValueType(i));

  for (KeyType i = 100; i >= 1; i--) {
    bt.deletion(i);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AT DELETE " << i << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "DESCENDING DELETE TEST PASSED\n";
}

template <typename KeyType, typename ValueType, typename Comparator>
void test_internal_deletions() {
  Tree<KeyType, ValueType, Comparator> bt;

  std::vector<KeyType> vals = {50, 20, 70, 10, 30, 60, 80, 5,
                               15, 25, 35, 55, 65, 75, 85};

  for (auto v : vals)
    bt.insertion(v, ValueType(v));

  std::vector<KeyType> dels = {50, 70, 20, 60, 30};

  for (auto d : dels) {
    bt.deletion(d);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER INTERNAL DELETE " << d << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "INTERNAL DELETE TEST PASSED\n";
}

template <typename KeyType, typename ValueType, typename Comparator>
void test_random_deletions() {
  Tree<KeyType, ValueType, Comparator> bt;

  std::vector<KeyType> vals;

  for (KeyType i = 1; i <= 500; i++)
    vals.push_back(i);

  std::mt19937 rng(42);

  std::shuffle(vals.begin(), vals.end(), rng);

  for (auto v : vals)
    bt.insertion(v, ValueType(v));

  if (!validate_tree(bt)) {
    std::cout << "FAILED AFTER BUILD\n";
    return;
  }

  std::shuffle(vals.begin(), vals.end(), rng);

  for (auto v : vals) {
    bt.deletion(v);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER DELETE " << v << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "RANDOM DELETE TEST PASSED\n";
}
template <typename KeyType, typename ValueType, typename Comparator>
void test_duplicate_delete_calls() {
  Tree<KeyType, ValueType, Comparator> bt;

  for (KeyType i = 1; i <= 20; i++)
    bt.insertion(i, ValueType(i));

  for (KeyType i = 1; i <= 20; i++) {
    bt.deletion(i);
    bt.deletion(i);
    bt.deletion(i);

    if (!validate_tree(bt)) {
      std::cout << "FAILED AFTER DUPLICATE DELETE " << i << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "DUPLICATE DELETE TEST PASSED\n";
}

// ========================================
// MIXED RANDOM OPS TEST
// ========================================

template <typename KeyType, typename ValueType, typename Comparator>
void test_mixed_random_operations() {
  Tree<KeyType, ValueType, Comparator> bt;

  std::mt19937 rng(42);

  std::uniform_int_distribution<int> value_dist(1, 1000);
  std::uniform_int_distribution<int> op_dist(0, 1);

  std::set<KeyType> alive;

  for (int i = 0; i < 2000; i++) {
    KeyType value = static_cast<KeyType>(value_dist(rng));

    if (op_dist(rng) == 0) {
      bt.insertion(value, ValueType(value));

      alive.insert(value);
    } else {
      bt.deletion(value);

      alive.erase(value);
    }

    if (!validate_tree(bt)) {
      std::cout << "FAILED DURING RANDOM OPS\n";

      std::cout << "iteration : " << i << "\n";

      std::cout << "value : " << value << "\n";

      print_tree<KeyType, ValueType, Comparator>(bt.tree);

      return;
    }
  }

  std::cout << "MIXED RANDOM TEST PASSED\n";
}

struct IntComparator {
  int operator()(const int &a, const int &b) const {
    if (a < b)
      return -1;
    if (a > b)
      return 1;
    return 0;
  }
};

int main() {
  test_basic_insertions<int, int, IntComparator>();

  test_ascending_insertions<int, int, IntComparator>();

  test_descending_insertions<int, int, IntComparator>();

  test_random_insertions<int, int, IntComparator>();

  test_basic_deletions<int, int, IntComparator>();

  test_ascending_deletions<int, int, IntComparator>();

  test_descending_deletions<int, int, IntComparator>();

  test_internal_deletions<int, int, IntComparator>();

  test_random_deletions<int, int, IntComparator>();

  test_duplicate_delete_calls<int, int, IntComparator>();

  test_mixed_random_operations<int, int, IntComparator>();

  std::cout << "\n=====================================\n";
  std::cout << "All tests completed successfully.\n";
  std::cout << "=====================================\n";

  return 0;
}
