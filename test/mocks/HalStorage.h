#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#define O_RDONLY 0x00
#define O_WRITE 0x02
#define O_CREAT 0x04
#define O_APPEND 0x08
#define O_TRUNC 0x10

// No-op file mock: every operation is safe but reports "nothing here" (no
// files exist in the native mock filesystem). Real firmware code names this
// type `FsFile` in most call sites and `HalFile` in a few newer ones (the
// real lib/hal/HalStorage.h aliases `using FsFile = HalFile;`) — both names
// must resolve to this same mock class.
class FsFile {
 public:
  operator bool() const { return false; }
  bool isOpen() const { return false; }
  bool isDirectory() const { return false; }
  void close() {}
  bool seek(size_t) { return false; }
  bool seekCur(int64_t) { return false; }
  bool seekSet(size_t) { return false; }
  size_t position() const { return 0; }
  size_t getName(char* buf, size_t len) { if (buf && len) buf[0] = '\0'; return 0; }
  void getName(char* buf, int len) { if (buf && len) buf[0] = '\0'; }
  int read(void*, size_t) { return 0; }
  int read() { return -1; }
  size_t write(const void*, size_t) { return 0; }
  size_t write(uint8_t) { return 0; }
  void flush() {}
  void println(const char*) {}
  void println(const std::string& s) { println(s.c_str()); }
  void print(const char*) {}
  bool available() const { return false; }
  size_t size() { return 0; }
  size_t fileSize() { return 0; }
  bool rename(const char*) { return false; }
  void rewindDirectory() {}
  FsFile openNextFile() { return FsFile(); }
};
using HalFile = FsFile;

struct StorageMock {
  void mkdir(const char*) {}
  bool exists(const char*) { return false; }
  FsFile open(const char*, int = 0) { return FsFile(); }
  bool remove(const char*) { return false; }
  bool removeDir(const char*) { return false; }
  bool rmdir(const char*) { return false; }
  bool rename(const char*, const char*) { return false; }
  bool openFileForRead(const char*, const std::string&, FsFile&) { return false; }
  bool openFileForWrite(const char*, const std::string&, FsFile&) { return false; }
  void writeFile(const char*, const std::string&) {}
};

inline StorageMock Storage;
