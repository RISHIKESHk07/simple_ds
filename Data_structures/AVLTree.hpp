#include <cstddef>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <ostream>
#include <utility>
// rewrite the removal with spawing idea over naive idea i used ...
class BST {

public:
  typedef struct node {
    int key = -1;
    struct node *left = nullptr;
    struct node *right = nullptr;
  } node;

  node *tree;

  BST() {
    tree = new node();
    insert_node(44);
    insert_node(17);
    insert_node(88);
    insert_node(32);
    insert_node(65);
    insert_node(97);
    insert_node(28);
    insert_node(54);
    insert_node(82);
    insert_node(29);
    insert_node(76);
    insert_node(80);
    insert_node(78);

    removal_node(32);
    printTree();
  };
  std::vector<std::pair<node *, node *>>
  searchTree_node(node *tree_curr, int k,
                  std::vector<std::pair<node *, node *>> &vec,
                  node *parent_curr = nullptr) {
    if (tree_curr == nullptr) {
      std::cout << "not found" << std::endl;
      return (vec.size() > 0 ? vec : (vec = {{tree_curr, parent_curr}}));
    }
    if (tree_curr->key >= k) {
      std::cout << " search l" << std::endl;
      if (tree_curr->key == k) {
        vec.push_back({tree_curr, parent_curr});
        return vec;
      }
      return searchTree_node(tree_curr->left, k, vec, tree_curr);
    } else {
      std::cout << "search r" << std::endl;
      return searchTree_node(tree_curr->right, k, vec, tree_curr);
    }
    return {};
  };
  node *searchTree_nextSpot(node *tree_curr, int k,
                            node *parent_curr = nullptr) {
    if (tree_curr == nullptr) {
      std::cout << "stopping at empty spot" << std::endl;
      return parent_curr;
    }
    if (tree_curr->key >= k) {
      std::cout << "l search empty spot" << std::endl;
      return searchTree_nextSpot(tree_curr->left, k, tree_curr);
    } else {
      std::cout << "r search empty spot" << std::endl;
      return searchTree_nextSpot(tree_curr->right, k, tree_curr);
    }
  };
  void insert_node(int k) {

    if (tree->key == -1) {
      tree->key = k;
      return;
    }

    auto pos = searchTree_nextSpot(tree, k);
    node *new_node = new node();
    new_node->key = k;

    if (pos->key >= k) {
      pos->left = new_node;
    } else {
      pos->right = new_node;
    }
  };
  void removal_node(int rem) {
    std::vector<std::pair<node *, node *>> vec;
    auto pos = searchTree_node(tree, rem, vec);
    if (pos[0].first == nullptr)
      pos.clear();
    if (pos.size() != 0) {

      if (pos[0].first->right != nullptr) {
        std::vector<std::pair<node *, node *>> vec3;
        auto pos2 = searchTree_node(pos[0].first->right, rem, vec3);
        std::cout << "--" << pos2[0].second->key << std::endl;

        if (pos2[0].second->right == nullptr &&
            pos2[0].second->left == nullptr) {
          std::vector<std::pair<node *, node *>> vec2;
          auto pos3 = searchTree_node(tree, pos2[0].second->key, vec2);
          std::cout << pos3[0].second->key << std::endl;
          if (pos3[0].second->key >= pos3[0].first->key) {
            pos3[0].second->left = nullptr;
          } else {
            pos3[0].second->right = nullptr;
          }
          int temp = pos2[0].second->key;
          pos[0].first->key = temp;
          delete (pos3[0].first);

        } else {

          std::vector<std::pair<node *, node *>> vec2;
          auto pos3 = searchTree_node(tree, pos2[0].second->key, vec2);
          if (pos3[0].second->left->key == pos3[0].first->key) {

            pos3[0].second->left = pos3[0].first->right;
            pos3[0].first->right = nullptr;
          }
          int temp = pos2[0].second->key;
          pos[0].first->key = temp;
          delete (pos3[0].first);
        }
      } else if (pos[0].first->left != nullptr) {
        std::vector<std::pair<node *, node *>> vec3;
        auto pos2 = searchTree_node(pos[0].first->left, rem, vec3);
        std::cout << "left-max--" << pos2[0].second->key << std::endl;

        if (pos2[0].second->right == nullptr &&
            pos2[0].second->left == nullptr) {
          std::vector<std::pair<node *, node *>> vec2;
          auto pos3 = searchTree_node(tree, pos2[0].second->key, vec2);
          std::cout << pos3[0].second->key << std::endl;
          if (pos3[0].second->key >= pos3[0].first->key) {
            pos3[0].second->left = nullptr;
          } else {
            pos3[0].second->right = nullptr;
          }
          int temp = pos2[0].second->key;
          pos[0].first->key = temp;
          delete (pos3[0].first);

        } else {

          std::vector<std::pair<node *, node *>> vec2;
          auto pos3 = searchTree_node(tree, pos2[0].second->key, vec2);
          if (pos3[0].second->left->key == pos3[0].first->key) {

            pos3[0].second->left = pos3[0].first->right;
            pos3[0].first->right = nullptr;
          }
          int temp = pos2[0].second->key;
          pos[0].first->key = temp;
          delete (pos3[0].first);
        }

      } else {
        if (pos[0].second->key >= pos[0].first->key) {
          pos[0].second->left = nullptr;
        } else {
          pos[0].second->right = nullptr;
        }
      }

    } else {
      std::cout << "Element does not exit" << std::endl;
    }
  };
  void printTree() {

    std::deque<node> dq;
    auto temp_tree = tree;
    dq.push_back(*temp_tree);
    int depth = 0;
    while (!dq.empty() && depth < 7) {
      std::cout << " --- depth:" << depth << " ---" << std::endl;

      int len_curr_dq = dq.size();
      std::cout << dq.size() << std::endl;
      for (int i = 0; i < len_curr_dq; i++) {
        auto curr = dq.front();
        dq.pop_front();
        if (curr.key > 0)
          std::cout << curr.key;
        else
          std::cout << "null";
        if (curr.left != nullptr)
          dq.push_back(*(curr.left));
        else {
          // node *null_node = new node();
          // dq.push_back(*null_node);
        }
        if (curr.right != nullptr)
          dq.push_back(*curr.right);
        else {
          // node *null_node = new node();
          // dq.push_back(*null_node);
        }
      }

      std::cout << std::endl;
      depth++;
      std::cout << "-------" << std::endl;
    }
  };
};

