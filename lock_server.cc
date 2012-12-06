// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  possessed.clear();
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status 
lock_server::acquire(int clt, lock_protocol::lockid_t lid, lock_protocol::status &r) {
#ifdef _DEBUG
	printf("[log] %d acquire %d\n", clt, (int)lid);
#endif

	pthread_mutex_lock(&mutex);
	for (;;) {
  		std::map<lock_protocol::lockid_t, int>::iterator tmp = possessed.find(lid);
  		if (tmp == possessed.end()) {
  			possessed[lid] = clt;
  			break;
  		}
  		pthread_cond_wait(&cond, &mutex);
	}
	pthread_mutex_unlock(&mutex);

#ifdef _DEBUG
	printf("[log] %d acquire %d done\n", clt, (int)lid);
#endif
	return lock_protocol::OK;
}

lock_protocol::status 
lock_server::release(int clt, lock_protocol::lockid_t lid, lock_protocol::status &r) {
#ifdef _DEBUG
	printf("[log] %d release %d\n", clt, (int)lid);
#endif
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, int>::iterator tmp = possessed.find(lid);
  
	if (tmp == possessed.end() || tmp->second != clt) {
		pthread_mutex_unlock(&mutex);
		//this lock is not possessed by this client
		return lock_protocol::NOENT; 
	}

	possessed.erase(tmp);
	pthread_mutex_unlock(&mutex);
	pthread_cond_broadcast(&cond);

#ifdef _DEBUG
	printf("[log] %d release %d done\n", clt, (int)lid);
#endif
	return lock_protocol::OK;
}
