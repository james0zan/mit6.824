#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

  struct file_list {
    std::string &x;
    file_list(std::string &x_) :x(x_){}

    bool in_dir(std::string file, inum &ino) {
      const char *f = file.c_str(), *d = x.c_str();
      while(*d) {
        if(strcmp(f, d) == 0 ) { // check if filename matches
          d += strlen(d) + 1;
          ino = n2i(std::string(d));
          return  true;
        }
        d += strlen(d) + 1; // skip filename
        d += strlen(d) + 1; // skip inum
      }
      return false;
    }

    std::string add_file(std::string file, inum ino) {
      std::ostringstream ost;
      ost << file << std::string(1, NULL) << ino << std::string(1, NULL) << x;
      return ost.str();
    }

    void get_entries(std::vector<dirent> &entries) {
      dirent entry;
      for(const char *d = x.c_str(); *d;) {
        entry.name = std::string(d);
        d += strlen(d) + 1; // skip filename
        entry.inum = n2i(std::string(d));
        d += strlen(d) + 1; // skip inum
        entries.push_back(entry);
      }
    }
  };
 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int read(inum ino, std::string &s);
  int write(inum ino, std::string s);
  int remove(inum ino);
  
  int create(inum parent, std::string name, inum &ino, bool dir = false);
  int lookup(inum parent, std::string name, inum &ino);
  int readdir(inum dir, std::vector<dirent> &entries);
};

#endif 
