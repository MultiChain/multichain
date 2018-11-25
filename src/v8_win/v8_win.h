#ifndef V8_WIN_H
#define V8_WIN_H

#include "declspec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct V8Engine;
struct V8Filter;
struct IFilterCallback;

DLLEXPORT V8Filter *V8Filter_Create();
DLLEXPORT void V8Filter_Delete(V8Filter **filter_);
DLLEXPORT bool V8Filter_IsRunning(V8Filter *filter_);
DLLEXPORT int V8Filter_Initialize(V8Filter *filter_, V8Engine *engine_, const char *script_, const char *functionName_,
                                  const char **callbackNames_, int nCallbackNames_, bool isFilterLimitedMathSet_,
                                  char **strResult_);
DLLEXPORT int V8Filter_Run(V8Filter *filter_, char **strResult_);

DLLEXPORT V8Engine *V8Engine_Create();
DLLEXPORT void V8Engine_Delete(V8Engine **engine_);
DLLEXPORT int V8Engine_Initialize(V8Engine *engine_, IFilterCallback *filterCallback_, const char *dataDir_,
                                  bool fDebug_, const char **strResult_);
DLLEXPORT int V8Engine_CreateFilter(V8Engine *engine_, const char *script_, const char *main_name_,
                                    const char **callbackNames_, int nCallbackNames_, V8Filter *filter_,
                                    bool isFilterLimitedMathSet_, char **strResult_);
DLLEXPORT int V8Engine_RunFilter(V8Engine *engine_, V8Filter *filter_, char **strResult_);
DLLEXPORT void V8Engine_TerminateFilter(V8Engine *engine_, V8Filter *filter_, const char *reason_);

#ifdef __cplusplus
}
#endif

#endif // V8_WIN_H
