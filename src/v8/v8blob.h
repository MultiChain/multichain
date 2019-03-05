// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8BLOB_H
#define V8BLOB_H

#include "v8_win/declspec.h"
#include <map>
#include <memory>
#include <string>

namespace mc_v8
{
class V8_WIN_EXPORTS Blob;
using BlobPtr = std::shared_ptr<Blob>;

class V8_WIN_EXPORTS Blob
{
  public:
    const size_t size_increment = 4096;

    static BlobPtr Instance(std::string name = "");
    ~Blob();

    void Reset();

    void Set(void *data, size_t size);

    void Append(void *data, size_t size);

    unsigned char *Data()
    {
        return m_buffer;
    }

    size_t DataSize() const
    {
        return m_size;
    }

    bool IsEmpty() const
    {
        return m_size == 0;
    }

    static void Remove(std::string name);

  private:
    Blob()
    {
    }

    void Resize(size_t add_size);

    static std::map<std::string, BlobPtr> m_instances;

    unsigned char *m_buffer = nullptr;
    size_t m_size = 0;
    size_t m_allocated = 0;
};
} // namespace mc_v8

#endif // V8BLOB_H
