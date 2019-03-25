// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8_win.h"
#include "v8_win/v8blob.h"
#include "v8_win/v8engine.h"
#include "v8_win/v8filter.h"
#include "v8_win/v8utils.h"

using namespace mc_v8;

const size_t RESULT_SIZE = 4096;

std::vector<std::string> cstrs2vec(const char **cstrs, size_t ncstrs)
{
    std::vector<std::string> vec(ncstrs);
    for (size_t i = 0; i < ncstrs; ++i)
    {
        vec[i] = std::string(cstrs[i]);
    }
    return vec;
}

void Blob_Reset(Blob_t *blob_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    blob->Reset();
}

void Blob_Set(Blob_t *blob_, const unsigned char *data_, size_t size_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    blob->Set(data_, size_);
}

void Blob_Append(Blob_t *blob_, const unsigned char *data_, size_t size_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    blob->Append(data_, size_);
}

const unsigned char *Blob_Data(Blob_t *blob_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    return blob->Data();
}

size_t Blob_DataSize(Blob_t *blob_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    return blob->DataSize();
}

const char *Blob_ToString(Blob_t *blob_)
{
    auto blob = Blob::Instance(reinterpret_cast<Blob *>(blob_)->Name());
    return blob->ToString().c_str();
}

V8Filter_t *V8Filter_Create()
{
    return reinterpret_cast<V8Filter_t *>(new V8Filter());
}

void V8Filter_Delete(V8Filter_t **filter_)
{
    auto filter = reinterpret_cast<V8Filter *>(*filter_);
    if (filter != nullptr)
    {
        delete filter;
        *filter_ = nullptr;
    }
}

bool V8Filter_IsRunning(V8Filter_t *filter_)
{
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    return filter->IsRunning();
}

int V8Filter_Initialize(V8Filter_t *filter_, V8Engine_t *engine_, const char *script_, const char *functionName_,
                        const char **callbackNames_, size_t nCallbackNames_, int jsInjectionParams,
                        char *strResult_)
{
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    auto engine = reinterpret_cast<V8Engine *>(engine_);
    std::vector<std::string> callbackNames = cstrs2vec(callbackNames_, nCallbackNames_);
    std::string strResult;
    int retval = filter->Initialize(engine, script_, functionName_, callbackNames, jsInjectionParams, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

int V8Filter_Run(V8Filter_t *filter_, char *strResult_)
{
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    std::string strResult;
    int retval = filter->Run(strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

V8Engine_t *V8Engine_Create()
{
    return reinterpret_cast<V8Engine_t *>(new V8Engine());
}

void V8Engine_Delete(V8Engine_t **engine_)
{
    auto engine = reinterpret_cast<V8Engine *>(*engine_);
    if (engine != nullptr)
    {
        delete engine;
        *engine_ = nullptr;
    }
}

int V8Engine_Initialize(V8Engine_t *engine_, IFilterCallback_t *filterCallback_, const char *dataDir_, bool fDebug_,
                        char *strResult_)
{
    auto engine = reinterpret_cast<V8Engine *>(engine_);
    auto filterCallback = reinterpret_cast<IFilterCallback *>(filterCallback_);
    std::string strResult;
    int retval = engine->Initialize(filterCallback, dataDir_, fDebug_, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

int V8Engine_CreateFilter(V8Engine_t *engine_, const char *script_, const char *mainName_, const char **callbackNames_,
                          size_t nCallbackNames_, V8Filter_t *filter_, int jsInjectionParams, char *strResult_)
{
    auto engine = reinterpret_cast<V8Engine *>(engine_);
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    std::vector<std::string> callbackNames = cstrs2vec(callbackNames_, nCallbackNames_);
    std::string strResult;
    int retval = engine->CreateFilter(script_, mainName_, callbackNames, filter, jsInjectionParams, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

int V8Engine_RunFilter(V8Engine_t *engine_, V8Filter_t *filter_, char *strResult_)
{
    auto engine = reinterpret_cast<V8Engine *>(engine_);
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    std::string strResult;
    int retval = engine->RunFilter(filter, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

void V8Engine_TerminateFilter(V8Engine_t *engine_, V8Filter_t *filter_, const char *reason_)
{
    auto engine = reinterpret_cast<V8Engine *>(engine_);
    auto filter = reinterpret_cast<V8Filter *>(filter_);
    engine->TerminateFilter(filter, reason_);
}
