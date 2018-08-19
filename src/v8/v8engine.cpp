// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8engine.h"
#include "v8filter.h"
#include "utils/define.h"
#include "utils/util.h"
#include <boost/filesystem.hpp>
#include <cassert>

namespace fs = boost::filesystem;

namespace mc_v8
{

void V8Engine::Zero()
{
//    LogPrintf("V8Engine: Zero\n");
}

int V8Engine::Destroy()
{
//    LogPrintf("V8Engine: Destroy\n");
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    this->Zero();
    return MC_ERR_NOERROR;
}

int V8Engine::Initialize(std::string& strResult)
{
    LogPrintf("V8Engine: Initialize\n");
    std::string argv0 = (fs::path(std::getenv("HOME")) / "local" / "v8" / "data" / "foo").string();

    strResult.clear();
    v8::V8::InitializeICUDefaultLocation(argv0.c_str());
    v8::V8::InitializeExternalStartupData(argv0.c_str());
    m_platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(m_platform.get());
    if (!v8::V8::Initialize())
    {
        strResult = "Error initializing V8";
        return MC_ERR_INTERNAL_ERROR;
    }

    return MC_ERR_NOERROR;
}

int V8Engine::CreateFilter(std::string script, std::string main_name, V8Filter* filter, std::string& strResult)
{
    LogPrintf("V8Engine: CreateFilter\n");
    int status = filter->Initialize(script, main_name, strResult);
    if (status != MC_ERR_NOERROR)
    {
        delete filter;
    }

    return status;
}

} // namespace mc_v8
