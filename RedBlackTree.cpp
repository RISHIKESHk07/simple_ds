#include <iostream>
#include <vector>
//  The main reason of using red black trees over the AVL tree is that they much
//  less strict per node wise and present a solution which can make the system
//  balanced , RB trees are actually a variant of 2-4-3 trees making them pretty
//  a binary B tree So you can make a one on one mapping to 2-3 or 2-4-3 tree
//  generally . Some extra info is the we have //variants of RB tree called left
//  leaning and righ leaning trees which impose some extra conditions to // make
//  the implementation of the tree itself easier

class RedBlackTree {
public:
  enum Color { red, black };
  enum Direction : int { LEFT, RIGHT };
  typedef struct node {
    node *parent = nullptr;
    union {

      struct {
        struct node *left = nullptr;
        struct node *right = nullptr;
      };
      struct node *child[2];
    };
    int key;
    Color color;
  } node;

  node *tree = nullptr;
  Direction return_dir_opposite(Direction dir) {
    if (dir == Direction::RIGHT)
      return Direction::LEFT;
    else
      return Direction::RIGHT;
  }
  node *subtree_rotation(node *sub, Direction dir) {
    auto new_root = sub->child[1 - dir];
    auto old_root_parent = sub->parent;
    auto new_child = new_root->child[dir];

    sub->child[1 - dir] = new_child;

    if (new_child != nullptr)
      new_child->parent = sub;

    new_root->child[dir] = sub;
    sub->parent = new_root;

    new_root->parent = old_root_parent;

    if (old_root_parent) {
      old_root_parent->child[sub == old_root_parent->right] = new_root;
    } else {
      tree = new_root; // this one line almost wasted some time , lore is tree
                       // is a pointer but we are using -> access it here
                       // meaning we are actually changing a local version which
                       // is not mapped to the global scope resulting in this
                       // change not being visible after this updation ...
                       // another secret to maintain pointers correctly .
    }

    return new_root;
  }
  // on insertion remeber we will always add a red node , inorder to make sure
  // the balance is not disturbed and now we have to look case of the node
  // attached to is black or red ,
  // if black proceed forward nothing else matters ... cannot break balance
  // *case 1*, if red node then , P->R G->B U->R  --> P & U ->black G->red *case
  // 2* , issue this could make G 's parent unstable meaning we need to check
  // the roots above to make sure it fine , if P is red and G is root ---> P is
  // black (check this once) if P is red and root ---> P is black *case4* P->R
  // G->B U->B --> P is rotated left and then right , P->B & N->R & G->R p->R
  // G->B U->B outer -> P is new root by rotaing right and then p->B N->R & G->R

  std::pair<node *, node *> search(node *tree, int k, node *parent = nullptr) {
    if (tree == nullptr)
      return {nullptr, parent};
    if (tree->key == k)
      return {tree, parent};
    else if (tree->key >= k)
      return search(tree->left, k, tree);
    else
      return search(tree->right, k, tree);
  }

