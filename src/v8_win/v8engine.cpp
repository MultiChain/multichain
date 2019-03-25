// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8engine.h"
#include "v8_win/v8filter.h"
#include "v8_win/v8utils.h"
#include <libplatform/libplatform.h>

extern "C"
{
    extern char _binary_icudtl_dat_start;
    extern char _binary_icudtl_dat_end;
    extern char _binary_natives_blob_bin_start;
    extern char _binary_natives_blob_bin_end;
    extern char _binary_snapshot_blob_bin_start;
    extern char _binary_snapshot_blob_bin_end;
}

namespace mc_v8
{
V8Engine::~V8Engine()
{
    if (m_isolate != nullptr)
    {
        m_isolate->Dispose();
        delete m_createParams.array_buffer_allocator;
    }
}

int V8Engine::Initialize(IFilterCallback *filterCallback, std::string dataDir_, bool fDebug_, std::string &strResult)
{
    fDebug = fDebug_;
    dataDir = dataDir_;
    CreateLogger();
    spdlog::set_level(fDebug ? spdlog::level::debug : spdlog::level::info);

    logger->debug("V8Engine::Initialize - enter");
    strResult.clear();
    if (!m_isV8Initialized)
    {
        this->InitializeV8();
        m_isV8Initialized = true;
    }
    m_createParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    m_isolate = v8::Isolate::New(m_createParams);
    m_filterCallback = filterCallback;
    logger->debug("V8Engine::Initialize - leave strResult='{}'", strResult);
    return MC_ERR_NOERROR;
}

int V8Engine::CreateFilter(std::string script, std::string mainName, const std::vector<std::string> &callbackNames,
                           V8Filter *filter, int jsInjectionParams, std::string &strResult)
{
    logger->debug("V8Engine::CreateFilter - enter");
    strResult.clear();
    int retval = filter->Initialize(this, script, mainName, callbackNames, jsInjectionParams, strResult);
    logger->debug("V8Engine::CreateFilter - leave retval={} strResult='{}'", retval, strResult);
    return retval;
}

int V8Engine::RunFilter(V8Filter *filter, std::string &strResult)
{
    logger->debug("V8Engine::RunFilter - enter");
    strResult.clear();
    if (filter == nullptr)
    {
        strResult = "Trying to run an invalid filter";
        return MC_ERR_NOERROR;
    }
    int retval = filter->Run(strResult);
    logger->debug("V8Engine::RunFilter - leave retval={} strResult='{}'", retval, strResult);
    return retval;
}

void V8Engine::TerminateFilter(V8Filter *filter, std::string reason)
{
    logger->debug("V8Engine::TerminateFilter");
    if (filter != nullptr && filter->IsRunning())
    {
        m_reason = reason;
        m_isolate->TerminateExecution();
    }
}

void V8Engine::InitializeV8()
{
    logger->debug("V8Engine::InitializeV8 - enter");
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
    logger->debug("V8Engine::InitializeV8 - leave");
}

std::unique_ptr<v8::Platform> V8Engine::m_platform;
v8::Isolate::CreateParams V8Engine::m_createParams;
bool V8Engine::m_isV8Initialized = false;
} // namespace mc_v8
