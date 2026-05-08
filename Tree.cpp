#include <cstddef>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <ostream>
#include <utility>
// rewrite the removal with spawing idea over naive idea i used ...
class BST {

  typedef struct node {
    int key = -1;
    struct node *left = nullptr;
    struct node *right = nullptr;
  } node;

  node *tree;

public:
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

class AVLtree {
  /*
    Here we are working towards self balancing trees where we self balnce trees
    in order to armortize our search,insertion,deletion to log(N) , from the
    original paper we are using the golden ration and fibonacci numbers in
    showing what is the limit of nodes per depth
  */
  typedef struct node {
    int key;
    node *left;
    node *right;
  } node;

  node *tree;
  std::map<node *, int> heightb; // measures height balance factor

public:
  AVLtree() { tree = nullptr; }
  std::pair<node *, node *> search_internal(int k, node *tree,
                                   node *current_ptr = nullptr,
                                   node *parent_ptr = nullptr) {
    if (tree == nullptr)
      return {current_ptr, parent_ptr};

    if (tree->key == k)
      return {current_ptr, parent_ptr};
    else if ((*tree).key > k)
      return search_internal(k, tree->left, tree->left, tree);
    else
      return search_internal(k, tree->right, tree->right, tree);
  }; 

  std::pair<node *, node *> search(int k , node* tree_acc) {
    return search_internal(k, tree_acc, tree, nullptr);
  }

  // search through like BST ..

  node *left_rotate(node *f, node *s) {
    f->right = s->left;
    s->left = f;
    if (heightb[s] == 0) {
      heightb[f] = 1;
      heightb[s] = -1;
    } else {
      heightb[s] = 0;
      heightb[f] = 0;
    }
    return s;
  }
  node *right_rotate(node *f, node *s) {
    f->left = s->right;
    s->right = f;
    if (heightb[s] == 0) {
      heightb[s] = 1;
      heightb[f] = -1;
    } else {
      heightb[s] = 0;
      heightb[f] = 0;
    }
    return s;
  }
  node *left_right_rotate(node *f, node *s) {
    // right_rotate in node* s
    auto Y = heightb[s->left];
    auto G = right_rotate(s, s->left);
    f->right = G;
    auto G2 = left_rotate(f, G);
    if (Y == 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 0;
    } else if (Y < 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 1;
    } else {
      heightb[G2->right] = -1;
      heightb[G2->left] = 0;
    }
    return G2;
  }

  node *right_left_rotate(node *f, node *s) {
    // left_rotate in node* s
    auto Y = heightb[s->right];
    auto G = left_rotate(s, s->right);
    f->left = G;
    auto G2 = right_rotate(f, G);
    if (Y == 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 0;
    } else if (Y < 0) {
      heightb[G2->right] = 0;
      heightb[G2->left] = 1;
    } else {
      heightb[G2->right] = -1;
      heightb[G2->left] = 0;
    }
    return G2;
  }

  void insertion(int k) {
    auto pos = search(k, tree);

    // empty root
    if (pos.second == nullptr) {

      node *temp = new node();
      temp->key = k;
      tree = temp;
      heightb[temp] = 0;
      return;
    }
    // add the node height here ... and node as well
    node *new_incoming = new node();
    new_incoming->key = k;
    std::cout << pos.second->key << std::endl;
    if (pos.second->key < k) {
      pos.second->right = new_incoming;
    } else {
      pos.second->left = new_incoming;
    }
    heightb[new_incoming] = 0;
    this->printTree();
    // we iterate over the parents to fix unbalance
    auto Z = new_incoming;
    for (auto X = pos.second; X != nullptr;) {
      node *G = new node();
      node *N = new node();
      auto pos3 = search(X->key, tree);
      if (X->right && X->right->key == Z->key) {
        if (heightb[X] > 0) {
          G = pos3.second;
          std::cout << "> case" << std::endl;
          if (heightb[Z] >= 0) {
            N = left_rotate(X, Z);
          } else {
            N = left_right_rotate(X, Z);
          }
        } else {

          if (heightb[X] < 0) {
            std::cout << "<0 case" << std::endl;
            heightb[X] = 0;
            break;
          }
          std::cout << "0 case" << std::endl;

          heightb[X] = 1;
          // next parent updation for X,Z;
          Z = X;
          X = pos3.second;
          continue;
        }
      } else if (X->left && X->left->key == Z->key) {
        if (heightb[X] < 0) {
          G = pos3.second;
          std::cout << "<0 case -- l" << std::endl;
          if (heightb[Z] <= 0) {
            N = right_rotate(X, Z);
          } else {
            N = right_left_rotate(X, Z);
          }
        } else {
          if (heightb[X] > 0) {
            std::cout << ">0 case -- l" << std::endl;
            heightb[X] = 0;
            break;
          }
          heightb[X] = -1;
          // next parent updation for X,Z;
          std::cout << "0 case -- l" << std::endl;
          Z = X;
          X = pos3.second;
          continue;
        }
      }

      // attach the displaced node over here ...
      std::cout << "Attaching" << std::endl;
      if (G == nullptr)
        tree = N;
      else if (G->key < N->key)
        G->right = N;
      else
        G->left = N;

      break;
    }
  };
  // insertion like normally how BST works , but requires self
  // balancing using 4 cases we generally see should be used