#include "../v1/IN-MEM_DB_key_spec.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <random>

template <typename KeyType, typename ValueType, typename Comparator>
class AVLtree {
  Comparator cmp;
  /*
    AVL TREE
    --------
    Self balancing BST where:
      BF = height(right) - height(left)

    Valid BF:
      -1, 0, +1

    Rotations:
      LL -> right rotate
      RR -> left rotate
      LR -> left-right rotate
      RL -> right-left rotate
  */

  typedef struct node {
    KeyType key;
    ValueType val;
    node *left;
    node *right;
    node(KeyType &k, ValueType &v) : key(k), val(v) {}
  } node;

public:
  node *tree;

private:
  std::map<node *, int> heightb;

public:
  AVLtree() { tree = nullptr; }

  // ============================================================
  // SEARCH
  // ============================================================

  std::pair<node *, node *> search_internal(KeyType k, node *tree,
                                            node *current_ptr = nullptr,
                                            node *parent_ptr = nullptr) {

    if (tree == nullptr)
      return {current_ptr, parent_ptr};

    if (cmp(tree->key, k) == 0)
      return {current_ptr, parent_ptr};

    else if (cmp(tree->key, k) == -1)
      return search_internal(k, tree->left, tree->left, tree);

    else
      return search_internal(k, tree->right, tree->right, tree);
  }

  std::pair<node *, node *> search(KeyType k, node *tree_acc) {
    return search_internal(k, tree_acc, tree, nullptr);
  }

  // ============================================================
  // ROTATIONS
  // ============================================================

  node *left_rotate(node *f, node *s) {

    f->right = s->left;
    s->left = f;

    if (heightb[s] == 0) {
      heightb[f] = 1;
      heightb[s] = -1;
    } else {
      heightb[f] = 0;
      heightb[s] = 0;
    }

    return s;
  }

  node *right_rotate(node *f, node *s) {

    f->left = s->right;
    s->right = f;

    if (heightb[s] == 0) {
      heightb[f] = -1;
      heightb[s] = 1;
    } else {
      heightb[f] = 0;
      heightb[s] = 0;
    }

    return s;
  }

  node *left_right_rotate(node *f, node *s) {

    auto Y = heightb[s->left];

    auto G = right_rotate(s, s->left);

    f->right = G;

    auto G2 = left_rotate(f, f->right);

    if (Y == 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 0;
    } else if (Y < 0) {
      heightb[G2->right] = 1;
      heightb[G2->left] = 0;
    } else {
      heightb[G2->right] = 0;
      heightb[G2->left] = -1;
    }

    heightb[G2] = 0;

    return G2;
  }

  node *right_left_rotate(node *f, node *s) {

    auto Y = heightb[s->right];

    auto G = left_rotate(s, s->right);

    f->left = G;

    auto G2 = right_rotate(f, f->left);

    if (Y == 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 0;
    } else if (Y < 0) {
      heightb[G2->right] = 1;
      heightb[G2->left] = 0;
    } else {
      heightb[G2->right] = 0;
      heightb[G2->left] = -1;
    }

    heightb[G2] = 0;

    return G2;
  }

  // ============================================================
  // INSERTION
  // ============================================================

