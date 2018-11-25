#include "v8_win.h"
#include "v8_win/v8filter.h"
#include "v8_win/v8engine.h"

DLLEXPORT V8Filter *V8Filter_Create()
{
    return reinterpret_cast<V8Filter *>(new mc_v8::V8Filter());
}

DLLEXPORT void V8Filter_Delete(V8Filter **filter_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    if (filter != nullptr)
    {
        delete filter;
        *filter_ = nullptr;
    }
}

DLLEXPORT bool V8Filter_IsRunning(V8Filter *filter_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    return filter->IsRunning();
}

DLLEXPORT int V8Filter_Initialize(V8Filter *filter_, V8Engine *engine_, const char *script_, const char *functionName_,
                                  const char **callbackNames_, int nCallbackNames_, bool isFilterLimitedMathSet_,
                                  char **strResult_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    std::vector<std::string> callbackNames;
    std::transform(callbackNames_, callbackNames_ + nCallbackNames_, std::back_inserter(callbackNames),
            [](const char* s) -> std::string { return std::string(s); });
    std::string strResult;
    int retval = filter->Initialize(engine, script_, functionName_, callbackNames, isFilterLimitedMathSet_, strResult);
    *strResult_ = strdup(strResult.c_str());
    return retval;
}

DLLEXPORT int V8Filter_Run(V8Filter *filter_, char **strResult_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    std::string strResult;
    int retval = filter->Run(strResult);
    *strResult_ = strdup(strResult.c_str());
    return retval;
}

DLLEXPORT V8Engine *V8Engine_Create()
{
    return reinterpret_cast<V8Engine *>(new mc_v8::V8Engine());
}

DLLEXPORT void V8Engine_Delete(V8Engine **engine_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    if (engine != nullptr)
    {
        delete engine;
        *engine_ = nullptr;
    }
}

DLLEXPORT int V8Engine_Initialize(V8Engine *engine_, IFilterCallback *filterCallback_, const char *dataDir_,
                                  bool fDebug_, const char **strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    auto filterCallback = reinterpret_cast<mc_v8::IFilterCallback *>(filterCallback_);
    std::string strResult;
    int retval = engine->Initialize(filterCallback, dataDir_, fDebug_, strResult);
    *strResult_ = strdup(strResult.c_str());
    return retval;
}

DLLEXPORT int V8Engine_CreateFilter(V8Engine *engine_, const char *script_, const char *main_name_,
                                    const char **callbackNames_, int nCallbackNames_, V8Filter *filter_,
                                    bool isFilterLimitedMathSet_, char **strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    std::vector<std::string> callbackNames;
    std::transform(callbackNames_, callbackNames_ + nCallbackNames_, std::back_inserter(callbackNames),
            [](const char* s) -> std::string { return std::string(s); });
    std::string strResult;
    int retval = engine->CreateFilter(script_, main_name_, callbackNames, filter, isFilterLimitedMathSet_, strResult);
    *strResult_ = strdup(strResult.c_str());
    return retval;
}

DLLEXPORT int V8Engine_RunFilter(V8Engine *engine_, V8Filter *filter_, char **strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    std::string strResult;
    int retval = engine->RunFilter(filter, strResult);
    *strResult_ = strdup(strResult.c_str());
    return retval;
}

DLLEXPORT void V8Engine_TerminateFilter(V8Engine *engine_, V8Filter *filter_, const char *reason_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine *>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter *>(filter_);
    engine->TerminateFilter(filter, reason_);
}
