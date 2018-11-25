#ifndef V8_WIN_H
#define V8_WIN_H

#include "filters/ifiltercallback.h"

#ifdef __cplusplus
extern "C" {
#endif

struct V8Engine;
struct V8Filter;

V8Filter *V8Filter_Create();
bool V8Filter_IsRunning(V8Filter *filter);
int V8Filter_Initialize(V8Filter *filter, V8Engine *engine, const char *script, const char *functionName,
               const char **callbackNames, int nCallbackNames, bool isFilterLimitedMathSet, const char **strResult);
int V8Filter_Run(V8Filter *filter, const char **strResult);

V8Engine *V8Engine_Create();
int V8Engine_Initialize(V8Engine *engine, IFilterCallback *filterCallback, std::string dataDir_, bool fDebug_, std::string &strResult);

#ifdef __cplusplus
}
#endif

#endif // V8_WIN_H
