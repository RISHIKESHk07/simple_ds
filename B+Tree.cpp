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
      std::cout << search_pos.second->keys.size()
                << search_pos.second->vnode.size() << std::endl;
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
          X->vnode.erase(X->vnode.begin() + median_index + 1,
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
  void deletion(int k) {
    // inorder to delete start from the actual position of deletion based on
    // sucessor value that we need to look for and then replace it with succesor
    // node  value
    // searching for pos we want ...

    auto pos_search = search(tree, k);
    if (pos_search.first == -1)
      return;
    // for leaf nodes always , direct removal first ...
    auto n_pos = pos_search.second;
    for (int i = 0; i < n_pos->keys.size(); i++) {
      if (n_pos->keys[i] == k) {
        n_pos->keys.erase(n_pos->keys.begin() + i);
        n_pos->vnode.erase(n_pos->vnode.begin() + i);
        break;
      }
    }
    // make sure we change all the parent copies ...
    BPnode *cur = n_pos;

    while (cur->parent) {

      BPnode *par = cur->parent;

      int child_idx = 0;

      while (child_idx < par->vnode.size() && par->vnode[child_idx] != cur)
        child_idx++;

      if (child_idx > 0 && !cur->keys.empty())
        par->keys[child_idx - 1] = cur->keys[0];

      cur = par;
    }

    // rebalance part in case of fault
    auto X = n_pos;
    int min_keys = ((order + 1) / 2);
    while (X != nullptr) {

      // root  case where we only have single node
      if (X == tree) {

        if (X->isLeaf) {

          if (X->keys.empty()) {
            delete X;
            tree = nullptr;
          }

          return;
        }

        if (X->keys.empty()) {

          tree = X->vnode[0];

          if (tree)
            tree->parent = nullptr;

          delete X;
        }

        return;
      }

      if (X->keys.size() >= min_keys) {
        return;
      }

      // check if any copies left to update from ...

      BPnode *P = X->parent;
      int cur_pos_index = 0;

      while (cur_pos_index < P->vnode.size() && P->vnode[cur_pos_index] != X) {
        cur_pos_index++;
      }
      auto l = (cur_pos_index - 1 >= 0) ? P->vnode[cur_pos_index - 1] : nullptr;
      auto r = (cur_pos_index + 1 < P->vnode.size())
                   ? P->vnode[cur_pos_index + 1]
                   : nullptr;

      // right rotate for right sibling greater , break after
      if (l && l->keys.size() > min_keys) {
        auto sep = P->keys[cur_pos_index - 1];
        auto brower_key = l->keys.back();
        auto brower_child = l->vnode.back();
        if (X->isLeaf) {
          X->keys.insert(X->keys.begin(), brower_key);
          X->vnode.insert(X->vnode.begin(), nullptr);
          l->keys.pop_back();
          P->keys[cur_pos_index - 1] = X->keys[0];
        } else {
          l->keys.pop_back();
          l->vnode.pop_back();

          P->keys[cur_pos_index - 1] = brower_key;

          X->keys.insert(X->keys.begin(), sep);

          X->vnode.insert(X->vnode.begin(), brower_child);
          brower_child->parent = X;
        }
        break;
      }
      // left rotate for left sibling greater , break after
      else if (r && r->keys.size() > min_keys) {

        auto sep = P->keys[cur_pos_index];
        auto brower_key = r->keys.front();
        auto brower_child = r->vnode.front();

        if (X->isLeaf) {
          X->keys.push_back(brower_key);
          X->vnode.push_back(nullptr);
          P->keys[cur_pos_index] = r->keys[0];
          r->keys.erase(r->keys.begin());
        } else {
          r->keys.erase(r->keys.begin());
          r->vnode.erase(r->vnode.begin());
          P->keys[cur_pos_index] = brower_key;
          X->keys.push_back(sep);
          X->vnode.push_back(brower_child);
          brower_child->parent = X;
        }
        break;
      }
      // merge with parent and siblings when all nodes small , still go up
      else {
        BPnode *left = nullptr;
        BPnode *right = nullptr;
        int start_point_index = cur_pos_index;
        int sep_index = -1;
        bool m_isLeaf = false;

        if (l) {
          left = P->vnode[start_point_index - 1];
          right = P->vnode[start_point_index];
          sep_index = start_point_index - 1;
          if (X->isLeaf) {
            left->keys.insert(left->keys.end(), X->keys.begin(), X->keys.end());
            left->vnode.insert(left->vnode.end(), X->vnode.begin(),
                               X->vnode.end());
            left->linked_list_at_leaf = X->linked_list_at_leaf;
          } else {
            left->keys.push_back(P->keys[sep_index]);
            left->keys.insert(left->keys.end(), X->keys.begin(), X->keys.end());
            left->vnode.insert(left->vnode.end(), X->vnode.begin(),
                               X->vnode.end());
            for (auto child : X->vnode) {
              if (child) {
                child->parent = left;
              }
            }
          }
          P->keys.erase(P->keys.begin() + sep_index);
          P->vnode.erase(P->vnode.begin() + sep_index + 1);

        } else {
          left = P->vnode[start_point_index];
          right = P->vnode[start_point_index + 1];
          sep_index = start_point_index;

          if (X->isLeaf) {
            X->keys.insert(X->keys.end(), right->keys.begin(),
                           right->keys.end());
            X->vnode.insert(X->vnode.end(), right->vnode.begin(),
                            right->vnode.end());
            X->linked_list_at_leaf = right->linked_list_at_leaf;
          } else {
            X->keys.push_back(P->keys[sep_index]);
            X->keys.insert(X->keys.end(), right->keys.begin(),
                           right->keys.end());
            X->vnode.insert(X->vnode.end(), right->vnode.begin(),
                            right->vnode.end());
            for (auto child : right->vnode) {
              if (child) {
                child->parent = X;
              }
            }
          }
          P->keys.erase(P->keys.begin() + sep_index);
          P->vnode.erase(P->vnode.begin() + sep_index + 1);
        }

        X = P;
      }
    }
  }

  std::vector<int> range_lookup(std::pair<int, int> range) {
    auto pos = search(tree, range.first);
    auto X = pos.second;
    std::vector<int> py;
    while (X && X->keys[0] <= range.second) {
      for (int i = 0; i < X->keys.size(); i++) {
        if (X->keys[i] > range.second)
          break;
        else {
          py.push_back(X->keys[i]);
        }
      }
      X = X->linked_list_at_leaf;
    }
    return py;
  }
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

  static void test_delete_simple();

  static void test_delete_leaf_no_rebalance();

  static void test_delete_many();

  static void test_delete_all();

  static void test_delete_reverse();

  static void test_delete_random();

  static void test_delete_root_shrink();

  static void test_range_lookup();
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

void BPlusTreeTests::test_delete_simple() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_simple\n" << RESET;

  BPlusTree T;

  for (int x : {10, 20, 30, 40, 50})
    T.insertion(x);

  print_tree(T);

  T.deletion(30);

  print_tree(T);

  assert(T.search(T.tree, 30).first == -1);

  assert(T.search(T.tree, 10).first == 10);
  assert(T.search(T.tree, 20).first == 20);
  assert(T.search(T.tree, 40).first == 40);
  assert(T.search(T.tree, 50).first == 50);

  validate_all(T);

  std::cout << GREEN << "PASSED : test_delete_simple\n" << RESET;
}