  void insertion(int k) {
    // insertion of a single node at leaf somewhere
    auto ins_parent = search(tree, k);
    auto ins_node = new node();
    ins_node->key = k;
    ins_node->parent = ins_parent.second;
    ins_node->color = Color::red;

    if (ins_parent.second == nullptr) {
      tree = ins_node;
      return;
    } else if (ins_parent.second->key > k)
      ins_parent.second->left = ins_node;
    else
      ins_parent.second->right = ins_node;
    std::cout << "Added node to tree at parent:" << ins_parent.second->key
              << std::endl;
    // RB violations checker and while loop
    auto X = ins_parent.second;
    auto N = ins_node;
    do {
      // case 1; black parent so we are done
      if (X->color == Color::black) {
        std::cout << "case 1" << std::endl;
        return;
      } else {
        auto G = X->parent;

        if (G == nullptr) {
          // root case
          std::cout << "case 4" << std::endl;
          X->color = Color::black;
          return;
        }
        Direction dir =
            (X->parent->right == X) ? Direction::RIGHT : Direction::LEFT;
        auto U = X->parent->child[1 - dir];
        std::cout << dir << std::endl;
        if (!U || U->color == Color::black) {
          std::cout << "case 5 & 6" << std::endl;
          auto G_parent = G->parent;
          Direction G_parent_dir =
              G_parent ? ((G_parent->right == G) ? Direction::RIGHT
                                                 : Direction::LEFT)
                       : Direction::LEFT;

          if (X->child[1 - dir] == N) {
            auto new_root = subtree_rotation(X, dir);
            G->child[dir] = new_root;
            N = X;
            X = new_root;
          }
          auto new_root2 = subtree_rotation(G, return_dir_opposite(dir));
          new_root2->color = Color::black;
          new_root2->child[1 - dir]->color = Color::red;
          X = new_root2;

          new_root2->parent = G_parent;
          if (G_parent == nullptr) {
            tree = new_root2;
          } else {
            G_parent->child[G_parent_dir] = new_root2;
            if (G_parent_dir == Direction::RIGHT)
              G_parent->right = new_root2;
            else
              G_parent->left = new_root2;
          }

          return;
        }

        if (U->color == Color::red) {
          std::cout << "case 2" << std::endl;
          // color shift and now we can have violations above this point in
          // parent of grandparent so we need to fix it ... look up for code for
          // the flow of this
          G->color = Color::red;
          X->color = Color::black;
          U->color = Color::black;

          N = G;
          X = G->parent;
          continue;
        }
      }

      N = X;
      X = X->parent;
    } while (X != nullptr);

    return;
  }
  node *sucessor(int k) {
    // min on right sub-tree
    auto n = search(tree, k);
    if (n.first->right != nullptr) {
      auto x = n.first->right;
      while (x->left != nullptr) {
        x = x->left;
      }
      return x;
    } else {
      auto y = n.first->parent;
      while (y->key < k) {
        y = y->parent;
      }
      return y;
    }
  }
  Color get_Color(node *n) { return n ? n->color : Color::black; }
  void deletion(int k) {
    // normal deletion here ...
    auto n = search(tree, k);
    node *pod;
    if (!n.first) {
      return; // deletion not possible
    }
    if (n.first->left != nullptr && n.first->right != nullptr) {
      pod = sucessor(k);
      n.first->key = pod->key;
      // deletion of the successor node is being done accordingly;

    } else {
      pod = n.first;
    }

    // root deletion cases

    if (pod->parent == nullptr) {

      if (pod->left == nullptr && pod->right == nullptr) {
        tree = nullptr;
        return;
      } else if (pod->left) {
        tree = pod->left;
        tree->parent = nullptr;
        tree->color = Color::black;
        return;
      } else {
        tree = pod->right;
        tree->parent = nullptr;
        tree->color = Color::black;
        return;
      }
    }

    auto rl = (pod->parent->right == pod) ? 1 : 0;
    auto t = pod->parent;
    n.first->key = pod->key;
    if (pod->color == Color::red) {
      if (pod->right) {
        if (rl) {
          t->right = pod->right;
          pod->right->parent = t;
        } else {
          t->left = pod->right;
          pod->right->parent = t;
        }
      } else {
        if (rl) {
          t->right = nullptr;
        } else
          t->left = nullptr;
      }
      std::cout << "del leaf red case" << std::endl;
      pod->right = nullptr;
      pod->parent = nullptr;
      return;
    }
    // deletion node is balck in color
    if (pod->right) {
      if (rl) {
        t->right = pod->right;
        pod->right->parent = t;
      } else {
        t->left = pod->right;
        pod->right->parent = t;
      }

      pod->right->color = Color::black;
      pod->right = nullptr;
      std::cout << "del black leaf with child present" << std::endl;
      pod->parent = nullptr;
      return;
    }
    // now the case where we have to delete a black node with no children
    else {
      // RB tree fixing should be done ... Right all 12 cases here remove
      // unecessary cases and then invesigate for simialr cases in all of them
      // and group them , and then you see a pattern where some cases can led by
      // moving through others , use the wiki page for table of structural
      // changes ...
      std::cout << "Del leaf no children ...  main del loop" << std::endl;
      Direction dir = rl ? Direction::RIGHT : Direction::LEFT;

      // deleting the node ... don't forget ...

      t->child[dir]->parent = nullptr;
      t->child[dir] = nullptr;

      auto X = t;
      std::cout << X->key << std::endl;
      auto N = X->child[dir];
      do {

        if (X == nullptr) {
          // change in color required at all
          std::cout << "root del in loop" << std::endl;
          return;
        } else {
          auto S = X->child[1 - dir];
          auto C = S->child[dir];
          auto D = S->child[1 - dir];

          if (get_Color(X) == Color::black && get_Color(S) == Color::red &&
              get_Color(C) == Color::black && get_Color(D) == Color::black) {

            // case #3
            std::cout << "del case 3" << std::endl;

            auto new_root = subtree_rotation(X, dir);

            new_root->color = Color::black;
            new_root->child[dir]->color = Color::red;

            // variable updates required here to make sure this is conssitent
            X = X;
            S = C;
            C = S->child[dir];
            D = S->child[1 - dir];

            if (get_Color(S) == Color::black && get_Color(C) == Color::red &&
                get_Color(D) == Color::black) {

              // case 5
              std::cout << "del case 3->5" << std::endl;
              goto case_5;
            }

            if (get_Color(S) == Color::black && get_Color(D) == Color::red) {

              // case 6
              std::cout << "del case 3->6" << std::endl;
              goto case_6;
            }

            if (get_Color(X) == Color::red && get_Color(S) == Color::black &&
                get_Color(C) == Color::black && get_Color(D) == Color::black) {

              std::cout << "del case 3->4" << std::endl;

              X->color = Color::black;
              S->color = Color::red;

              return;
            }
          }

          if (get_Color(S) == Color::black && get_Color(C) == Color::red &&
              get_Color(D) == Color::black) {

            // case 5
            std::cout << "del case 5" << std::endl;

            goto case_5;
          }

          if (get_Color(S) == Color::black && get_Color(D) == Color::red) {

            // case 6
            std::cout << "del case 6" << std::endl;

            goto case_6;
          }

          if (get_Color(X) == Color::red && get_Color(S) == Color::black &&
              get_Color(C) == Color::black && get_Color(D) == Color::black) {

            // case 4
            std::cout << "del case 4" << std::endl;

            X->color = Color::black;
            S->color = Color::red;

            return;
          }
          // case #2
          std::cout << "del case 2" << std::endl;

          if (get_Color(X) == Color::black && get_Color(S) == Color::black &&
              get_Color(C) == Color::black && get_Color(D) == Color::black) {

            S->color = Color::red;
          }
        }

        N = X;
        X = X->parent;
        if (X != nullptr)
          dir = (X->right == N)
                    ? Direction::RIGHT
                    : Direction::LEFT; // made a sumb mistake here dir was
                                       // comparing wrong stuff

      } while (X != nullptr);
      return;
    case_5: {
      auto new_root =
          subtree_rotation(X->child[1 - dir], return_dir_opposite(dir));
      new_root->color = Color::black;
      new_root->child[1 - dir]->color = Color::red;
      
      goto case_6;
    }
    case_6: {
      auto new_root2 = subtree_rotation(X, dir);
      new_root2->child[dir]->color = Color::black;
      new_root2->child[1 - dir]->color = Color::black;
      return;
    }
    }
  }
};
// Extra stuff for the tests ....
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

