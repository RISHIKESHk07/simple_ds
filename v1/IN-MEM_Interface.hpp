#include "../Data_structures/AVLTree.hpp"
#include "../Data_structures/BTree.hpp"
#include "IN-MEM_Helper.hpp"
// We need to wrap all the different types of memtable into a combination of two
// strcutures , indexes whch use the underlying tree to the exact ptr of
// universal object we generate , this object will have a internal data
// structure based on the type to store the payload it self .

class B_interface : public BTree {};

class AVL_interface {
  // we will use avl tree for DBkey index mapping to a wrap_object
  void look_up();
  void insert_node();
  void delete_node();
  void range_scan();
};
class MemtbaleFactory {
  // maps exact internal index tree type here ...
};
class Memtable {

  // here we will generate the universal data object for wrapping incoming set
  // and get for string object , sorted set , dictionary(map) , so based on
  // incoming cmd we will generate a corresponding object
};
