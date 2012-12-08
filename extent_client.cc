// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  pthread_mutex_init(&lock, NULL);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  ScopedLock m(&lock);

  std::map<extent_protocol::extentid_t, file>::iterator tmp = cache.find(eid);
  if(tmp != cache.end()) {
    buf = tmp->second.buf;
    tmp->second.attr.atime = time(NULL);
  } else {
    ret = cl->call(extent_protocol::get, eid, buf);
    if (ret == extent_protocol::OK) {
      cache[eid].buf = buf;
      cache[eid].dirty = false;
      cl->call(extent_protocol::getattr, eid, cache[eid].attr);
    }
  }
  printf("get: %llu %d %s\n", eid, tmp == cache.end(), buf.c_str());
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;

  ScopedLock m(&lock);

  std::map<extent_protocol::extentid_t, file>::iterator tmp = cache.find(eid);
  if(tmp != cache.end()) {
    attr = tmp->second.attr;
  } else {
    ret = cl->call(extent_protocol::getattr, eid, attr);
    if (ret == extent_protocol::OK) {
      cache[eid].attr = attr;
      cache[eid].dirty = false;
      cl->call(extent_protocol::get, eid, cache[eid].buf);
    }
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  ScopedLock m(&lock);
  file &tmp = cache[eid];
  tmp.buf = buf;
  tmp.dirty = true;

  unsigned int t = time(NULL);
  tmp.attr.ctime = t; tmp.attr.mtime = t;
  tmp.attr.size = buf.size();
  return extent_protocol::OK;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  ScopedLock m(&lock);
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  //TODO
  cl->call(extent_protocol::remove, eid, r);
  cache.erase(eid);
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  ScopedLock m(&lock);
  if(cache.count(eid) && cache[eid].dirty)
    ret = cl->call(extent_protocol::put, eid, cache[eid].buf, ret);

  cache.erase(eid);
  return ret;
}