class RBTreeTester {
public:
  using node = RedBlackTree::node;

  // ==========================================================
  // TERMINAL COLORS
  // ==========================================================

  static constexpr const char *RED = "\033[31m";
  static constexpr const char *GREEN = "\033[32m";
  static constexpr const char *WHITE = "\033[37m";
  static constexpr const char *RESET = "\033[0m";

  // ==========================================================
  // SAFE COLOR ACCESS
  // ==========================================================

  static RedBlackTree::Color color(node *n) {
    return n ? n->color : RedBlackTree::Color::black;
  }

  // ==========================================================
  // PRETTY PRINTER
  // ==========================================================

  static void print_helper(node *root, std::string indent, bool last) {

    if (!root)
      return;

    std::cout << indent;

    if (last) {
      std::cout << "R----";
      indent += "     ";
    } else {
      std::cout << "L----";
      indent += "|    ";
    }

    if (root->color == RedBlackTree::Color::red)
      std::cout << RED;
    else
      std::cout << WHITE;

    std::cout << root->key << RESET << "\n";

    print_helper(root->left, indent, false);
    print_helper(root->right, indent, true);
  }

  static void print_tree(RedBlackTree &tree) {

    std::cout << "\n=====================================\n";

    if (!tree.tree) {
      std::cout << "EMPTY TREE\n";
    } else {
      print_helper(tree.tree, "", true);
    }

    std::cout << "=====================================\n";
  }

  // ==========================================================
  // VALIDATION
  // ==========================================================

  static int validate_black_height(node *root) {

    if (!root)
      return 1;

    int left = validate_black_height(root->left);

    int right = validate_black_height(root->right);

    if (left != right) {

      std::cout << RED << "BLACK HEIGHT VIOLATION at " << root->key << RESET
                << "\n";

      std::exit(1);
    }

    return left + (root->color == RedBlackTree::Color::black);
  }

  static void validate_red_property(node *root) {

    if (!root)
      return;

    if (root->color == RedBlackTree::Color::red) {

      if ((root->left && root->left->color == RedBlackTree::Color::red) ||

          (root->right && root->right->color == RedBlackTree::Color::red)) {

        std::cout << RED << "RED VIOLATION at " << root->key << RESET << "\n";

        std::exit(1);
      }
    }

    validate_red_property(root->left);
    validate_red_property(root->right);
  }

  static void validate_parent_links(node *root) {

    if (!root)
      return;

    if (root->left) {

      if (root->left->parent != root) {

        std::cout << RED << "BAD LEFT PARENT at " << root->left->key << RESET
                  << "\n";

        std::exit(1);
      }
    }

    if (root->right) {

      if (root->right->parent != root) {

        std::cout << RED << "BAD RIGHT PARENT at " << root->right->key << RESET
                  << "\n";

        std::exit(1);
      }
    }

    validate_parent_links(root->left);
    validate_parent_links(root->right);
  }

