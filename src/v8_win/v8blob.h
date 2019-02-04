// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8BLOB_H
#define V8BLOB_H

#include <map>
#include <memory>
#include <string>

namespace mc_v8
{
class Blob;
using BlobPtr = std::shared_ptr<Blob>;

/**
 * Convenince class for holding an unspecified blob of data.
 *
 * Manages the storage of the blob, so the user does not need to issue allocate and delete actions.
 *
 * Each named Blob is a singleton that can only grow it's allocated size. This makes it an efficient container for a
 * large number of filling and refilling actions.
 */
class Blob
{
public:
    /**
     * Blob content can only grow, by multiples of this increment, every time it needs to.
     */
    const size_t sizeIncrement = 4096;

    /**
     * Create or get a named Blob.
     *
     * @param name The name of the Blob.
     * @return     A shared pointer to the (single) Blob of that name.
     */
    static BlobPtr Instance(std::string name = "");

    /**
     * Destructor
     */
    ~Blob();

    /**
     * Reset the Blob to the empty state (DataSize = 0).
     */
    void Reset();

    /**
     * Set the content of the blob.
     *
     * @param data The data to set.
     * @param size The size of the data, in bytes.
     */
    void Set(const void *data, size_t size);

    /**
     * Append content to the Blob.
     *
     * @param data The data to append.
     * @param size The size of the data, in bytes.
     */
    void Append(const void *data, size_t size);

    /**
     * Get the name of the Blob.
     */
    std::string Name() const { return m_name; }

    /**
     * Get the data stored in the blob.
     */
    const unsigned char *Data() const { return m_buffer; }

    /**
     * Get the size of the data stored in the blob, in bytes.
     */
    size_t DataSize() const { return m_size; }

    /**
     * Test if the Blob is empty.
     */
    bool IsEmpty() const { return m_size == 0; }

    /**
     * Get a textual representation og the Blob.
     *
     * The format of the output is <size>:<hex>.
     */
    std::string ToString() const;

    static void Remove(std::string name);

private:
    Blob(std::string name) : m_name(name) {}

    void Resize(size_t add_size);

    static std::map<std::string, BlobPtr> m_instances;

    std::string m_name;
    unsigned char *m_buffer = nullptr;
    size_t m_size = 0;
    size_t m_allocated = 0;
};
} // namespace mc_v8

#endif // V8BLOB_H
