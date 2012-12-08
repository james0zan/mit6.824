#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;

  std::map<std::string, rpcc*> clients;
  struct lock {
    bool used;
    std::string x;
    std::set<std::string> waited;
  };
  std::map<lock_protocol::lockid_t, lock> possessed;
  pthread_mutex_t mutex;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
  int release2(lock_protocol::lockid_t lid);
};

#endif