  static void validate_root(node *root) {

    if (!root)
      return;
  }

  static void validate_tree(RedBlackTree &tree) {

    validate_root(tree.tree);

    validate_red_property(tree.tree);

    validate_black_height(tree.tree);

    validate_parent_links(tree.tree);

    std::cout << GREEN << "TREE VALID\n" << RESET;
  }

  // ==========================================================
  // TESTS
  // ==========================================================

  static void test_single_insert_delete() {

    std::cout << "\n[TEST] SINGLE INSERT DELETE\n";

    RedBlackTree tree;

    tree.insertion(10);

    validate_tree(tree);

    print_tree(tree);

    tree.deletion(10);

    validate_tree(tree);

    print_tree(tree);
  }

  static void test_sequential_insert() {

    std::cout << "\n[TEST] SEQUENTIAL INSERT\n";

    RedBlackTree tree;

    for (int i = 1; i <= 10; i++) {

      tree.insertion(i);

      validate_tree(tree);
      print_tree(tree);
    }

    print_tree(tree);
  }

  static void test_sequential_delete() {

    std::cout << "\n[TEST] SEQUENTIAL DELETE\n";

    RedBlackTree tree;

    for (int i = 1; i <= 10; i++)
      tree.insertion(i);

    validate_tree(tree);
    print_tree(tree);

    for (int i = 1; i <= 10; i++) {

      tree.deletion(i);

      validate_tree(tree);
      print_tree(tree);
    }

    print_tree(tree);
  }

  static void test_reverse_delete() {

    std::cout << "\n[TEST] REVERSE DELETE\n";

    RedBlackTree tree;

    for (int i = 1; i <= 10; i++)
      tree.insertion(i);

    validate_tree(tree);

    for (int i = 10; i >= 1; i--) {

      tree.deletion(i);
      print_tree(tree);

      validate_tree(tree);
    }

    print_tree(tree);
  }

  static void test_random_insert_delete() {

    std::cout << "\n[TEST] RANDOM INSERT DELETE\n";

    RedBlackTree tree;

    std::vector<int> vals;

    for (int i = 0; i < 10; i++)
      vals.push_back(i);

    std::random_device rd;

    std::mt19937 g(rd());

    std::shuffle(vals.begin(), vals.end(), g);

    // RANDOM INSERTS

    for (auto v : vals) {
      std::cout << v << "inserting ..." << std::endl;
      tree.insertion(v);
      print_tree(tree);
      validate_tree(tree);
    }

    // RANDOM DELETES

    std::shuffle(vals.begin(), vals.end(), g);

    for (auto v : vals) {

      std::cout << v << "deleting .. " << std::endl;

      tree.deletion(v);
      print_tree(tree);
      validate_tree(tree);
    }

    print_tree(tree);
  }

  static void test_root_deletes() {

    std::cout << "\n[TEST] ROOT DELETES\n";

    RedBlackTree tree;

    for (int i = 1; i <= 10; i++)
      tree.insertion(i);

    validate_tree(tree);

    while (tree.tree) {

      int root_key = tree.tree->key;

      std::cout << "Deleting root: " << root_key << "\n";

      tree.deletion(root_key);

      validate_tree(tree);
    }
  }

  static void test_alternating_operations() {

    std::cout << "\n[TEST] ALTERNATING OPS\n";

    RedBlackTree tree;

    for (int i = 0; i < 10; i++) {

      tree.insertion(i);

      validate_tree(tree);

      if (i % 3 == 0) {

        tree.deletion(i);

        validate_tree(tree);
      }
    }

    print_tree(tree);
  }

  static void test_zig_zag_case() {

    std::cout << "\n[TEST] ZIG ZAG\n";

    RedBlackTree tree;

    tree.insertion(10);
    tree.insertion(20);
    tree.insertion(15);

    validate_tree(tree);

    print_tree(tree);

    tree.deletion(10);

    validate_tree(tree);

    print_tree(tree);
  }

  static void test_duplicate_inserts() {

    std::cout << "\n[TEST] DUPLICATE INSERTS\n";

    RedBlackTree tree;

    tree.insertion(10);
    tree.insertion(10);
    tree.insertion(10);

    validate_tree(tree);

    print_tree(tree);
  }

  // ==========================================================
  // RUN ALL
  // ==========================================================

  static void run_all() {

    test_single_insert_delete();

    test_sequential_insert();

    test_sequential_delete();

    test_reverse_delete();

    test_random_insert_delete();

    // test_root_deletes();

    // test_alternating_operations();

    // test_zig_zag_case();

    // test_duplicate_inserts();

    std::cout << GREEN << "\nALL TESTS PASSED\n" << RESET;
  }
};

// ============================================================
// MAIN
// ============================================================

int main() {

  RBTreeTester::run_all();

  return 0;
}
