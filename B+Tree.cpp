#include <algorithm>
#include <iostream>
#include <iterator>

class BPlusTree {
public:
  int order = 4;
  typedef struct BPnode {
    bool isLeaf = false;
    std::vector<int> keys;
    std::vector<BPnode *> vnode;
    BPnode *linked_list_at_leaf = nullptr;
    BPnode *parent;
  } BPnode;

  BPnode *tree = nullptr;

  std::pair<int, BPnode *> search(BPnode *tree, int k,
                                  BPnode *parent = nullptr) {
    // search current node  and we stop at exactly at p ..
    if (tree == nullptr)
      return {-1, parent};

    int index_res = -1;
    for (int i = 0; i < tree->keys.size(); i++) {
      if (tree->isLeaf && tree->keys[i] == k) {
        return {k, tree};
      }
      if (tree->keys[i] < k) {
        index_res = i;
      }
      if (tree->keys[i] == k) {
        index_res += 1;
      }
    }
    // special case

    if (index_res == -1) {
      return search(tree->vnode[0], k, tree);

    } else if (index_res == tree->keys.size() - 1) {
      return search(tree->vnode[tree->vnode.size() - 1], k, tree);
    }
    return search(tree->vnode[index_res + 1], k, tree);
  }
  void insertion(int k) {
    // mainly we need handle attach directly case and attach at internal case
    // and lift up mechanism and also here at leaf we have special case of lift
    // up , in lift up case we need to check the tree    all the way up the root
    // until we are sure invariant is fine v.node.size()  == v.keys.size() + 1
    if (tree == nullptr) {
      BPnode *BP = new BPnode();
      BP->keys.push_back(k);
      BP->vnode.resize(2, nullptr);
      BP->parent = nullptr;
      BP->isLeaf = true;
      tree = BP;
      return;
    }
    auto search_pos = search(tree, k);
    // direct insertion is possibe then here ...
    if (search_pos.second->keys.size() + 1 <= order) {
      auto lb_pos = std::lower_bound(search_pos.second->keys.begin(),
                                     search_pos.second->keys.end(), k);
      auto dis_lb_pos = std::distance(search_pos.second->keys.begin(), lb_pos);
      search_pos.second->keys.insert(lb_pos, k);

      search_pos.second->vnode.insert(
          search_pos.second->vnode.begin() + dis_lb_pos, nullptr);
      std::cout << search_pos.second->keys.size() << search_pos.second->vnode.size() << std::endl;
    } else {
      // rebalance here , using while loop with above invariant ...
      auto X = search_pos.second; // this a start point the leaf here ...
      BPnode *right_child = nullptr;
      auto next_k = k;
      while (X != nullptr) {
        // stop if we find a place where we can insert out node without any
        // rebalance required at all
        if (X->keys.size() + 1 <= order) {
          auto lb_pos =
              std::lower_bound(X->keys.begin(), X->keys.end(), next_k);
          auto dis_lb_pos = std::distance(X->keys.begin(), lb_pos);
          X->keys.insert(lb_pos, next_k);
          if (X->isLeaf) {
            X->vnode.insert(X->vnode.begin() + dis_lb_pos + 1, nullptr);
          } else {
            X->vnode.insert(X->vnode.begin() + dis_lb_pos + 1, right_child);
          }
          return;
        }

        auto insert_it =
            std::lower_bound(X->keys.begin(), X->keys.end(), next_k);
        int insert_idx = std::distance(X->keys.begin(), insert_it);

        X->keys.insert(insert_it, next_k);
        std::cout << X->keys.size() << X->vnode.size() << std::endl;
        X->vnode.insert(X->vnode.begin() + insert_idx + 1, right_child);

        int median_index = X->keys.size() / 2;
        int median_value = X->keys[median_index];

        BPnode *right_node = new BPnode();

        right_node->isLeaf = X->isLeaf;

        right_node->parent = X->parent;

        if (X->isLeaf) {
          right_node->keys.assign(X->keys.begin() + median_index,
                                  X->keys.end());
          right_node->vnode.assign(X->vnode.begin() + median_index,
                                   X->vnode.end());
          right_node->linked_list_at_leaf = X->linked_list_at_leaf;
          X->linked_list_at_leaf = right_node;
          X->keys.erase(X->keys.begin() + median_index, X->keys.end());
          X->vnode.erase(
              X->vnode.begin() + median_index + 1,
              X->vnode.end()); // got destroyed with this line ... 

        } else {
          right_node->keys.assign(X->keys.begin() + median_index + 1,
                                  X->keys.end());
          right_node->vnode.assign(X->vnode.begin() + median_index + 1,
                                   X->vnode.end());
          X->keys.erase(X->keys.begin() + median_index, X->keys.end());
          X->vnode.erase(X->vnode.begin() + median_index + 1, X->vnode.end());
        }
        for (auto *child : right_node->vnode) {
          if (child)
            child->parent = right_node;
        }

        // if we have reached root and we found that we need still split meeds
        // to be handled
        if (X->parent == nullptr) {
          // root split occurs here ..
          BPnode *BP = new BPnode();
          BP->keys.push_back(median_value);
          BP->vnode.push_back(X);
          BP->vnode.push_back(right_node);
          BP->parent = nullptr;
          BP->isLeaf = false;
          X->parent = BP;
          right_node->parent = BP;
          tree = BP;
          return;
        }
        next_k = median_value;
        right_child = right_node;
        X = X->parent;
      }
    }
  };
  void deletion(int k);
};

