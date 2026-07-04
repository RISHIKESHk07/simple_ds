#pragma once
#include "../Data_structures/AVLTree.hpp"
#include "../Data_structures/BTree.hpp"
#include "IN-MEM_Helper.hpp"
// We need to wrap all the different types of memtable into a combination of two
// strcutures , indexes whch use the underlying tree to the exact ptr of
// universal object we generate , this object will have a internal data
// structure based on the type to store the payload it self .
enum class Memtable_tree_interfaces { AVL, B, Bplus, RB };

template <typename KeyType, typename ValueType> class Base_adapter {
public:
  virtual std::pair<KeyType, ValueType> look_up();
  virtual bool insert_node();
  virtual bool delete_node();
  virtual std::vector<std::pair<KeyType, ValueType>> range_scan();
};

template <typename KeyType, typename ValueType, typename Comparator>
class B_interface : public Base_adapter<KeyType, ValueType> {
  std::pair<KeyType, ValueType> look_up() override;
  bool insert_node() override;
  bool delete_node() override;
};

template <typename KeyType, typename ValueType, typename Comparator>
class AVL_interface : public Base_adapter<KeyType, ValueType> {
private:
  // we will use avl tree for DBkey index mapping to a wrap_object
  AVLtree<KeyType, ValueType, Comparator> index_map;

public:
  std::pair<KeyType, ValueType> look_up() override {};
  bool insert_node() override;
  bool delete_node() override;
  bool size();
  std::vector<std::pair<KeyType, ValueType>> range_scan() override;
};
template <Memtable_tree_interfaces T, typename K, typename V, typename C>
class MemtableFactory {
  // maps exact internal index tree type here ...
public:
  Base_adapter<K, V> create_memtable() {
    if (T == Memtable_tree_interfaces::AVL) {
      AVL_interface<K, V, C> AVL_obj;
      return AVL_obj;
    }
  }
};
template <Memtable_tree_interfaces T, typename KeyType, typename ValueType,
          typename Comparator>
class Memtable {
  // here we will generate the universal data object for wrapping incoming set
  // and get for string object , sorted set , dictionary(map) , so based on
  // incoming cmd we will generate a corresponding object
private:
  MemtableFactory<T, KeyType, ValueType, Comparator> mf;

public:
  Memtable() { std::cout << "Created memtable .. " << std::endl; }
  Memtable get_Memtable_object() { return mf.create_memtable(); };
  int delete_Memtable(); // params: key
  int insert_Memtable(); // params: key & value
  int point_lookup();    // params: key
  int range_scan();      // params: array of keys
};
