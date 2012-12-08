// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&mutex, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{
  std::string holder; int ret; bool revoked =false;
  {
    ScopedLock L(&mutex);
    //build reverse rpc
    if(!clients.count(id)) {
      sockaddr_in dstsock;
      make_sockaddr(id.c_str(), &dstsock);
      rpcc *cl = new rpcc(dstsock);
      if(cl->bind() < 0) return lock_protocol::IOERR;
      clients[id] = cl;
    }
    
    std::map<lock_protocol::lockid_t, lock>::iterator tmp = possessed.find(lid);
    //no one possessed this lock
    if (tmp == possessed.end() || tmp->second.used == false) {
      possessed[lid].used = true;
      possessed[lid].x = id;
      ret = lock_protocol::OK;
      possessed[lid].waited.erase(id);
      if (possessed[lid].waited.size()) {
        revoked = true;
        holder = id;
      }
    } else {
      possessed[lid].waited.insert(id);
      holder = possessed[lid].x;
      ret = lock_protocol::RETRY;
      revoked = true;
    }
  }
  if (revoked) {
    rlock_protocol::status rr = clients[holder]->call(rlock_protocol::revoke, lid, r);
    //TODO
    //VERIFY(rr == rlock_protocol::OK);
  }
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  std::string next_client;
  {
	  ScopedLock L(&mutex);
	  std::map<lock_protocol::lockid_t, lock>::iterator tmp = possessed.find(lid);
	  
	  //this lock is not possessed by this client
    if (tmp == possessed.end() || tmp->second.used == false || tmp->second.x != id) 
      return lock_protocol::NOENT; 

    tmp->second.used = false;
    tmp->second.waited.erase(id);
	  //no one is waiting for this lock
    if (tmp->second.waited.empty()) {
      return lock_protocol::OK;
	  }

	  next_client = *(tmp->second.waited.begin());
	  tmp->second.waited.erase(tmp->second.waited.begin());
  }

  rlock_protocol::status rr = clients[next_client]->call(rlock_protocol::retry, lid, r);
  VERIFY(rr == rlock_protocol::OK);
  return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

