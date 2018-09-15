// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8engine.h"
#include "v8filter.h"
#include "utils/define.h"
#include "utils/util.h"

namespace mc_v8
{

void V8Engine::Zero()
{
}

int V8Engine::Destroy()
{
    this->Zero();
    return MC_ERR_NOERROR;
}

int V8Engine::Initialize(std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Engine::Initialize\n");
    strResult.clear();
    return MC_ERR_NOERROR;
}

int V8Engine::CreateFilter(std::string script, std::string main_name, V8Filter* filter, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Engine::CreateFilter\n");
    strResult.clear();
    return filter->Initialize(script, main_name, strResult);
}

int V8Engine::RunFilter(V8Filter* filter, std::string& strResult)
{
    strResult.clear();
    if (filter == nullptr)
    {
        strResult = "Trying to run an invalid filter";
        return MC_ERR_NOERROR;
    }
    return filter->Run(strResult);
}

int V8Engine::RunFilterWithCallbackLog(V8Filter* filter, std::string& strResult, json_spirit::Array& callbacks)
{
    strResult.clear();
    if (filter == nullptr)
    {
        strResult = "Trying to run an invalid filter";
        return MC_ERR_NOERROR;
    }
    return filter->RunWithCallbackLog(strResult, callbacks);
}

} // namespace mc_v8
