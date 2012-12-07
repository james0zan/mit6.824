// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  pthread_mutex_init(&lock, NULL);
  disk.clear();
  int i; put(1, "", i); // create the root
}

#ifdef _DEBUG
std::string output(std::string x) {
  for (size_t i=0; i<x.length(); i++) if (x[i] == 0) x[i] = ' ';
  return x;
}
#endif

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  ScopedLock m(&lock);
#ifdef _DEBUG
  printf("put %llu |%s|\n", id, output(buf).c_str());
#endif
  file &ex = disk[id];
  ex.str = buf;
  
  unsigned int t = time(NULL);
  ex.attr.ctime = t; ex.attr.mtime = t;
  ex.attr.size = buf.size();
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  ScopedLock m(&lock);
#ifdef _DEBUG
  printf("get %llu\n", id);
#endif
  std::map<extent_protocol::extentid_t, file>::iterator it = disk.find(id);
  if (it == disk.end()) return extent_protocol::NOENT;
  buf = it->second.str;
#ifdef _DEBUG
  printf("get %llu |%s|\n", id, output(buf).c_str());
#endif
  unsigned int t = time(NULL);
  it->second.attr.atime = t;
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  ScopedLock m(&lock);
  std::map<extent_protocol::extentid_t, file>::iterator it = disk.find(id);
  if (it == disk.end()) return extent_protocol::NOENT;
  a = it->second.attr;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  ScopedLock m(&lock);
  std::map<extent_protocol::extentid_t, file>::iterator it = disk.find(id);
  if (it == disk.end()) return extent_protocol::NOENT;
  disk.erase(it);
  return extent_protocol::OK;
}