  node *bst_succesor(node *x) {
    if (x->right != nullptr) {
      auto y = x->right;
      while (y->left != nullptr) {
        y = y->left;
      }
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

  node *shift_nodes(node *u, node *v) {
    auto u_pos = search(u->key, tree);
    if (u_pos.second == nullptr) {
      v->left = tree->left;
      v->right = tree->right;
      tree = v;
    } else if (u_pos.second->left == u_pos.first) {
      u_pos.second->left = v;
    } else if (u_pos.second->right == u_pos.first) {
      u_pos.second->right = v;
    }
    return u_pos.second;
  }

  node *bst_delete(int k) {
    auto u_pos = search(k, tree);
    if (u_pos.first->left == nullptr) {
      return shift_nodes(u_pos.first, u_pos.first->right);

    } else if (u_pos.first->right == nullptr) {
      return shift_nodes(u_pos.first, u_pos.first->left);
    } else {
      auto y = bst_succesor(u_pos.first);
      auto y_pos = search(y->key, tree);
      if (y_pos.second != u_pos.first) {
        y->right = u_pos.first->right;
      }
      shift_nodes(u_pos.first, y);
      y->left = u_pos.first->left;
      return y_pos.second;
    }
  }

  void deletion(int k) {

    // noraml BST delete
    auto new_pos_on_del = bst_delete(k);
    std::cout << "Deleted from the tree" << new_pos_on_del->key <<  std::endl;
    printTree();
    // Self balancing occurs now
    auto pos_del = search(new_pos_on_del->key, tree);
    std::cout << "pos found from the tree" << pos_del.first->key << std::endl;
    auto Z = pos_del.first;
    for (auto X = pos_del.second; X != nullptr;) {
      auto pos3 = search(X->key, tree);
      node *G = new node();
      node *N = new node();
      if (X->left == new_pos_on_del) {
        if (heightb[X] > 0) {
          std::cout << "--l >0" << std::endl;
          G = pos3.second;
          if (heightb[Z] < 0) {
            N = left_right_rotate(X, Z);
          } else {
            N = right_rotate(X, Z);
          }
        } else {
          if (heightb[X] < 0) {
            std::cout << "--l <0" << std::endl;
            heightb[X] = 0;
            break;
          }
          std::cout << "--l =0" << std::endl;
          heightb[X] = -1;
          Z = X;
          X = pos3.second;
          continue;
        }

      } else if (X->right == new_pos_on_del) {

        if (heightb[X] < 0) {
          G = pos3.second;
          std::cout << "--r <0" << std::endl;
          if (heightb[Z] < 0) {
            N = left_right_rotate(X, Z);
          } else {
            N = right_rotate(X, Z);
          }
        } else {
          if (heightb[X] > 0) {
            std::cout << "--r >0" << std::endl;
            heightb[X] = 0;
            break;
          }
          std::cout << "--r =0" << std::endl;
          heightb[X] = -1;
          Z = X;
          X = pos3.second;
          continue;
        }
      }

      if (G == nullptr) {
        tree = N;
      } else if (G->key >= N->key)
        G->left = N;
      else
        G->right = N;

      break;
    }
  };
  void join(); // join , split , union_AVL are methods which help with
               // parallelization of bulk operations which something we need
               // for storage engines in general
  void split();
  void union_AVL();

  void printTree() {
    std::deque<node *> dq;
    auto temp_tree = tree;
    dq.push_back(temp_tree);
    int depth = 0;
    while (!dq.empty() && depth < 3) {
      std::cout << " ------- depth:" << depth << " ------" << std::endl;

      int len_curr_dq = dq.size();
      std::cout << dq.size() << std::endl;
      for (int i = 0; i < len_curr_dq; i++) {
        auto curr = dq.front();
        dq.pop_front();
        if (curr->key > 0) {
          std::cout << "key:" << curr->key << "--" << heightb[curr];
        }
        if (curr->left != nullptr)
          dq.push_back((curr->left));
        else {
          // node *null_node = new node();
          // dq.push_back(*null_node);
        }
        if (curr->right != nullptr)
          dq.push_back(curr->right);
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

class Tester {
  enum test_type { BST, AVL };
  test_type tt;

public:
  Tester() { std::cout << "Starting Tester" << std::endl; }
  void testPropagation() {
    AVLtree avl;
    // Create a deep tree
    for (int i = 1; i <= 15; i++)
      avl.insertion(i);

    // Delete nodes from the left to force right-heavy imbalances
    // that propagate upwards.
    std::cout << "\nTesting Propagation (Deleting 1, 2, 3):" << std::endl;
    avl.deletion(1);
    avl.deletion(2);
    avl.deletion(3);
    avl.printTree();
  }
  void testDeletion() {
    AVLtree avl;
    // Build a balanced tree first: 10, 20, 30, 5, 25, 40
    int nodes[] = {20, 10, 30, 5, 15, 25, 40};
    for (int n : nodes)
      avl.insertion(n);

    // Case 1: Delete a leaf (no rotation needed)
    // Delete 5 -> Height of 10's left subtree changes, BF(10) should change
    std::cout << "\nDeleting Leaf 5:" << std::endl;
    avl.deletion(5);
    avl.printTree();

    // Case 2: Delete a node with one child
    // Delete 10 (which now only has 15) -> 15 should shift up
    std::cout << "\nDeleting Node with one child 10:" << std::endl;
    avl.deletion(10);
    avl.printTree();

    // Case 3: The "R0" Deletion (Your initial question!)
    // Delete from a subtree such that parent BF becomes 2 and child BF is 0
    // This requires a specific setup:
    //        10(BF:1)
    //       /  \
    //    5(BF:0) 15 (delete 15)
    //    / \
    //   2   7
    AVLtree r0_tree;
    r0_tree.insertion(10);
    r0_tree.insertion(5);
    r0_tree.insertion(15);
    r0_tree.insertion(2);
    r0_tree.insertion(7);
    std::cout << "\nTesting R0 Deletion (Delete 15):" << std::endl;
    r0_tree.deletion(15);
    r0_tree.printTree();
  }

  void testInsertion() {
    AVLtree avl;

    // Case 1: Right-Right (Single Left Rotation)
    // Order: 10, 20, 30 -> Root should be 20
    avl.insertion(10);
    avl.insertion(20);
    avl.insertion(30);
    std::cout << "After RR Insertion (10,20,30):" << std::endl;
    avl.printTree();

    // Case 2: Left-Left (Single Right Rotation)
    // Order: 5, 4 -> Root 20 remains, but 10, 5, 4 should rebalance 10,5,4 to
    // root 5
    avl.insertion(5);
    avl.insertion(4);
    std::cout << "After LL Insertion (5,4):" << std::endl;
    avl.printTree();

    // Case 3: Right-Left (Double Rotation)
    // Insertion of 25 into current tree
    avl.insertion(25);
    std::cout << "After RL Insertion (25):" << std::endl;
    avl.printTree();
  }
  void testInsertions2() {
    // Case 1: Right-Right (Single Left Rotation)
    // Insertion order: 1, 2, 3
    //  Expected: 2 is root, 1 is left, 3 is right
    std::cout << "Test 1: Right-Right Case" << std::endl;
    AVLtree t1;
    t1.insertion(1);
    t1.insertion(2);
    t1.insertion(3);
    t1.printTree();

    // Case 2: Left-Left (Single Right Rotation)
    // Insertion order: 3, 2, 1
    std::cout << "\nTest 2: Left-Left Case" << std::endl;
    AVLtree t2;
    t2.insertion(3);
    t2.insertion(2);
    t2.insertion(1);
    t2.printTree();

    // Case 3: Left-Right (Double Rotation)
    // Insertion order: 3, 1, 2
    // 1. Insert 3, then 1 (Left heavy)
    // 2. Insert 2 (Right child of 1) -> Triggers Left-Right
    std::cout << "\nTest 3: Left-Right Case" << std::endl;
    AVLtree t3;
    t3.insertion(3);
    t3.insertion(1);
    t3.insertion(2);
    t3.printTree();

    // Case 4: Right-Left (Double Rotation)
    // Insertion order: 1, 3, 2
    std::cout << "\nTest 4: Right-Left Case" << std::endl;
    AVLtree t4;
    t4.insertion(1);
    t4.insertion(3);
    t4.insertion(2);
    t4.printTree();
  }
  void Avl_tests() {
    testInsertion();
    testDeletion();
    testPropagation();
  }
};

int main() {
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
  auto t = new Tester();
  t->Avl_tests();
  // -----------------------------------------
  return 0;
}
