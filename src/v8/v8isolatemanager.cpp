// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8isolatemanager.h"
#include "v8utils.h"
#include "utils/util.h"

extern char _binary_icudtl_dat_start;
extern char _binary_icudtl_dat_end;
extern char _binary_natives_blob_bin_start;
extern char _binary_natives_blob_bin_end;
extern char _binary_snapshot_blob_bin_start;
extern char _binary_snapshot_blob_bin_end;

namespace mc_v8
{

V8IsolateManager* V8IsolateManager::m_instance = nullptr;

V8IsolateManager* V8IsolateManager::Instance()
{
    if (m_instance == nullptr)
    {
        m_instance = new V8IsolateManager();
    }
    return m_instance;
}

v8::Isolate* V8IsolateManager::GetIsolate()
{
    if (m_isolate == nullptr)
    {
        m_createParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        m_isolate = v8::Isolate::New(m_createParams);
    }
    return m_isolate;
}

IsolateData& V8IsolateManager::GetIsolateData(v8::Isolate* isolate)
{
    auto it = m_callbacks.find(isolate);
    if (it == m_callbacks.end())
    {
        auto p = m_callbacks.insert(std::make_pair(isolate, IsolateData()));
        it = p.first;
    }
    return it->second;
}

V8IsolateManager::V8IsolateManager()
{
    if(fDebug)LogPrint("v8filter", "v8filter: V8IsolateManager::V8IsolateManager\n");

    fs::path tempDir = GetTemporaryPidDirectory();
    fs::path v8TempDir = tempDir / "v8";
    fs::create_directories(v8TempDir);

    fs::path icudtl_blob = v8TempDir / "icudtl.dat";
    fs::path natives_blob = v8TempDir / "natives_blob.bin";
    fs::path snapshot_blob = v8TempDir / "snapshot_blob.bin";

    WriteBinaryFile(icudtl_blob, &_binary_icudtl_dat_start, &_binary_icudtl_dat_end - &_binary_icudtl_dat_start);
    WriteBinaryFile(natives_blob, &_binary_natives_blob_bin_start,
            &_binary_natives_blob_bin_end - &_binary_natives_blob_bin_start);
    WriteBinaryFile(snapshot_blob, &_binary_snapshot_blob_bin_start,
            &_binary_snapshot_blob_bin_end - &_binary_snapshot_blob_bin_start);

    v8::V8::InitializeICUDefaultLocation(icudtl_blob.string().c_str());
    v8::V8::InitializeExternalStartupData(natives_blob.string().c_str());

    fs::remove_all(tempDir);

    m_platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(m_platform.get());
    v8::V8::Initialize();
}

V8IsolateManager::~V8IsolateManager()
{
    if (m_isolate != nullptr)
    {
        m_isolate->Dispose();
        delete m_createParams.array_buffer_allocator;
    }
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
}

} /* namespace mc_v8 */
