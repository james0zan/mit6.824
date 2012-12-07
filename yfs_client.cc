// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

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


int yfs_client::read(inum ino, std::string &s) {
  return ec->get(ino, s);
}

int yfs_client::write(inum ino, std::string s) {
  return ec->put(ino, s);
}

int yfs_client::remove(inum ino) {
  return ec->remove(ino);
}

int yfs_client::lookup(inum parent, std::string name, inum &ino) {
  if(!isdir(parent)) return IOERR;

  std::string tmp;
  int ret = read(parent, tmp);
  if( ret != OK ) return ret;

  file_list pdir(tmp);
  if(pdir.in_dir(name, ino)) return EXIST;
  else return NOENT;
}

int yfs_client::create(inum parent, std::string name, inum &ino, bool dir) {
  if(!isdir(parent)) return IOERR;

  std::string tmp;
  int ret = read(parent, tmp);
  if( ret != OK ) return ret;

  file_list pdir(tmp);
  if(pdir.in_dir(name, ino)) return EXIST;

  //TODO: while 1 check getattr == NOENT
  if(dir) ino = (rand() & 0x7FFFFFFF);
  else ino = (rand() & 0xFFFFFFFF) | 0x80000000;
  
  ret = write(ino, "");
  if(ret != OK) return ret;
  ret = write(parent, pdir.add_file(name, ino));
  return ret;
}

int yfs_client::readdir(inum dir, std::vector<dirent> &entries) {
  if(!isdir(dir)) return NOENT;

  std::string tmp;
  int ret = read(dir, tmp);
  if(ret != OK) return ret;

  file_list pdir(tmp);
  pdir.get_entries(entries);
  return OK;
}
