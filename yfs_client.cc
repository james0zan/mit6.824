// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "lock_client_cache.h"

struct myScopeLock {
  lock_client *lc;
  yfs_client::inum ino;

  myScopeLock(lock_client *lc_, yfs_client::inum ino_) {
    lc = lc_; ino = ino_;
    lc->acquire(ino);
  }
  ~ myScopeLock() {
    lc->release(ino);
  }
};

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst, new my_lock_release_user(ec));
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  myScopeLock L(lc, inum);
  // You modify this function for Lab 3
  // - hold and release the file lock

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  myScopeLock L(lc, inum);
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}


int yfs_client::get(inum ino, std::string &s) {
  return ec->get(ino, s);
}

int yfs_client::put(inum ino, std::string s) {
  return ec->put(ino, s);
}

int yfs_client::remove(inum ino) {
  return ec->remove(ino);
}

int yfs_client::lookup(inum parent, std::string name, inum &ino) {
  if(!isdir(parent)) return IOERR;
  myScopeLock L(lc, parent);
  std::string tmp;
  int ret = get(parent, tmp);
  if( ret != OK ) return ret;

  file_list pdir(tmp);
  if(pdir.in_dir(name, ino)) return EXIST;
  else return NOENT;
}

int yfs_client::create(inum parent, std::string name, inum &ino, bool dir) {
  if(!isdir(parent)) return IOERR;
  myScopeLock L(lc, parent);
  std::string tmp;
  int ret = get(parent, tmp);
  if( ret != OK ) return ret;

  file_list pdir(tmp);
  if(pdir.in_dir(name, ino)) return EXIST;

  //TODO: while 1 check getattr == NOENT
  if(dir) ino = (rand() & 0x7FFFFFFF);
  else ino = (rand() & 0xFFFFFFFF) | 0x80000000;
  myScopeLock L2(lc, ino);
  ret = put(ino, "");
  if(ret != OK) return ret;
  ret = put(parent, pdir.add_file(name, ino));
  return ret;
}

int yfs_client::readdir(inum dir, std::vector<dirent> &entries) {
  if(!isdir(dir)) return IOERR;
  myScopeLock L2(lc, dir);
  std::string tmp;
  int ret = get(dir, tmp);
  if(ret != OK) return ret;

  file_list pdir(tmp);
  pdir.get_entries(entries);
  return OK;
}

int yfs_client::resize(inum ino, unsigned long long size) {
  if(!isfile(ino)) return IOERR;
  myScopeLock L(lc, ino);
  std::string s;
  int ret = get(ino, s);
  if(ret != OK) return ret;

  s.resize(size);
  return put(ino, s);
}

int yfs_client::write(inum ino, std::string s, off_t off) {
  if(!isfile(ino)) return IOERR;
  myScopeLock L(lc, ino);
  //printf("write %llu %s\n", ino, s.c_str());
  std::string file;
  int ret = get(ino, file);
  if(ret != OK) return ret;
  //printf("write get %llu %s\n", ino, file.c_str());
  if(off >= file.size()) file.resize(off);
  file.replace(off, s.size(), s);
  ret = put(ino, file);
  //printf("write done %llu %d\n", ino, ret);
  return ret;
}

int yfs_client::read(inum ino, std::string &s, off_t off, size_t &size) {
  if(!isfile(ino)) return IOERR;
  myScopeLock L(lc, ino);
  int ret = get(ino, s);
  if(ret != OK) return ret;

  if(off >= s.size()) s = "";
  else s = s.substr(off, size);
  size = s.size();
  return OK;
}

int yfs_client::unlink(inum parent, std::string s) {
  if(!isdir(parent)) return IOERR;
  myScopeLock L(lc, parent);
  std::string tmp;
  int ret = get(parent, tmp); 
  //printf("$$GET %llu %d %s$$\n", parent, ret, s.c_str());

  if(ret != OK) return ret;
  
  file_list pdir(tmp); inum ino;
  //printf("$$in_dir: %d\n", pdir.in_dir(s, ino));
  if(!pdir.in_dir(s, ino)) return NOENT;

  myScopeLock L2(lc, ino);
  ret = remove(ino);
  //printf("$$ret: %d\n", ret);
  if(ret != OK) return ret;
  return put(parent, pdir.remove_file(s));
}
