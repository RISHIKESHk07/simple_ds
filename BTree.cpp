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
    auto item_it = std::lower_bound(pos_ser.second->keys.begin(),
                                    pos_ser.second->keys.end(), k);
    auto item_index = std::distance(pos_ser.second->keys.begin(), item_it);
    BNode *start_point;

    bool is_leaf = true;

    for (auto child : pos_ser.second->vnode) {
      if (child != nullptr) {
        is_leaf = false;
        break;
      }
    }

    if (is_leaf) {
      // leaf case

      pos_ser.second->keys.erase(item_it);
      pos_ser.second->vnode.erase(pos_ser.second->vnode.begin() + item_index +
                                  1);
      start_point = pos_ser.second->parent;

    } else {
      // internal node or root
      if (pos_ser.second->vnode[item_index - 1] != nullptr) {
        // left tree largest
        auto X = pos_ser.second->vnode[item_index - 1];
        while (X->vnode.back() != nullptr) {
          X = X->vnode.back();
        }
        auto l_lg = X->keys.back();
        X->keys.pop_back();
        X->vnode.pop_back();
        start_point = X->parent;
        pos_ser.second->keys[item_index] = l_lg;
      } else if (pos_ser.second->vnode[item_index + 1] != nullptr) {
        // right tree smallest
        auto X = pos_ser.second->vnode[item_index + 1];
        while (X->vnode.front() != nullptr) {
          X = X->vnode.front();
        }
        auto r_sm = X->keys.front();
        X->keys.erase(X->keys.begin());
        X->vnode.erase(X->vnode.begin());
        start_point = X->parent;
        pos_ser.second->keys[item_index] = r_sm;
      }
    }
    // rebalance

    auto X = start_point;
    while (X != nullptr) {

      if (start_point->keys.size() >= this->order / 2)
        break;

      int start_point_index = -1;

      for (int i = 0; i < X->vnode.size(); i++) {
        if (X->vnode[i] == start_point) {
          start_point_index = i;
          break;
        }
      }

      if (X->vnode[start_point_index - 1] &&
          X->vnode[start_point_index - 1]->keys.size() > (this->order / 2)) {
        // right rotate
        auto l = X->vnode[start_point_index - 1];
        auto X_org = X->keys[start_point_index - 1];
        auto def = X->vnode[start_point_index];

        auto l_val = l->keys.back();
        l->keys.pop_back();
        l->vnode.pop_back();

        X->keys[start_point_index - 1] = l_val;

        def->keys.insert(def->keys.begin(), X_org);
        auto moved_child = l->vnode.back();
        l->vnode.pop_back();

        def->vnode.insert(def->vnode.begin(), moved_child);

        if (moved_child)
          moved_child->parent = def;

      } else if (X->vnode[start_point_index + 1] &&
                 X->vnode[start_point_index + 1]->keys.size() >
                     (this->order / 2)) {
        auto r = X->vnode[start_point_index + 1];
        auto X_org = X->keys[start_point_index];
        auto def = X->vnode[start_point_index];

        auto r_val = r->keys.front();
        r->keys.erase(r->keys.begin());
        r->vnode.erase(r->vnode.begin());

        X->keys[start_point_index] = r_val;

        def->keys.insert(def->keys.end() - 1, X_org);
        auto moved_child = r->vnode.front();
        r->vnode.erase(r->vnode.begin());

        def->vnode.insert(def->vnode.begin(), moved_child);

        if (moved_child)
          moved_child->parent = def;

      } else if (X->vnode[start_point_index - 1]->keys.size() <=
                     (this->order / 2) &&
                 X->vnode[start_point_index + 1]->keys.size() <=
                     (this->order / 2)) {
        // merge case both siblings too small
        BNode *new_bn_node = new BNode();
        new_bn_node->keys.insert(new_bn_node->keys.end(),
                                 X->vnode[start_point_index - 1]->keys.begin(),
                                 X->vnode[start_point_index - 1]->keys.end());

        new_bn_node->keys.push_back(X->keys[start_point_index - 1]);

        new_bn_node->keys.insert(new_bn_node->keys.end(),
                                 X->vnode[start_point_index]->keys.begin(),
                                 X->vnode[start_point_index]->keys.end());

        new_bn_node->vnode.insert(
            new_bn_node->vnode.end(),
            X->vnode[start_point_index - 1]->vnode.begin(),
            X->vnode[start_point_index - 1]->vnode.end());

        new_bn_node->vnode.insert(new_bn_node->vnode.end(),
                                  X->vnode[start_point_index]->vnode.begin(),
                                  X->vnode[start_point_index]->vnode.end());

        X->keys.erase(X->keys.begin() + start_point_index - 1);
        X->vnode[start_point_index - 1] = new_bn_node;
        X->vnode.erase(X->vnode.begin() + start_point_index);

        new_bn_node->parent = X;
      }

      if (X->parent && X->parent->keys.size() == 0) {
        // case where we reduced the root ....
        tree = X->vnode[0];
        tree->parent = nullptr;
        return;
      }
      X = X->parent;
    }
  };
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
// MAIN
// ========================================

int main() {

  test_basic_insertions();

  test_ascending_insertions();

  test_descending_insertions();

  test_random_insertions();

  return 0;
}
