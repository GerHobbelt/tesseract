
#pragma once

#include <stdio.h>
#include <memory>

namespace tesseract
{
typedef decltype(std::fclose) fclose_f;
using __file_ptr = std::shared_ptr<FILE>;

class FilePtr final : public __file_ptr {
private:
  // Second argument is the deleter function: a pointer to the type of
  // std::fclose.
  //
  // Use shared_ptr instead of unique_ptr so we can pass variables (instances)
  // of type FilePtr as function parameters, where they would otherwise have
  // std::move()d the embedded file handle if we had used a unique_ptr here; now
  // these "shadows" will be properly reference-counted instead.
  //
  // Regarding run-time costs: 
  // - https://stackoverflow.com/questions/15129263/is-there-a-non-atomic-equivalent-of-stdshared-ptr-and-why-isnt-there-one-in
  // - https://stackoverflow.com/questions/41871115/why-would-i-stdmove-an-stdshared-ptr

public:
  FilePtr() : __file_ptr(nullptr, deleter) {}
  FilePtr(FILE *handle) : __file_ptr(handle, deleter) {}

  ~FilePtr() = default;

  //FilePtr(FilePtr &h) = delete;
  //FilePtr(const FilePtr &h) = delete;

protected:
  static void deleter(FILE *f) {
    if (f) {
      std::fclose(f);
    }
  }
};


// FileHandle is like FilePtr, but requires us to pass it by reference only, never by value.

using __file_handle = std::unique_ptr<FILE, void(*)(FILE*)>;

class FileHandle final : public __file_handle {
  // Second argument is the deleter function: a pointer to the type of
  // std::fclose.
public:
  FileHandle() : __file_handle(nullptr, deleter) {}
  FileHandle(FILE *handle) : __file_handle(handle, deleter) {}

  ~FileHandle() = default;

  //FileHandle(FileHandle &h) = delete;
  //FileHandle(const FileHandle &h) = delete;

protected:
  static void deleter(FILE *f) {
    if (f) {
      std::fclose(f);
    }
  }
};

}
