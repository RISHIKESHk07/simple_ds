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
  enum Direction { LEFT, RIGHT };
  typedef struct node {
    node *parent = nullptr;
    struct {
      struct node *left;
      struct node *right;
    };
    struct node *child[2];
    int key;
    Color color;
  } node;

  node *tree;
  Direction return_dir_opposite(Direction dir) {
    if (dir == Direction::RIGHT)
      return Direction::LEFT;
    else
      return Direction::RIGHT;
  }
  node *subtree_rotation(node *tree, node *sub, Direction dir) {
    auto new_root = sub->child[1 - dir];
    auto old_root_parent = sub->parent;
    auto new_child = new_root->child[dir];

    sub->child[1 - dir] = new_child;
    new_child->parent = sub;

    new_root->child[dir] = sub;
    sub->parent = new_root;

    new_root->parent = old_root_parent;

    if (old_root_parent) {
      old_root_parent->child[sub == old_root_parent->right] = new_root;
    } else {
      tree = new_root;
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
      ins_parent.second->child[0] = ins_node;
    else
      ins_parent.second->child[1] = ins_node;

    // RB violations checker and while loop
    auto X = ins_parent.second;
    auto N = ins_node;
    do {
      Direction dir =
          (X->parent->right == X) ? Direction::RIGHT : Direction::LEFT;
      auto G = X->parent;
      auto U = X->parent->child[1 - dir];
      // case 1; black parent so we are done
      if (X->color == Color::black) {
        return;
      } else {

        if (G == nullptr) {
          // root case
          X->color = Color::black;
          return;
        }

        if (!U && G->color == Color::black && U->color == Color::black) {
          if (G->child[dir] == X && X->child[1 - dir] == N) {
            auto new_root = subtree_rotation(tree, X, dir);
          }
          auto new_root2 = subtree_rotation(tree, G, return_dir_opposite(dir));
          new_root2->color = Color::black;
          new_root2->child[1 - dir]->color = Color::red;
          X = new_root2;

          return;
        }

        if (G->color == Color::black && U->color == Color::red) {

          // color shift and now we can have violations above this point in
          // parent of grandparent so we need to fix it ... look up for code for
          // the flow of this
          G->color = Color::red;
          X->color = Color::black;
          U->color = Color::black;
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
  void deletion(int k) {
    // normal deletion here ...
    auto n = search(tree, k);
    if (!n.first) {
      return;
    }
    auto pod = sucessor(k);
    auto rl = (pod->parent->right == pod) ? 1 : 0;
    auto t = pod->parent;
    pod->parent = nullptr;
    n.first->key = pod->key;
    if (pod->color == Color::red) {
      if (pod->right) {
        if (rl) {
          t->right = pod->right;
        }
        t->left = pod->right;

      } else {
        if (rl) {
          t->right = nullptr;
        }
        t->left = nullptr;
      }
      pod->right = nullptr;
      return;
    }
    // deletion node is balck in color
    if (pod->right) {
      if (rl) {
        t->right = pod->right;
      }
      t->left = pod->right;
      pod->right->color = Color::black;
      pod->right = nullptr;
      return;
    }
    // now the case where we have to delete a black node with no children
    else {
      // RB tree fixing should be done ... Right all 12 cases here remove
      // unecessary cases and then invesigate for simialr cases in all of them
      // and group them , and then you see a pattern where some cases can led by
      // moving through others , use the wiki page for table of structural
      // changes ...

      Direction dir = rl ? Direction::RIGHT : Direction::LEFT;
      auto X = pod->parent;
      auto N = X->child[dir];
      do {
        auto S = X->child[1 - dir];
        auto C = S->child[dir];
        auto D = S->child[1 - dir];
        if (X->parent == nullptr) {
          // change in color required at all
          return;
        } else {
          // case #2
          if (X->color == Color::black && S->color == Color::black &&
              C->color == Color::black && D->color == Color::black) {
            S->color = Color::red;
          }
          // case #3
          if (X->color == Color::black && S->color == Color::red &&
              C->color == Color::black && D->color == Color::black) {
            auto new_root = subtree_rotation(tree, S, dir);
            new_root->color = Color::black;
            new_root->child[dir]->color = Color::red;

            if (S->color == Color::black && C->color == Color::red &&
                D->color == Color::black) {
              // case 5
              auto new_root =
                  subtree_rotation(tree, C, return_dir_opposite(dir));
              new_root->color = Color::black;
              new_root->child[1 - dir]->color = Color::red;
            }

            if (S->color == Color::black && D->color == Color::red) {
              // case 6
              auto new_root2 = subtree_rotation(tree, X, dir);
              new_root2->child[dir]->color = Color::black;
              new_root2->child[1 - dir]->color = Color::black;
            }

            if (X->color == Color::red && S->color == Color::black &&
                C->color == Color::black && D->color == Color::black) {
              X->color = Color::black;
              S->color = Color::red;
              return;
            }
          }
          if (S->color == Color::black && C->color == Color::red &&
              D->color == Color::black) {
            // case 5
            goto case_5;
          }

          if (S->color == Color::black && D->color == Color::red) {
            // case 6
            goto case_6;
          }

          if (X->color == Color::red && S->color == Color::black &&
              C->color == Color::black && D->color == Color::black) {
            // case 4
            X->color = Color::black;
            S->color = Color::red;
            return;
          }
        }

        N = X;
        X = X->parent;
        dir = (X->parent->right == X) ? Direction::RIGHT : Direction::LEFT;

      } while (X != nullptr);
    case_5:
      auto new_root = subtree_rotation(tree, X->child[1-dir]->child[dir], return_dir_opposite(dir));
      new_root->color = Color::black;
      new_root->child[1 - dir]->color = Color::red;
    case_6:
      auto new_root2 = subtree_rotation(tree, X, dir);
      new_root2->child[dir]->color = Color::black;
      new_root2->child[1 - dir]->color = Color::black;
      return;
    }
  }
};
int main() { return 0; }
