#include "v8_win/v8_win.h"
#include "v8_win/v8engine.h"
#include "v8_win/v8filter.h"
#include "v8_win/v8utils.h"

const size_t RESULT_SIZE = 4096;

std::vector<std::string> cstrs2vec(const char** cstrs, size_t ncstrs)
{
    std::vector<std::string> vec(ncstrs);
    for (size_t i = 0; i < ncstrs; ++i) {
        vec[i] = std::string(cstrs[i]);
    }
    return vec;
}

DLLEXPORT V8Filter_t* V8Filter_Create()
{
    return reinterpret_cast<V8Filter_t*>(new mc_v8::V8Filter());
}

DLLEXPORT void V8Filter_Delete(V8Filter_t** filter_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(*filter_);
    if (filter != nullptr) {
        delete filter;
        *filter_ = nullptr;
    }
}

DLLEXPORT bool V8Filter_IsRunning(V8Filter_t* filter_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    return filter->IsRunning();
}

DLLEXPORT int V8Filter_Initialize(V8Filter_t* filter_,
                                  V8Engine_t* engine_,
                                  const char* script_,
                                  const char* functionName_,
                                  const char** callbackNames_,
                                  size_t nCallbackNames_,
                                  bool isFilterLimitedMathSet_,
                                  char* strResult_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(engine_);
    std::vector<std::string> callbackNames = cstrs2vec(callbackNames_, nCallbackNames_);
    std::string strResult;
    int retval = filter->Initialize(engine,
                                    script_,
                                    functionName_,
                                    callbackNames,
                                    isFilterLimitedMathSet_,
                                    strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

DLLEXPORT int V8Filter_Run(V8Filter_t* filter_, char* strResult_)
{
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    std::string strResult;
    int retval = filter->Run(strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

DLLEXPORT V8Engine_t* V8Engine_Create()
{
    return reinterpret_cast<V8Engine_t*>(new mc_v8::V8Engine());
}

DLLEXPORT void V8Engine_Delete(V8Engine_t** engine_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(*engine_);
    if (engine != nullptr) {
        delete engine;
        *engine_ = nullptr;
    }
}

DLLEXPORT int V8Engine_Initialize(V8Engine_t* engine_,
                                  IFilterCallback_t* filterCallback_,
                                  const char* dataDir_,
                                  bool fDebug_,
                                  char* strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(engine_);
    auto filterCallback = reinterpret_cast<IFilterCallback*>(filterCallback_);
    std::string strResult;
    int retval = engine->Initialize(filterCallback, dataDir_, fDebug_, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

DLLEXPORT int V8Engine_CreateFilter(V8Engine_t* engine_,
                                    const char* script_,
                                    const char* main_name_,
                                    const char** callbackNames_,
                                    size_t nCallbackNames_,
                                    V8Filter_t* filter_,
                                    bool isFilterLimitedMathSet_,
                                    char* strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    std::vector<std::string> callbackNames = cstrs2vec(callbackNames_, nCallbackNames_);
    std::string strResult;
    int retval = engine->CreateFilter(script_,
                                      main_name_,
                                      callbackNames,
                                      filter,
                                      isFilterLimitedMathSet_,
                                      strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

DLLEXPORT int V8Engine_RunFilter(V8Engine_t* engine_, V8Filter_t* filter_, char* strResult_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    std::string strResult;
    int retval = engine->RunFilter(filter, strResult);
    strcpy_s(strResult_, RESULT_SIZE, strResult.c_str());
    return retval;
}

DLLEXPORT void V8Engine_TerminateFilter(V8Engine_t* engine_,
                                        V8Filter_t* filter_,
                                        const char* reason_)
{
    auto engine = reinterpret_cast<mc_v8::V8Engine*>(engine_);
    auto filter = reinterpret_cast<mc_v8::V8Filter*>(filter_);
    engine->TerminateFilter(filter, reason_);
}