#include <algorithm>
#include <cassert>
#include <iostream>
#include <queue>
#include <vector>

// ============================================================
// COLORS
// ============================================================

#define RESET "\033[0m"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"

// ============================================================
// TEST SUITE
// ============================================================

class BPlusTreeTests {

public:
  // ==========================================================
  // PRINT TREE
  // ==========================================================

  static void print_tree(BPlusTree &T);

  // ==========================================================
  // VALIDATORS
  // ==========================================================

  static void validate_all(BPlusTree &T);

  static void validate_sorted(BPlusTree::BPnode *node);

  static void validate_parent_links(BPlusTree::BPnode *node);

  static void validate_leaf_chain(BPlusTree::BPnode *root);

  static void validate_internal_invariant(BPlusTree::BPnode *node);

  // ==========================================================
  // TESTS
  // ==========================================================

  static void test_basic_insert();

  static void test_root_split();

  static void test_sequential_insert();

  static void test_reverse_insert();

  static void test_random_insert();

  static void test_duplicates();

  static void test_search_miss();

  static void test_heavy_split();

  static void run_all_tests();
};

// ============================================================
// PRINT TREE
// ============================================================

void BPlusTreeTests::print_tree(BPlusTree &T) {

  if (!T.tree) {

    std::cout << RED << "EMPTY TREE\n" << RESET;

    return;
  }

  std::queue<BPlusTree::BPnode *> q;

  q.push(T.tree);

  int level = 0;

  std::cout << CYAN << "\n================ TREE ================\n" << RESET;

  while (!q.empty()) {

    int sz = q.size();

    std::cout << YELLOW << "LEVEL " << level << " : " << RESET;

    while (sz--) {

      auto *cur = q.front();

      q.pop();

      if (cur->isLeaf)
        std::cout << GREEN;
      else
        std::cout << BLUE;

      std::cout << "[ ";

      for (auto x : cur->keys)
        std::cout << x << " ";

      std::cout << "] " << RESET;

      for (auto *child : cur->vnode) {

        if (child)
          q.push(child);
      }
    }

    std::cout << "\n";

    level++;
  }

  std::cout << CYAN << "======================================\n" << RESET;
}

// ============================================================
// VALIDATORS
// ============================================================

void BPlusTreeTests::validate_sorted(BPlusTree::BPnode *node) {

  if (!node)
    return;

  for (int i = 1; i < node->keys.size(); i++) {

    assert(node->keys[i - 1] <= node->keys[i]);
  }

  for (auto *child : node->vnode) {

    validate_sorted(child);
  }
}

void BPlusTreeTests::validate_parent_links(BPlusTree::BPnode *node) {

  if (!node)
    return;

  for (auto *child : node->vnode) {

    if (child) {

      assert(child->parent == node);

      validate_parent_links(child);
    }
  }
}

void BPlusTreeTests::validate_leaf_chain(BPlusTree::BPnode *root) {

  if (!root)
    return;

  auto *cur = root;

  while (!cur->isLeaf)
    cur = cur->vnode[0];

  int prev = -1e9;

  while (cur) {

    for (auto x : cur->keys) {

      assert(x >= prev);

      prev = x;
    }

    cur = cur->linked_list_at_leaf;
  }
}

