#ifndef V8_WIN_H
#define V8_WIN_H

#include "declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct V8Engine_t;
struct V8Filter_t;
struct IFilterCallback_t;

DLLEXPORT V8Filter_t* V8Filter_Create();
DLLEXPORT void V8Filter_Delete(V8Filter_t** filter_);
DLLEXPORT bool V8Filter_IsRunning(V8Filter_t* filter_);
DLLEXPORT int V8Filter_Initialize(V8Filter_t* filter_, V8Engine_t* engine_, const char* script_,
                                  const char* functionName_, const char** callbackNames_, size_t nCallbackNames_,
                                  bool isFilterLimitedMathSet_, char* strResult_);
DLLEXPORT int V8Filter_Run(V8Filter_t* filter_, char* strResult_);

DLLEXPORT V8Engine_t* V8Engine_Create();
DLLEXPORT void V8Engine_Delete(V8Engine_t** engine_);
DLLEXPORT int V8Engine_Initialize(V8Engine_t* engine_, IFilterCallback_t* filterCallback_, const char* dataDir_,
                                  bool fDebug_, char* strResult_);
DLLEXPORT int V8Engine_CreateFilter(V8Engine_t* engine_, const char* script_, const char* main_name_,
                                    const char** callbackNames_, size_t nCallbackNames_, V8Filter_t* filter_,
                                    bool isFilterLimitedMathSet_, char* strResult_);
DLLEXPORT int V8Engine_RunFilter(V8Engine_t* engine_, V8Filter_t* filter_, char* strResult_);
DLLEXPORT void V8Engine_TerminateFilter(V8Engine_t* engine_, V8Filter_t* filter_, const char* reason_);

#ifdef __cplusplus
}
#endif

#endif // V8_WIN_H