  void insertion(KeyType k, ValueType v) {

    auto pos = search(k, tree);

    // EMPTY TREE

    if (pos.second == nullptr) {

      node *temp = new node(k, v);
      temp->left = nullptr;
      temp->right = nullptr;

      tree = temp;

      heightb[temp] = 0;

      return;
    }

    node *new_incoming = new node(k, v);

    new_incoming->left = nullptr;
    new_incoming->right = nullptr;

    if (cmp(pos.second->key, k) == -1)
      pos.second->right = new_incoming;
    else
      pos.second->left = new_incoming;

    heightb[new_incoming] = 0;

    auto Z = new_incoming;

    for (auto X = pos.second; X != nullptr;) {

      auto pos3 = search(X->key, tree);

      node *G = nullptr;
      node *N = nullptr;
      bool was_left_child =
          (pos3.second != nullptr && pos3.second->left == pos3.first);
      // RIGHT SIDE INSERTION

      if (X->right && cmp(X->right->key, Z->key) == 0) {

        if (heightb[X] > 0) {

          G = pos3.second;

          if (heightb[Z] >= 0)
            N = left_rotate(X, Z);
          else
            N = left_right_rotate(X, Z);

        } else {

          if (heightb[X] < 0) {
            heightb[X] = 0;
            break;
          }

          heightb[X] = 1;

          Z = X;
          X = pos3.second;

          continue;
        }
      }

      // LEFT SIDE INSERTION

      else if (X->left && cmp(X->left->key, Z->key) == 0) {

        if (heightb[X] < 0) {

          G = pos3.second;

          if (heightb[Z] <= 0)
            N = right_rotate(X, Z);
          else
            N = right_left_rotate(X, Z);

        } else {

          if (heightb[X] > 0) {
            heightb[X] = 0;
            break;
          }

          heightb[X] = -1;

          Z = X;
          X = pos3.second;

          continue;
        }
      }

      // ATTACH BACK

      if (G == nullptr)
        tree = N;

      else if (was_left_child)
        G->left = N;

      else
        G->right = N;

      break;
    }
  }

  // ============================================================
  // BST SUCCESSOR
  // ============================================================

  node *bst_succesor(node *x) {

    if (x->right != nullptr) {

      auto y = x->right;

      while (y->left != nullptr)
        y = y->left;

      return y;
    }

    auto x_pos = search(x->key, tree);

    auto y2 = x_pos.second;
    auto x2 = x_pos.first;

    while (y2 != nullptr && x2 == y2->right) {

      x2 = y2;

      auto y2_pos = search(y2->key, tree);

      y2 = y2_pos.second;
    }

    return y2;
  }

  // ============================================================
  // SHIFT NODES
  // ============================================================

  void shift_nodes(node *u, node *v) {

    auto u_pos = search(u->key, tree);

    if (u_pos.second == nullptr) {

      tree = v;

    } else if (u_pos.second->left == u_pos.first) {

      u_pos.second->left = v;

    } else if (u_pos.second->right == u_pos.first) {

      u_pos.second->right = v;
    }
  }

  // ============================================================
  // BST DELETE
  // ============================================================

  // returns:
  // first  -> rebalance start node
  // second -> subtree that became shorter
  std::pair<node *, node *> bst_delete(KeyType k) {

    auto u_pos = search(k, tree);

    if (u_pos.first == nullptr)
      return {nullptr, nullptr};

    node *rebalance_start = nullptr;
    node *shrunk_subtree = nullptr;

    // --------------------------------------------------------
    // CASE 1 : NO LEFT CHILD
    // --------------------------------------------------------

    if (u_pos.first->left == nullptr) {

      rebalance_start = u_pos.second;

      shrunk_subtree = (u_pos.second && u_pos.second->left == u_pos.first)
                           ? u_pos.second->left
                       : u_pos.second ? u_pos.second->right
                                      : nullptr;

      shift_nodes(u_pos.first, u_pos.first->right);

      return {rebalance_start, shrunk_subtree};
    }

    // --------------------------------------------------------
    // CASE 2 : NO RIGHT CHILD
    // --------------------------------------------------------

    else if (u_pos.first->right == nullptr) {

      rebalance_start = u_pos.second;

      shrunk_subtree = (u_pos.second && u_pos.second->left == u_pos.first)
                           ? u_pos.second->left
                       : u_pos.second ? u_pos.second->right
                                      : nullptr;

      shift_nodes(u_pos.first, u_pos.first->left);

      return {rebalance_start, shrunk_subtree};
    }

    // --------------------------------------------------------
    // CASE 3 : TWO CHILDREN
    // --------------------------------------------------------

    else {

      auto y = bst_succesor(u_pos.first);

      auto y_pos = search(y->key, tree);

      // ----------------------------------------------------
      // successor not direct child
      // ----------------------------------------------------

      if (y_pos.second != u_pos.first) {

        // THIS is where subtree height first changes
        rebalance_start = y_pos.second;

        // successor removed from LEFT side of parent
        shrunk_subtree = y_pos.second->left;

        shift_nodes(y_pos.first, y_pos.first->right);

        y->right = u_pos.first->right;
      }

      // ----------------------------------------------------
      // successor direct child
      // ----------------------------------------------------

      else {

        rebalance_start = y;

        shrunk_subtree = y->right;
      }

      shift_nodes(u_pos.first, y);

      y->left = u_pos.first->left;

      return {rebalance_start, shrunk_subtree};
    }
  }

  // ============================================================
  // DELETE
  // ============================================================

