// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8blob.h"
#include <cstring>

namespace mc_v8
{
BlobPtr Blob::Instance(std::string name)
{
    auto it = m_instances.find(name);
    if (it == m_instances.end())
    {
        it = m_instances.insert(std::make_pair(name, std::make_shared<Blob>())).first;
    }
    return it->second;
}

Blob::~Blob()
{
    if (m_buffer != nullptr)
    {
        delete[] m_buffer;
    }
}

void Blob::Reset()
{
    m_size = 0;
}

void Blob::Set(void *data, size_t size)
{
    this->Reset();
    this->Append(data, size);
}

void Blob::Append(void *data, size_t size)
{
    this->Resize(size);
    std::memcpy(m_buffer + m_size, data, size);
    m_size += size;
}

void Blob::Remove(std::string name)
{
    m_instances.erase(name);
}

void Blob::Resize(size_t add_size)
{
    if (m_size + add_size > m_allocated)
    {
        size_t new_size = ((m_size + add_size - 1) / size_increment + 1) * size_increment;
        auto new_buffer = new unsigned char[new_size];
        if (m_allocated > 0)
        {
            std::memcpy(new_buffer, m_buffer, m_allocated);
            delete[] m_buffer;
        }
        m_buffer = new_buffer;
        m_allocated = new_size;
    }
}

std::map<std::string, BlobPtr> Blob::m_instances;
} // namespace mc_v8