void BPlusTreeTests::test_delete_leaf_no_rebalance() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_leaf_no_rebalance\n"
            << RESET;

  BPlusTree T;

  for (int i = 1; i <= 15; i++)
    T.insertion(i);

  print_tree(T);

  for (int x : {3, 5, 7, 9}) {
    std::cout << "deleted .. " << x << std::endl;
    T.deletion(x);

    assert(T.search(T.tree, x).first == -1);

    validate_all(T);
  }

  print_tree(T);

  std::cout << GREEN << "PASSED : test_delete_leaf_no_rebalance\n" << RESET;
}

void BPlusTreeTests::test_delete_many() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_many\n" << RESET;

  BPlusTree T;

  for (int i = 1; i <= 10; i++)
    T.insertion(i);

  print_tree(T);

  for (int i = 1; i <= 10; i++) {

    std::cout << "delete .." << i << std::endl;

    T.deletion(i);

    print_tree(T);

    assert(T.search(T.tree, i).first == -1);

    validate_all(T);
  }
  print_tree(T);

  std::cout << GREEN << "PASSED : test_delete_many\n" << RESET;
}

void BPlusTreeTests::test_delete_reverse() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_reverse\n" << RESET;

  BPlusTree T;

  for (int i = 1; i <= 50; i++)
    T.insertion(i);

  for (int i = 50; i >= 1; i--) {

    T.deletion(i);

    assert(T.search(T.tree, i).first == -1);

    validate_all(T);
  }
  print_tree(T);

  std::cout << GREEN << "PASSED : test_delete_reverse\n" << RESET;
}

void BPlusTreeTests::test_delete_random() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_random\n" << RESET;

  BPlusTree T;

  std::vector<int> vals;

  for (int i = 1; i <= 100; i++) {

    vals.push_back(i);

    T.insertion(i);
  }
  print_tree(T);

  // random_shuffle(vals.begin(), vals.end());

  for (auto x : vals) {
    std::cout << x << std::endl;
    T.deletion(x);

    assert(T.search(T.tree, x).first == -1);

    validate_all(T);
  }
  print_tree(T);

  std::cout << GREEN << "PASSED : test_delete_random\n" << RESET;
}
void BPlusTreeTests::test_delete_root_shrink() {

  std::cout << MAGENTA << "\nRUNNING : test_delete_root_shrink\n" << RESET;

  BPlusTree T;

  for (int i = 1; i <= 20; i++)
    T.insertion(i);

  for (int i = 1; i <= 20; i++) {

    T.deletion(i);

    validate_all(T);
  }

  print_tree(T);

  assert(T.search(T.tree, 20).first == 20);

  validate_all(T);

  std::cout << GREEN << "PASSED : test_delete_root_shrink\n" << RESET;
}

void BPlusTreeTests::test_range_lookup() {

  BPlusTree T;

  for (int i = 1; i <= 20; i++)
    T.insertion(i);

  for (int i = 1; i <= 10; i++) {

    T.deletion(i);

    validate_all(T);
  }

  print_tree(T);

  assert(T.search(T.tree, 20).first == 20);
  assert(T.search(T.tree, 10).first == -1);
  validate_all(T);

  auto vec =
      T.range_lookup({12, 21}); // 21 does not exist here so we should not break
                                // but give the last value possible ..

  for (auto x : vec) {
    std::cout << YELLOW << x << "--";
  }
  std::cout << RESET;

  std::cout << GREEN << "PASSED : test range lookup\n" << RESET;
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

  test_delete_simple();

  test_delete_leaf_no_rebalance();

  test_delete_many();

  test_delete_reverse();

  test_delete_random();

  test_delete_root_shrink();

  test_range_lookup();

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