  void deletion(KeyType k) {

    auto del_info = bst_delete(k);

    auto start = del_info.first;
    auto N = del_info.second;

    if (start == nullptr)
      return;

    for (auto X = start; X != nullptr;) {

      auto pos3 = search(X->key, tree);

      node *G = nullptr;

      int b = 0;

      bool was_left_child =
          (pos3.second != nullptr && pos3.second->left == pos3.first);

      // ====================================================
      // LEFT SIDE SHRUNK
      // ====================================================

      if (X->left == N) {

        if (heightb[X] > 0) {

          G = pos3.second;

          b = heightb[X->right];

          if (heightb[X->right] < 0)
            N = left_right_rotate(X, X->right);
          else
            N = left_rotate(X, X->right);

        } else {

          if (heightb[X] == 0) {

            heightb[X] = 1;
            break;
          }

          heightb[X] = 0;

          N = X;

          auto pos4 = search(N->key, tree);

          X = pos4.second;

          continue;
        }
      }

      // ====================================================
      // RIGHT SIDE SHRUNK
      // ====================================================

      else if (X->right == N) {

        if (heightb[X] < 0) {

          G = pos3.second;

          b = heightb[X->left];

          if (heightb[X->left] <= 0)
            N = right_left_rotate(X, X->left);
          else
            N = right_rotate(X, X->left);

        } else {

          if (heightb[X] == 0) {

            heightb[X] = -1;
            break;
          }

          heightb[X] = 0;

          N = X;

          auto pos4 = search(N->key, tree);

          X = pos4.second;

          continue;
        }
      }

      // ====================================================
      // REATTACH
      // ====================================================

      if (G == nullptr)
        tree = N;

      else if (was_left_child)
        G->left = N;

      else
        G->right = N;

      // height stopped changing
      if (b == 0)
        break;

      auto pos5 = search(N->key, tree);

      X = pos5.second;
    }
  }
  int heightOp(node *n) {

    if (n == nullptr)
      return 0;

    return std::max(heightOp(n->left), heightOp(n->right)) + 1;
  }

  // ============================================================
  // RECOMPUTE ALL BALANCE FACTORS
  // BF = RIGHT HEIGHT - LEFT HEIGHT
  // ============================================================

  void recomputeBF(node *root) {

    if (root == nullptr)
      return;

    recomputeBF(root->left);
    recomputeBF(root->right);

    heightb[root] = heightOp(root->right) - heightOp(root->left);
    std::cout << "RECOMPUTE " << root->key << " -> " << heightb[root]
              << std::endl;
  }

  // ============================================================
  // RIGHT AVL JOIN
  // ============================================================

  node *RightAVl(node *LT, node *RT, KeyType k, ValueType v) {

    auto LT_subl = LT->left;
    auto C = LT->right;
    auto k_ = LT->key;
    auto v_ = LT->val;

    if (heightOp(C) <= heightOp(RT) + 1) {

      auto T__ = new node(k, v);

      T__->left = C;
      T__->right = RT;

      if (heightOp(LT_subl) + 1 >= heightOp(T__)) {

        node *m_node = new node(k_, v_);

        m_node->left = LT_subl;
        m_node->right = T__;

        return m_node;

      } else {

        node *n_node = new node(k_, v_);

        n_node->left = LT_subl;
        n_node->right = T__;

        return left_right_rotate(n_node, n_node->right);
      }
    }

    auto T_ = RightAVl(C, RT, k, v);

    node *m_node = new node(k_, v_);

    m_node->left = LT_subl;
    m_node->right = T_;

    if (heightOp(LT->left) <= heightOp(T_) + 1) {

      return m_node;

    } else {

      return left_rotate(m_node, m_node->right);
    }
  }

  // ============================================================
  // LEFT AVL JOIN
  // ============================================================

  node *LeftAVL(node *LT, node *RT, KeyType k, ValueType v) {

    auto RT_subr = RT->right;
    auto C = RT->left;
    auto k_ = RT->key;
    auto v_ = RT->key;

    if (heightOp(C) <= heightOp(LT) + 1) {

      auto T__ = new node(k, v);

      T__->left = LT;
      T__->right = C;

      node *m_node = new node(k_, v_);

      m_node->left = T__;
      m_node->right = RT_subr;

      if (heightOp(RT_subr) + 1 >= heightOp(T__)) {

        return m_node;

      } else {

        return right_left_rotate(m_node, m_node->left);
      }
    }

    auto T_ = LeftAVL(LT, C, k, v);

    node *mn_node = new node(k_, v_);

    mn_node->left = T_;
    mn_node->right = RT_subr;

    if (heightOp(RT_subr) + 1 >= heightOp(T_))
      return mn_node;
    else
      return right_rotate(mn_node, mn_node->left);
  }

  // ============================================================
  // JOIN OPERATION
  // ============================================================

  node *JoingOp(node *LT, node *RT, KeyType k, ValueType v) {

    node *result = nullptr;

    if (heightOp(LT) == heightOp(RT)) {

      node *new_tree = new node(k, v);

      new_tree->left = LT;
      new_tree->right = RT;

      result = new_tree;
    }

    else if (heightOp(LT) > heightOp(RT)) {

      result = RightAVl(LT, RT, k, v);

    } else {

      result = LeftAVL(LT, RT, k, v);
    }

    // ========================================================
    // FIX ALL BALANCE FACTORS ONCE AT END
    // ========================================================

    recomputeBF(result);

    return result;
  }

