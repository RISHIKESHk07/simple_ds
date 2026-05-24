#include <algorithm>
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
        return {i, parent};
      if (tree->keys[i] < k) {
        index_res = i;
      }
    }
    if (index_res == -1) {
      return search(tree->vnode[0], k, tree);

    } else if (index_res == tree->keys.size()) {
      return search(tree->vnode[tree->vnode.size() - 1], k, tree);
    }
    return search(tree->vnode[index_res + 1], k, tree);
  };
  void rearrange_root_on_insertion(BNode *tr, int k) {
    int cur_node_size = tr->keys.size();
    if (cur_node_size + 1 <= order) {
      // search correct position here for this insertion
      
      // update keys vector
      // update keys vnode as well
    } else {
    }
  }
  void insertion(int k) {
    auto search_pos = search(tree, k);
    if (search_pos.first >= 0)
      return;
  };
  void deletion(int k);
};

int main() { return 0; }
