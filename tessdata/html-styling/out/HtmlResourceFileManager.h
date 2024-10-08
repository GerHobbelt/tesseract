/**
 * This file was generated by bin2cpp v3.0.0
 * Copyright (C) 2013-2024 end2endzone.com. All rights reserved.
 * bin2cpp is open source software, see http://github.com/end2endzone/bin2cpp
 * Do not modify this file.
 */
#ifndef BIN2CPP_HTMLRESOURCEFILEMANAGER_H
#define BIN2CPP_HTMLRESOURCEFILEMANAGER_H

#include <stddef.h>
#include <vector>

namespace bin2cpp
{
  #ifndef BIN2CPP_EMBEDDEDFILE_CLASS
  #define BIN2CPP_EMBEDDEDFILE_CLASS
  class File
  {
  public:
    // warning C5204: 'bin2cpp::File': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly
    virtual ~File() = default;

    virtual size_t getSize() const = 0;
    /* DEPRECATED */ virtual inline const char * getFilename() const { return getFileName(); }
    virtual const char * getFileName() const = 0;
    virtual const char * getFilePath() const = 0;
    virtual const char * getBuffer() const = 0;
    virtual bool save(const char * filename) const = 0;
  };
  #endif //BIN2CPP_EMBEDDEDFILE_CLASS

  #ifndef BIN2CPP_FILEMANAGER_CLASS
  #define BIN2CPP_FILEMANAGER_CLASS
  class FileManager
  {
  private:
    FileManager();
    ~FileManager();
  public:
    typedef const File & (*t_func)();
    static FileManager & getInstance();
    void registerFile(t_func func);
    size_t getFileCount() const;
    const File * getFile(const size_t & index) const;
    bool saveFiles(const char * directory) const;
    bool createParentDirectories(const char * file_path) const;
    bool createDirectories(const char * path) const;
  private:
    std::vector<t_func> functions_;
  };
  #endif //BIN2CPP_FILEMANAGER_CLASS
}; //bin2cpp

#endif //BIN2CPP_HTMLRESOURCEFILEMANAGER_H
