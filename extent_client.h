// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include "lock_client_cache.h"

class extent_client {
 private:
  rpcc *cl;

  struct file {
    std::string buf;
    extent_protocol::attr attr;
    bool dirty;
  };  
  pthread_mutex_t lock;
  std::map<extent_protocol::extentid_t, file> cache; 

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status flush(extent_protocol::extentid_t eid);
};

class my_lock_release_user : public lock_release_user {
	private:
  		extent_client* ec;
	public:
  		my_lock_release_user(extent_client* ec) : ec(ec) { }
  		void dorelease(lock_protocol::lockid_t eid) {
  			ec->flush(eid);
  		}
};

#endif 

