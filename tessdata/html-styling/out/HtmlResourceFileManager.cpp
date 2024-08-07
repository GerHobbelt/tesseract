/**
 * This file was generated by bin2cpp v3.0.0
 * Copyright (C) 2013-2024 end2endzone.com. All rights reserved.
 * bin2cpp is open source software, see http://github.com/end2endzone/bin2cpp
 * Do not modify this file.
 */
#include "HtmlResourceFileManager.h"
#include <string>
#include <string.h> // strlen
#include <sys/stat.h> // stat
#include <errno.h>    // errno, EEXIST
#if defined(_WIN32)
#include <direct.h>   // _mkdir
#endif

#if defined(_WIN32)
#define portable_stat _stat
#define portable_mkdir(path) _mkdir(path)
#define PATH_SEPARATOR_CHAR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#define portable_stat stat
#define portable_mkdir(path) mkdir(path, 0755)
#define PATH_SEPARATOR_CHAR '/'
#define PATH_SEPARATOR_STR "/"
#endif

namespace bin2cpp
{
  bool RegisterFile(FileManager::t_func functionPointer)
  {
    if (functionPointer == NULL)
      return false;
    FileManager::getInstance().registerFile(functionPointer);
    return true;
  }
  FileManager::FileManager() {}
  FileManager::~FileManager() {}
  FileManager & FileManager::getInstance() { static FileManager _mgr; return _mgr; }
  void FileManager::registerFile(t_func func) { functions_.push_back(func); }
  size_t FileManager::getFileCount() const { return functions_.size(); }
  const File * FileManager::getFile(const size_t & index) const
  {
    if (index >= functions_.size())
      return NULL;
    t_func ressource_getter_function = functions_[index];
    const bin2cpp::File & resource = ressource_getter_function();
    return &resource;
  }
  bool FileManager::saveFiles(const char * directory) const
  {
    if (directory == NULL)
      return false;
    size_t count = getFileCount();
    for(size_t i=0; i<count; i++)
    {
      const File * f = getFile(i);
      if (!f)
        return false;
      std::string path;
      path.append(directory);
      path.append(1,PATH_SEPARATOR_CHAR);
      path.append(f->getFilePath());
      if (!createParentDirectories(path.c_str()))
        return false;
      bool saved = f->save(path.c_str());
      if (!saved)
        return false;
    }
    return true;
  }
  static inline bool isRootDirectory(const char * path)
  {
    if (path == NULL && path[0] == '\0')
      return false;
  #if defined(_WIN32)
    bool isDriveLetter = ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z'));
    if ((isDriveLetter && path[1] == ':' && path[2] == '\0') || // test for C:
        (isDriveLetter && path[1] == ':' && path[2] == PATH_SEPARATOR_CHAR && path[3] == '\0')) // test for C:\ 
      return true;
  #else
    if (path[0] == PATH_SEPARATOR_CHAR)
      return true;
  #endif
    return false;
  }
  bool FileManager::createParentDirectories(const char * file_path) const
  {
    if (file_path == NULL)
      return false;
    std::string accumulator;
    size_t length = strlen(file_path);
    for(size_t i=0; i<length; i++)
    {
      if (file_path[i] == PATH_SEPARATOR_CHAR && !accumulator.empty() && !isRootDirectory(accumulator.c_str()))
      {
        int ret = portable_mkdir(accumulator.c_str());
        if (ret != 0 && errno != EEXIST)
          return false;
      }
      accumulator += file_path[i];
    }
    return true;
  }
  bool FileManager::createDirectories(const char * path) const
  {
    if (path == NULL)
      return false;
    std::string path_copy = path;
    path_copy.append(1,PATH_SEPARATOR_CHAR);
    return createParentDirectories(path_copy.c_str());
  }
}; //bin2cpp
