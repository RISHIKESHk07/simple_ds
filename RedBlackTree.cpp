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
  // if black proceed forward nothing else matters ... cannot break balance *case 1*,
  // if red node then ,
  // P->R G->B U->R  --> P & U ->black G->red *case 2* , issue this could make G 's parent unstable meaning we need to check the roots above to make sure it fine ,
  // if P is red and G is root ---> P is black (check this once) 
  // if P is red and root ---> P is black *case4*
  // P->R G->B U->B --> P is rotated left and then right , P->B & N->R & G->R
  // p->R G->B U->B outer -> P is new root by rotaing right and then p->B N->R & G->R
    
  node *insertion(node *tree, int k) {
  }
};
int main() { return 0; }
