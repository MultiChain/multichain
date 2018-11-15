#ifndef V8BLOB_H
#define V8BLOB_H

#include <map>
#include <string>

namespace mc_v8
{
class Blob
{
  public:
    const size_t size_increment = 4096;

    static Blob *Instance(std::string name);
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

    static void Destroy(std::string name);

  private:
    Blob();
    void Resize(size_t add_size);

    static std::map<std::string, Blob *> m_instances;

    unsigned char *m_buffer;
    size_t m_size;
    size_t m_allocated;
};

} // namespace mc_v8

#endif // V8BLOB_H
