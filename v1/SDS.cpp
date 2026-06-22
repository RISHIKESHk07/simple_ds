#include <iostream>
/*
V1:
Control plane , maintains the parsing the RESP protocol and a yaml config , for
provising the cores we need to use and sharding partition will be done based on
this , and incoming writes and reads are sent accordingly , writes and reads are
routed over here , and any form of range lookup need to be circumvented using
SPSC queues here . Persistance uses a WAL file per core and LSM per core ,
remember for replicas instead of compaction we will generate copies per core ,
not the system as a whole .
*/

class ControlPlane {
public:
  struct REQUEST_OBJECT {
    // type
    // seqNum
    // key
    // payload?
    // bool range_lookup
  };
  struct CONFIG_OBJECT {
    int num_cores;
    bool replication;
    std::string_view replication_url;
    int port_assigned;
    std::string_view master_host;
    int SIZE_LIMIT_PER_CORE;
    // LSM CONSTRAINTS ...
    //...
  };
  class CONFIG_PARSER {
  public:
    CONFIG_OBJECT
    parsing_yaml_config(
        std::string url); // no params , uses a config file in
                          // root for accessing data .. , automatically privsion
                          // based on config file , if hardware not avaible use
                          // default 1 core based solution
  };

  class RESP_PARSER {
  public:
    REQUEST_OBJECT
    parsing_processor(); // params: asio Buffers or string_view format , convert
                         // incoming requests from network / socket object into
                         // serlized data
  };

private:
  CONFIG_PARSER conf_p;
  RESP_PARSER res_p;
  std::shared_ptr<CONFIG_OBJECT> conf_obj;
  ControlPlane(std::string &config_url) {
    std::cout << "Control Plane starting ..." << std::endl;
    // check if file exists and also check for any issue during parsing the file
    // .. corruption etc
    conf_obj =
        std::make_shared<CONFIG_OBJECT>(conf_p.parsing_yaml_config(config_url));
  };
};

int main() { std::cout << "hello their !!!" << std::endl; }