  std::pair<node *, node *> SplitTrees(node *t, KeyType k) {
    if (t == nullptr)
      return {nullptr, nullptr};
    if (cmp(t->key, k) == 0) {

      auto L = t->left;
      auto R = t->right;

      t->left = nullptr;
      t->right = nullptr;

      recomputeBF(L);
      recomputeBF(R);

      return {L, R};
    } else if (cmp(t->key, k) == -1) {
      auto temp = SplitTrees(t->left, k);
      return {temp.first, JoingOp(temp.second, t->right, t->key, t->val)};
    } else {
      auto temp = SplitTrees(t->right, k);
      return {JoingOp(t->left, temp.first, t->key, t->val),
              temp.second}; // here as we go up you see that t->left denotes
                            // values smaller as we build from bottom to up
    }
  }

  node *UnionTrees(node *t1, node *t2) {
    if (t1 == nullptr)
      return t2;
    else if (t2 == nullptr)
      return t1;
    else {
      auto temp = SplitTrees(t2, t1->key);
      return JoingOp(UnionTrees(temp.first, t1->left),
                     UnionTrees(temp.second, t1->right), t1->key, t1->val);
    }
  }
  std::vector<node *> flatten_tree(node *tree, std::vector<node *> res) {
    if (tree == nullptr) {
      return;
    } else {
      flatten_tree(tree->left);
      res.push_back(tree);
      flatten_tree(tree->right);
    }
  }

  std::vector<std::pair<std::string, std::string>> inorder_full_traversal() {
    std::vector<node *> vec;
    std::vector<std::pair<std::string, std::string>> res;
    auto v = flatten_tree(tree, vec);
    for (auto i : v) {
      res.push_back({i.key, i.val});
    }
    return res;
  }

  std::vector<std::pair<std::string, std::string>>
  inorder_range_traversal(KeyType k1, KeyType k2) {
    auto n1 = search(k1, tree);
    auto n2 = search(k2, tree);
    if (n1.first == nullptr || n2.first == nullptr) {
    }
    std::vector<std::pair<std::string, std::string>> res;

    while (n1 != n2) {
      res.push_back({n1.first.key, n1.first.val});
      n1 = bst_succesor(n1);
    }

    res.push_back({n2.first.key, n2.first.val});

    return res;
  }

  int size_of_tree() {
    std::vector<node *> in_order;
    flatten_tree(tree, in_order);
    if (in_order.size() == 0)
      return -1;
    return in_order.size();
  };

  // ============================================================
  // TREE PRINTER
  // ============================================================

private:
  void printTreeInternal(node *root, std::string indent, bool last) {

    if (root == nullptr) {

      std::cout << indent;

      if (last)
        std::cout << "└── ";
      else
        std::cout << "├── ";

      std::cout << "NULL" << std::endl;

      return;
    }

    std::cout << indent;

    if (last) {
      std::cout << "└── ";
      indent += "    ";
    } else {
      std::cout << "├── ";
      indent += "│   ";
    }

    std::cout << root->key << " [BF=" << heightb[root] << "]" << std::endl;

    printTreeInternal(root->left, indent, false);
    printTreeInternal(root->right, indent, true);
  }

public:
  void printTree() {

    std::cout << "\n========== AVL TREE ==========\n";

    if (tree == nullptr) {
      std::cout << "EMPTY TREE\n";
      return;
    }

    printTreeInternal(tree, "", true);

    std::cout << "==============================\n";
  }

  // ============================================================
  // AVL VALIDATION
  // ============================================================

private:
  int realHeight(node *root) {

    if (root == nullptr)
      return 0;

    return 1 + std::max(realHeight(root->left), realHeight(root->right));
  }

  bool validateBST(node *root, int minv, int maxv) {

    if (root == nullptr)
      return true;

    if (root->key <= minv || root->key >= maxv)
      return false;

    return validateBST(root->left, minv, root->key) &&
           validateBST(root->right, root->key, maxv);
  }

  bool validateAVL(node *root) {

    if (root == nullptr)
      return true;

    int lh = realHeight(root->left);
    int rh = realHeight(root->right);

    int bf = rh - lh;

    if (std::abs(bf) > 1) {

      std::cout << "AVL VIOLATION at node " << root->key << " BF=" << bf
                << std::endl;

      return false;
    }

    if (heightb[root] != bf) {

      std::cout << "BALANCE FACTOR WRONG at node " << root->key
                << " expected=" << bf << " stored=" << heightb[root]
                << std::endl;

      return false;
    }

    return validateAVL(root->left) && validateAVL(root->right);
  }

public:
  void assertCorrect() {

    assert(validateBST(tree, INT_MIN, INT_MAX));
    assert(validateAVL(tree));

    std::cout << "AVL VALIDATION PASSED\n";
  }
};

// ============================================================
// TESTER
// ============================================================

class Tester {

public:
  Tester() { std::cout << "Starting AVL Tests\n"; }
  struct IntComparator {
    // Returns: -1 if a < b, 0 if a == b, 1 if a > b
    int operator()(const int &a, const int &b) const {
      if (a == b)
        return 0;
      else if (a < b)
        return -1;
      else
        return 1;
    }
  };

  // ============================================================
  // RR ROTATION
  // ============================================================

  void testRR() {

    std::cout << "\n===== RR TEST =====\n";

    AVLtree<int, int, IntComparator> t;

    t.insertion(1, 0);
    t.assertCorrect();

    t.insertion(2, 0);
    t.assertCorrect();

    t.insertion(3, 0);
    t.assertCorrect();

    t.printTree();

    auto root = t.tree;

    assert(root->key == 2);
    assert(root->left->key == 1);
    assert(root->right->key == 3);

    std::cout << "RR TEST PASSED\n";
  }

