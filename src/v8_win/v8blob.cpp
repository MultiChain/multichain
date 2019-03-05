// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8blob.h"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace mc_v8
{
std::map<std::string, BlobPtr> Blob::m_instances;

BlobPtr Blob::Instance(std::string name)
{
    struct make_shared_enabler : public Blob
    {
        make_shared_enabler(std::string name = "") : Blob(name) {}
    };
    auto it = m_instances.find(name);
    if (it == m_instances.end())
    {
        it = m_instances.insert(std::make_pair(name, std::make_shared<make_shared_enabler>(name))).first;
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

void Blob::Set(const void *data, size_t size)
{
    this->Reset();
    this->Append(data, size);
}

void Blob::Append(const void *data, size_t size)
{
    this->Resize(size);
    std::memcpy(m_buffer + m_size, data, size);
    m_size += size;
}

std::string Blob::ToString() const
{
    std::ostringstream ostr;
    ostr << m_size << ":";
    for (size_t i = 0; i < m_size; ++i)
    {
        ostr << m_buffer[i];
    }
    return ostr.str();
}

void Blob::Remove(std::string name)
{
    m_instances.erase(name);
}

void Blob::Resize(size_t add_size)
{
    if (m_size + add_size > m_allocated)
    {
        size_t new_size = ((m_size + add_size - 1) / sizeIncrement + 1) * sizeIncrement;
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
} // namespace mc_v8
