// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();

  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&mutex);

  //initialize lock
  if(!cache.count(lid)) {
    cache[lid].stat = NONE;
    //pthread_cond_init(&(cache[lid].cond), NULL);
    sem_init(&(cache[lid].cond), 0, 0);
  }

  for (;;) {
    lock& tmp = cache[lid];
    if (tmp.stat == FREE) {
      //printf("Lock %llu from FREE to LOCKED by %lu\n", lid, pthread_self());
      tmp.stat = LOCKED;
      tmp.id = pthread_self();
      break;
    } else if (tmp.stat == NONE) {
      //printf("Lock %llu before ACQUIRE by %lu %s\n", lid, pthread_self(), id.c_str());
      pthread_mutex_unlock(&mutex);
      int r = cl->call(lock_protocol::acquire, lid, id, r);
      pthread_mutex_lock(&mutex);

      if (r == lock_protocol::OK) {
        //printf("Lock %llu from NONE to LOCKED by %lu\n", lid, pthread_self());
        tmp.stat = LOCKED;   
        tmp.id = pthread_self(); 
        break;
      } else if (r == lock_protocol::RETRY) { 
        //printf("Lock %llu before RETRY by %lu %s\n", lid, pthread_self(), id.c_str());
        pthread_mutex_unlock(&mutex);
        sem_wait(&tmp.cond);
        pthread_mutex_lock(&mutex);
        //pthread_cond_wait(&tmp.cond, &mutex);
        //printf("Lock %llu after RETRY by %lu\n", lid, pthread_self());
      } else {
        VERIFY(0);
      }
    } else {
      //tmp.stat = REVOKED;
      //printf("Lock %llu before REVOKED by %lu\n", lid, pthread_self());
      //pthread_cond_wait(&tmp.cond, &mutex);
      pthread_mutex_unlock(&mutex);
      sem_wait(&tmp.cond);
      pthread_mutex_lock(&mutex);
      //printf("Lock %llu after REVOKED by %lu\n", lid, pthread_self());
    }
  }
  pthread_mutex_unlock(&mutex);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, lock>::iterator tmp = cache.find(lid);
    
  //this lock is not possessed by this thread
  if (tmp == cache.end() || 
      tmp->second.stat != LOCKED ||
      tmp->second.id != pthread_self()) {
    pthread_mutex_unlock(&mutex);
    return rlock_protocol::RPCERR; 
  } 
  //printf("%lu released %llu %d\n", pthread_self(), lid, tmp->second.revoked);
  if (tmp->second.revoked) {
    pthread_mutex_unlock(&mutex);

    int r = cl->call(lock_protocol::release, lid, id, r);
    if(r != lock_protocol::OK) return r;

    pthread_mutex_lock(&mutex);
    tmp->second.stat = NONE;
  } else tmp->second.stat = FREE;

  // this lock can be locked again
  //printf("Lock %llu broadcast 3 %s\n", lid, id.c_str());
  //pthread_cond_broadcast(&(tmp->second.cond));
  sem_post(&(tmp->second.cond)); 
  pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, lock>::iterator tmp = cache.find(lid);
    
  //this lock is not possessed by this thread
  /*if (tmp == cache.end()) {
    pthread_mutex_unlock(&mutex);
    printf("HERE: Lock %llu stat %d\n", lid, tmp->second.stat);
    return rlock_protocol::RPCERR; 
  } */

  if (tmp->second.stat == FREE) {
    pthread_mutex_unlock(&mutex);

    int r = cl->call(lock_protocol::release, lid, id, r);
    if(r != lock_protocol::OK) return r;

    pthread_mutex_lock(&mutex);
    tmp->second.stat = NONE;
    //TODO
    //printf("Lock %llu broadcast 1 %s\n", lid, id.c_str());
    //pthread_cond_broadcast(&(tmp->second.cond));  
    sem_post(&(tmp->second.cond)); 
  } else tmp->second.revoked = true;
  pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  ScopedLock L(&mutex);
  std::map<lock_protocol::lockid_t, lock>::iterator tmp = cache.find(lid);
  if (tmp == cache.end()) return rlock_protocol::RPCERR;

  //printf("Lock %llu broadcast 2 %s\n", lid, id.c_str());
  //pthread_cond_broadcast(&(tmp->second.cond));
  sem_post(&(tmp->second.cond)); 
  return rlock_protocol::OK;
}