void BPlusTreeTests::validate_internal_invariant(BPlusTree::BPnode *node) {

  if (!node)
    return;

  if (!node->isLeaf) {

    assert(node->vnode.size() == node->keys.size() + 1);
  }

  for (auto *child : node->vnode) {

    validate_internal_invariant(child);
  }
}

void BPlusTreeTests::validate_all(BPlusTree &T) {

  validate_sorted(T.tree);

  validate_parent_links(T.tree);

  validate_leaf_chain(T.tree);

  validate_internal_invariant(T.tree);
}

// ============================================================
// TEST : BASIC INSERT
// ============================================================

void BPlusTreeTests::test_basic_insert() {

  std::cout << MAGENTA << "\nRUNNING : test_basic_insert\n" << RESET;

  BPlusTree T;

  T.insertion(10);
  T.insertion(20);
  T.insertion(5);
  T.insertion(15);

  print_tree(T);

  assert(T.search(T.tree, 10).first == 10);

  assert(T.search(T.tree, 20).first == 20);

  assert(T.search(T.tree, 5).first == 5);

  assert(T.search(T.tree, 15).first == 15);

  validate_all(T);

  std::cout << GREEN << "PASSED : test_basic_insert\n" << RESET;
}

// ============================================================
// TEST : ROOT SPLIT
// ============================================================

void BPlusTreeTests::test_root_split() {

  std::cout << MAGENTA << "\nRUNNING : test_root_split\n" << RESET;

  BPlusTree T;

  for (int x : {10, 20, 30, 40, 50}) {

    T.insertion(x);
  }

  print_tree(T);

  assert(T.tree != nullptr);

  assert(T.tree->isLeaf == false);

  validate_all(T);

  std::cout << GREEN << "PASSED : test_root_split\n" << RESET;
}

// ============================================================
// TEST : SEQUENTIAL INSERT
// ============================================================

void BPlusTreeTests::test_sequential_insert() {

  std::cout << MAGENTA << "\nRUNNING : test_sequential_insert\n" << RESET;

  BPlusTree T;

  for (int i = 1; i <= 100; i++) {

    std::cout << "insertion of" << i << std::endl;
    T.insertion(i);
    print_tree(T);
  }

  print_tree(T);

  for (int i = 1; i <= 100; i++) {

    assert(T.search(T.tree, i).first == i);
  }

  validate_all(T);

  std::cout << GREEN << "PASSED : test_sequential_insert\n" << RESET;
}

// ============================================================
// TEST : REVERSE INSERT
// ============================================================

void BPlusTreeTests::test_reverse_insert() {

  std::cout << MAGENTA << "\nRUNNING : test_reverse_insert\n" << RESET;

  BPlusTree T;

  for (int i = 100; i >= 1; i--) {

    T.insertion(i);
  }

  print_tree(T);

  for (int i = 1; i <= 100; i++) {

    assert(T.search(T.tree, i).first == i);
  }

  validate_all(T);

  std::cout << GREEN << "PASSED : test_reverse_insert\n" << RESET;
}

// ============================================================
// TEST : RANDOM INSERT
// ============================================================

void BPlusTreeTests::test_random_insert() {

  std::cout << MAGENTA << "\nRUNNING : test_random_insert\n" << RESET;

  BPlusTree T;

  std::vector<int> vals = {42, 7,  99, 12, 53, 61, 2,  88,
                           17, 31, 73, 25, 64, 1,  90, 45};

  for (auto x : vals) {
    std::cout << "inserte.." << x << std::endl;
    T.insertion(x);
    print_tree(T);
  }
  print_tree(T);

  for (auto x : vals)
    assert(T.search(T.tree, x).first == x);

  validate_all(T);

  std::cout << GREEN << "PASSED : test_random_insert\n" << RESET;
}

// ============================================================
// RUN ALL
// ============================================================

void BPlusTreeTests::run_all_tests() {

  test_basic_insert();

  test_root_split();

  test_sequential_insert();

  test_reverse_insert();

  test_random_insert();

  std::cout << CYAN << "\n========================================\n"
            << " ALL TESTS PASSED SUCCESSFULLY\n"
            << "========================================\n"
            << RESET;
}

// ============================================================
// MAIN
// ============================================================

int main() {

  BPlusTreeTests::run_all_tests();

  return 0;
}