  // ============================================================
  // LL ROTATION
  // ============================================================

  void testLL() {

    std::cout << "\n===== LL TEST =====\n";

    AVLtree<int, int, IntComparator> t;

    t.insertion(3, 0);
    t.assertCorrect();

    t.insertion(2, 0);
    t.assertCorrect();

    t.insertion(1, 0);
    t.assertCorrect();

    t.printTree();

    auto root = t.tree;

    assert(root->key == 2);
    assert(root->left->key == 1);
    assert(root->right->key == 3);

    std::cout << "LL TEST PASSED\n";
  }

  // ============================================================
  // LR ROTATION
  // ============================================================

  void testLR() {

    std::cout << "\n===== LR TEST =====\n";

    AVLtree<int, int, IntComparator> t;

    t.insertion(3, 0);
    t.assertCorrect();

    t.insertion(1, 0);
    t.assertCorrect();

    t.insertion(2, 0);
    t.assertCorrect();

    t.printTree();

    auto root = t.tree;

    assert(root->key == 2);
    assert(root->left->key == 1);
    assert(root->right->key == 3);

    std::cout << "LR TEST PASSED\n";
  }

  // ============================================================
  // RL ROTATION
  // ============================================================

  void testRL() {

    std::cout << "\n===== RL TEST =====\n";

    AVLtree<int, int, IntComparator> t;

    t.insertion(1, 0);
    t.assertCorrect();

    t.insertion(3, 0);
    t.assertCorrect();

    t.insertion(2, 0);
    t.assertCorrect();

    t.printTree();

    auto root = t.tree;

    assert(root->key == 2);
    assert(root->left->key == 1);
    assert(root->right->key == 3);

    std::cout << "RL TEST PASSED\n";
  }

  // ============================================================
  // RANDOMIZED STRESS TEST
  // ============================================================

  void randomizedTest() {

    std::cout << "\n===== RANDOMIZED TEST =====\n";

    AVLtree<int, int, IntComparator> t;

    std::vector<int> vals;

    for (int i = 1; i <= 1000; i++)
      vals.push_back(i);

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(vals.begin(), vals.end(), g);

    std::cout << "INSERTION SECTION STARTS\n";
    for (auto v : vals) {

      std::cout << v << std::endl;
      t.insertion(v, 0);

      t.assertCorrect();
    }

    std::shuffle(vals.begin(), vals.end(), g);

    std::cout << "DELETION SECTION STARTS\n";
    for (auto v : vals) {

      std::cout << v << std::endl;
      t.deletion(v);

      t.assertCorrect();
    }

    std::cout << "RANDOMIZED TEST PASSED\n";
  }

  // ============================================================
  // JOIN OPERATION TESTS
  // ============================================================

  void testJoinEqualHeight() {

    std::cout << "\n===== JOIN EQUAL HEIGHT =====\n";

    AVLtree<int, int, IntComparator> t;
    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;

    t1.insertion(2, 0);
    t1.insertion(1, 0);
    t1.insertion(3, 0);

    t2.insertion(8, 0);
    t2.insertion(7, 0);
    t2.insertion(9, 0);

    t.tree = t.JoingOp(t1.tree, t2.tree, 5, 0);

    t.printTree();
    t.assertCorrect();

    std::cout << "JOIN EQUAL HEIGHT PASSED\n";
  }

  // ============================================================

