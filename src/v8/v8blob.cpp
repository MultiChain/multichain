#include "v8blob.h"
#include <cstring>

namespace mc_v8
{
std::map<std::string, Blob *> Blob::m_instances;

Blob *Blob::Instance(std::string name)
{
    auto it = m_instances.find(name);
    if (it == m_instances.end())
    {
        auto p = m_instances.insert(std::make_pair(name, new Blob()));
        it = p.first;
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

void Blob::Destroy(std::string name)
{
    auto it = m_instances.find(name);
    if (it != m_instances.end())
    {
        delete it->second;
        m_instances.erase(it);
    }
}

Blob::Blob() : m_buffer(nullptr), m_size(0), m_allocated(0)
{
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

} // namespace mc_v8
