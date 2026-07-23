#pragma once
#include "../Data_structures/AVLTree.hpp"
#include "../Data_structures/BTree.hpp"
#include <cstddef>
#include <memory>
#include <optional>

// We need to wrap all the different types of memtable into a combination of two
// strcutures , indexes whch use the underlying tree to the exact ptr of
// universal object we generate , this object will have a internal data
// structure based on the type to store the payload it self .
enum class Memtable_tree_interfaces { AVL, B, Bplus, RB };
template <typename K, typename V> class Base_adapter {
public:
  virtual ~Base_adapter() = default;
  virtual std::optional<std::pair<K, V>> find(const K &key) = 0;
  virtual bool insert_node(const K &key, const V &value) = 0;
  virtual bool delete_node(const K &key) = 0;
  virtual std::vector<std::pair<K, V>> range_scan(const K &low,
                                                  const K &high) = 0;
  virtual size_t size() = 0;
};

template <typename K, typename V, typename Comparator>
class B_interface : public Base_adapter<K, V> {
private:
  BTree<K, V, Comparator> tree;

public:
  bool insert_node(const K &key, const V &value) override {
    tree.insertion(key, value);
    return true;
  }

  bool delete_node(const K &key) override {
    tree.deletion(key);
    return true;
  }

  std::optional<V> look_up(const K &key) override {
    auto result = tree.search(key, tree.tree);

    if (result.first == nullptr)
      return std::nullopt;

    return result.first->val;
  }

  std::vector<std::pair<K, V>> range_scan(const K &begin,
                                          const K &end) override {
    return tree.inorder_range_traversal(begin, end);
  }

  size_t size() override { return tree.size_of_tree(); }

  void print() { tree.printTree(); }
};

template <typename K, typename V, typename Comparator>
class AVL_interface : public Base_adapter<K, V> {
private:
  // we will use avl tree for DBkey index mapping to a wrap_object
  AVLtree<K, V, Comparator> tree;

public:
  bool insert_node(const K &key, const V &value) override {
    tree.insertion(key, value);
    return true;
  }

  bool delete_node(const K &key) override {
    tree.deletion(key);
    return true;
  }

  std::optional<V> look_up(const K &key) override {
    auto result = tree.search(key, tree.tree);

    if (result.first == nullptr)
      return std::nullopt;

    return result.first->val;
  }

  std::vector<std::pair<K, V>> range_scan(const K &begin,
                                          const K &end) override {
    return tree.inorder_range_traversal(begin, end);
  }

  size_t size() override { return tree.size_of_tree(); }

  void print() { tree.printTree(); }
};
template <Memtable_tree_interfaces T, typename K, typename V, typename C>
class MemtableFactory {
  // maps exact internal index tree type here ...
public:
  static std::unique_ptr<Base_adapter<K, V>> create_memtable() {
    if constexpr (T == Memtable_tree_interfaces::AVL) {
      return std::make_unique<AVL_interface<K, V, C>>();
    }
  }
};
template <Memtable_tree_interfaces T, typename K, typename V, typename C>
class Memtable {
  // here we will generate the universal data object for wrapping incoming set
  // and get for string object , sorted set , dictionary(map) , so based on
  // incoming cmd we will generate a corresponding object
private:
  std::unique_ptr<Base_adapter<K, V>> index;

public:
  Memtable() { index = MemtableFactory<T, K, V, C>::create_memtable(); }

  bool insert(const K &key, const V &value) {
    return index->insert_node(key, value);
  }

  bool erase(const K &key) { return index->delete_node(key); }

  std::pair<K, V> find(const K &key) { return index->look_up(key); }

  std::vector<std::pair<K, V>> scan(const K &start, const K &end) {
    return index->range_scan(start, end);
  }

  size_t size() { return index->size(); }
};