  void testJoinLeftHeavy() {

    std::cout << "\n===== JOIN LEFT HEAVY =====\n";

    AVLtree<int, int, IntComparator> leftTree;

    for (int i = 1; i <= 5; i++)
      leftTree.insertion(i, 0);

    AVLtree<int, int, IntComparator> rightTree;

    rightTree.insertion(100, 0);
    rightTree.insertion(101, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.JoingOp(leftTree.tree, rightTree.tree, 50, 0);

    finalT.printTree();
    finalT.assertCorrect();

    std::cout << "JOIN LEFT HEAVY PASSED\n";
  }

  // ============================================================

  void testJoinRightHeavy() {

    std::cout << "\n===== JOIN RIGHT HEAVY =====\n";

    AVLtree<int, int, IntComparator> leftTree;

    leftTree.insertion(1, 0);
    leftTree.insertion(2, 0);

    AVLtree<int, int, IntComparator> rightTree;

    for (int i = 100; i <= 105; i++)
      rightTree.insertion(i, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.JoingOp(leftTree.tree, rightTree.tree, 50, 0);

    finalT.printTree();
    finalT.assertCorrect();

    std::cout << "JOIN RIGHT HEAVY PASSED\n";
  }

  // ============================================================

  void testDeepRecursiveJoin() {

    std::cout << "\n===== DEEP RECURSIVE JOIN =====\n";

    AVLtree<int, int, IntComparator> leftTree;
    AVLtree<int, int, IntComparator> rightTree;

    for (int i = 1; i <= 5; i++)
      leftTree.insertion(i, 0);

    for (int i = 7; i <= 10; i++)
      rightTree.insertion(i, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.JoingOp(leftTree.tree, rightTree.tree, 6, 0);

    finalT.printTree();
    finalT.assertCorrect();

    std::cout << "DEEP RECURSIVE JOIN PASSED\n";
  }

  // ============================================================

  void testJoinRotationTrigger() {

    std::cout << "\n===== JOIN ROTATION TRIGGER =====\n";

    AVLtree<int, int, IntComparator> leftTree;

    leftTree.insertion(1, 0);
    leftTree.insertion(2, 0);
    leftTree.insertion(3, 0);
    leftTree.insertion(4, 0);
    leftTree.insertion(5, 0);

    AVLtree<int, int, IntComparator> rightTree;

    rightTree.insertion(100, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.JoingOp(leftTree.tree, rightTree.tree, 50, 0);

    finalT.printTree();
    finalT.assertCorrect();

    std::cout << "JOIN ROTATION TRIGGER PASSED\n";
  }

  // ============================================================

  void testSingleNodeJoin() {

    std::cout << "\n===== SINGLE NODE JOIN =====\n";
    AVLtree<int, int, IntComparator> t;
    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;
    t1.insertion(1, 0);
    t2.insertion(3, 0);
    t.tree = t.JoingOp(t1.tree, t2.tree, 2, 0);

    t.printTree();
    t.assertCorrect();

    std::cout << "SINGLE NODE JOIN PASSED\n";
  }

  // ============================================================
  // SPLIT + UNION TESTS
  // ============================================================

  void testSplitMiddle() {

    std::cout << "\n===== SPLIT MIDDLE =====\n";

    AVLtree<int, int, IntComparator> t;

    for (int i = 1; i <= 15; i++)
      t.insertion(i, 0);

    std::cout << "\nORIGINAL TREE\n";
    t.printTree();

    auto res = t.SplitTrees(t.tree, 8);

    AVLtree<int, int, IntComparator> leftT;
    AVLtree<int, int, IntComparator> rightT;

    leftT.tree = res.first;
    rightT.tree = res.second;

    std::cout << "\nLEFT TREE\n";
    leftT.printTree();

    std::cout << "\nRIGHT TREE\n";
    rightT.printTree();

    leftT.assertCorrect();
    rightT.assertCorrect();

    std::cout << "SPLIT MIDDLE PASSED\n";
  }

  // ============================================================

  void testSplitLeaf() {

    std::cout << "\n===== SPLIT LEAF =====\n";

    AVLtree<int, int, IntComparator> t;

    for (int i = 1; i <= 10; i++)
      t.insertion(i, 0);

    auto res = t.SplitTrees(t.tree, 1);

    AVLtree<int, int, IntComparator> leftT;
    AVLtree<int, int, IntComparator> rightT;

    leftT.tree = res.first;
    rightT.tree = res.second;

    leftT.recomputeBF(leftT.tree);
    rightT.recomputeBF(rightT.tree);

    std::cout << "\nLEFT TREE\n";
    leftT.printTree();

    std::cout << "\nRIGHT TREE\n";
    rightT.printTree();

    leftT.assertCorrect();
    rightT.assertCorrect();

    std::cout << "SPLIT LEAF PASSED\n";
  }

  // ============================================================

  void testSplitRoot() {

    std::cout << "\n===== SPLIT ROOT =====\n";

    AVLtree<int, int, IntComparator> t;

    for (int i = 1; i <= 7; i++)
      t.insertion(i, 0);

    std::cout << "\nORIGINAL TREE\n";
    t.printTree();

    auto res = t.SplitTrees(t.tree, 4);

    AVLtree<int, int, IntComparator> leftT;
    AVLtree<int, int, IntComparator> rightT;

    leftT.tree = res.first;
    rightT.tree = res.second;

    leftT.recomputeBF(leftT.tree);
    rightT.recomputeBF(rightT.tree);

    std::cout << "\nLEFT TREE\n";
    leftT.printTree();

    std::cout << "\nRIGHT TREE\n";
    rightT.printTree();

    leftT.assertCorrect();
    rightT.assertCorrect();

    std::cout << "SPLIT ROOT PASSED\n";
  }

  // ============================================================

  void testSplitDeep() {

    std::cout << "\n===== SPLIT DEEP =====\n";

    AVLtree<int, int, IntComparator> t;

    for (int i = 1; i <= 100; i++)
      t.insertion(i, 0);

    auto res = t.SplitTrees(t.tree, 57);

    AVLtree<int, int, IntComparator> leftT;
    AVLtree<int, int, IntComparator> rightT;

    leftT.tree = res.first;
    rightT.tree = res.second;

    leftT.recomputeBF(leftT.tree);
    rightT.recomputeBF(rightT.tree);

    std::cout << "\nLEFT TREE\n";
    leftT.printTree();

    std::cout << "\nRIGHT TREE\n";
    rightT.printTree();

    leftT.assertCorrect();
    rightT.assertCorrect();

    std::cout << "SPLIT DEEP PASSED\n";
  }

  // ============================================================

  void testUnionSimple() {

    std::cout << "\n===== UNION SIMPLE =====\n";

    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;

    for (int i = 1; i <= 5; i++)
      t1.insertion(i, 0);

    for (int i = 100; i <= 105; i++)
      t2.insertion(i, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.UnionTrees(t1.tree, t2.tree);
    finalT.recomputeBF(finalT.tree);

    finalT.printTree();

    finalT.assertCorrect();

    std::cout << "UNION SIMPLE PASSED\n";
  }

  // ============================================================

  void testUnionInterleaved() {

    std::cout << "\n===== UNION INTERLEAVED =====\n";

    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;

    t1.insertion(10, 0);
    t1.insertion(20, 0);
    t1.insertion(30, 0);
    t1.insertion(40, 0);

    t2.insertion(5, 0);
    t2.insertion(15, 0);
    t2.insertion(25, 0);
    t2.insertion(35, 0);
    t2.insertion(45, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.UnionTrees(t1.tree, t2.tree);
    finalT.recomputeBF(finalT.tree);

    finalT.printTree();

    finalT.assertCorrect();

    std::cout << "UNION INTERLEAVED PASSED\n";
  }

  // ============================================================

  void testUnionLarge() {

    std::cout << "\n===== UNION LARGE =====\n";

    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;

    for (int i = 1; i <= 100; i += 2)
      t1.insertion(i, 0);

    for (int i = 2; i <= 100; i += 2)
      t2.insertion(i, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.UnionTrees(t1.tree, t2.tree);

    finalT.recomputeBF(finalT.tree);

    finalT.printTree();

    finalT.assertCorrect();

    std::cout << "UNION LARGE PASSED\n";
  }

  // ============================================================

  void testUnionEmpty() {

    std::cout << "\n===== UNION EMPTY =====\n";

    AVLtree<int, int, IntComparator> t1;
    AVLtree<int, int, IntComparator> t2;

    for (int i = 1; i <= 10; i++)
      t1.insertion(i, 0);

    AVLtree<int, int, IntComparator> finalT;

    finalT.tree = finalT.UnionTrees(t1.tree, nullptr);

    finalT.recomputeBF(finalT.tree);

    finalT.printTree();

    finalT.assertCorrect();

    std::cout << "UNION EMPTY PASSED\n";
  }

  // ============================================================

  void testSplitThenUnion() {

    std::cout << "\n===== SPLIT THEN UNION =====\n";

    AVLtree<int, int, IntComparator> t;

    for (int i = 1; i <= 50; i++)
      t.insertion(i, 0);

    auto res = t.SplitTrees(t.tree, 25);

    AVLtree<int, int, IntComparator> rebuilt;

    rebuilt.tree = rebuilt.UnionTrees(res.first, res.second);

    rebuilt.recomputeBF(rebuilt.tree);

    rebuilt.printTree();

    rebuilt.assertCorrect();

    std::cout << "SPLIT THEN UNION PASSED\n";
  }
  //============================================================
  // RUN ALL
  // ============================================================

  void runAllTests() {

    // testRR();
    //  testLL();
    //  testLR();
    //  testRL();
    //
    //  randomizedTest();
    //
    // --- Join testcases ----
    // testJoinEqualHeight();
    // testJoinLeftHeavy();
    // testJoinRightHeavy();
    // testDeepRecursiveJoin();
    // testJoinRotationTrigger();
    // testSingleNodeJoin();

    // --union & split testcases ----
    testSplitMiddle();
    testSplitLeaf();
    testSplitRoot();
    testSplitDeep();

    testUnionSimple();
    testUnionInterleaved();
    testUnionLarge();
    testUnionEmpty();

    testSplitThenUnion();

    std::cout << "\nALL TESTS PASSED\n";
  }
};

// ============================================================
// MAIN
// ============================================================
// int main() {
// ------------------------------------------------ //
// BST ..
// BST *bst = new BST();
//
// // The BST starts with: 44, 17, 88, 32, 65, 97, 28, 54, 82, 29, 76, 80,
// 78
// // Note: Your constructor already calls removal_node(32).
//
// std::cout << "\n--- Starting Additional Tests ---" << std::endl;
//
// // Test 1: Remove a Leaf Node (No children)
// // 29 is a leaf (child of 28).
// std::cout << "\nTest 1: Removing Leaf 29" << std::endl;
// bst->removal_node(29);
// // Result: 28's right child should now be nullptr.
//
// // Test 2: Remove a node with One Child
// // 82 has one child (76) after the internal removals in your BST.
// // Wait, let's pick 97 (leaf) or 88 (two children).
// // Let's try 54 (child of 65).
// std::cout << "\nTest 2: Removing Node 54 (One Child/Leaf)" << std::endl;
// bst->removal_node(54);
//
// // Test 3: Remove a node with Two Children
// // 88 has children 65 and 97.
// std::cout << "\nTest 3: Removing Node 88 (Two Children)" << std::endl;
// bst->removal_node(88);
// // Result: Successor (97 or 82) should move up.
//
// // Test 4: Removing the Root (TRICKY)
// // 44 is the root.
// std::cout << "\nTest 4: Removing Root 44" << std::endl;
// bst->removal_node(44);
// // Result: If pos[0].second is null, your code might crash here!
// // Check if pos[0].second exists before accessing .key.
//
// // Test 5: Non-existent element
// std::cout << "\nTest 5: Removing 999 (Doesn't exist)" << std::endl;
// bst->removal_node(999);
// Result: Should print "Element does not exit"

// -------------------------------------
// AVL trees ..
//   auto t = new Tester();
//   t->runAllTests();
//   // -----------------------------------------
//   return 0;
// }
