// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8_WIN_H
#define V8_WIN_H

#include "declspec.h"

#define MC_V8W_JS_INJECTION_LIMITED_MATH_SET             0x00000001
#define MC_V8W_JS_INJECTION_FIXED_DATE_FUNCTIONS         0x00000002
#define MC_V8W_JS_INJECTION_DISABLED_DATE_PARSE          0x00000004


#ifdef __cplusplus
extern "C" {
#endif

struct Blob_t;
struct V8Engine_t;
struct V8Filter_t;
struct IFilterCallback_t;

/**
 * Reset the Blob to the empty state (DataSize = 0).
 */
DLLEXPORT void Blob_Reset(Blob_t* blob_);

/**
 * Set the content of the blob.
 *
 * @param data_ The data to set.
 * @param size_ The size of the data, in bytes.
 */
DLLEXPORT void Blob_Set(Blob_t* blob_, const unsigned char* data_, size_t size_);

/**
 * Append content to the Blob.
 *
 * @param dat_a The data to append.
 * @param size_ The size of the data, in bytes.
 */
DLLEXPORT void Blob_Append(Blob_t* blob_, const unsigned char* data, size_t size_);

/**
 * Get the data stored in the blob.
 */
DLLEXPORT const unsigned char* Blob_Data(Blob_t* blob_);

/**
 * Get the size of the data stored in the blob, in bytes.
 */
DLLEXPORT size_t Blob_DataSize(Blob_t* blob_);

/**
 * Get a textual representation og the Blob.
 *
 * The format of the output is <size>:<hex>.
 */
DLLEXPORT const char* Blob_ToString(Blob_t* blob_);

/**
 * Create a new filter.
 */
DLLEXPORT V8Filter_t* V8Filter_Create();

/**
 * Delete a filter.
 */
DLLEXPORT void V8Filter_Delete(V8Filter_t** filter_);

/**
 * Test if the filter fuction is currently executing.
 */
DLLEXPORT bool V8Filter_IsRunning(V8Filter_t* filter_);

/**
 * Initialize the filter to run the function @p functionName in the JS @p script.
 *
 * @param filter_           The filter operate on.
 * @param script_           The filter JS code.
 * @param main_name_        The expected name of the filtering function in the script.
 * @param callbackNames_    An array of callback function names to register for the filter.
 *                          If empty, register no callback functions.
 * @param nCallbackNames_   The number of callback names in @p callback_names_
 * @param strResult_        Reason for failure if unsuccessful.
 * @return                  MC_ERR_INTERNAL_ERROR if the engine failed,
 * MC_ERR_NOERROR otherwise.
 */
DLLEXPORT int V8Filter_Initialize(V8Filter_t* filter_, V8Engine_t* engine_, const char* script_,
                                  const char* functionName_, const char** callbackNames_, size_t nCallbackNames_,
                                  int jsInjectionParams, char* strResult_);

/**
 * Run the filter function in the JS script.
 *
 * @param filter_       The filter operate on.
 * @param strResult_    Reason for script failure or rejection.
 * @return              MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
 */
DLLEXPORT int V8Filter_Run(V8Filter_t* filter_, char* strResult_);

/**
 * Create a new engine.
 */
DLLEXPORT V8Engine_t* V8Engine_Create();

/**
 * Delete an engine.
 */
DLLEXPORT void V8Engine_Delete(V8Engine_t** engine_);

/**
 * Initialize the V8 engine.
 *
 * @param engine_           The engine to operate on.
 * @param filterCallback_   A filter callback object.
 * @param dataDir_          The chain data folder.
 * @param fDebug_           @c true of the -debug argument was given.
 * @param strResult_        Reason for failure if unsuccessful.
 * @return                  MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
 */
DLLEXPORT int V8Engine_Initialize(V8Engine_t* engine_, IFilterCallback_t* filterCallback_, const char* dataDir_,
                                  bool fDebug_, char* strResult_);

/**
 * Create a new filter.
 *
 * @param engine_                   The engine to operate on.
 * @param script_                   The filter JS code.
 * @param mainName_                 The expected name of the filtering function in the script.
 * @param callbackNames_            An array of callback function names to register for the filter.
 *                                  If empty, register no callback functions.
 * @param nCallbackNames_           The number of callback names in @p callback_names_
 * @param filter_                   The filter object to initialize.
 * @param jsInjectionParams         @c JC injection params. MC_V8W_JS_INJECTION constants
 * @param strResult_                Reason for failure if unsuccessful.
 * @return                          MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
 */
DLLEXPORT int V8Engine_CreateFilter(V8Engine_t* engine_, const char* script_, const char* mainName_,
                                    const char** callbackNames_, size_t nCallbackNames_, V8Filter_t* filter_,
                                    int jsInjectionParams, char* strResult_);

/**
 * Run the filter function in the JS script.
 *
 * @param engine_       The engine to operate on.
 * @param filter_       The user-defined filter to use.
 * @param strResult_    Reason for script failure or rejection.
 * @return              MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
 */
DLLEXPORT int V8Engine_RunFilter(V8Engine_t* engine_, V8Filter_t* filter_, char* strResult_);

/**
 * Abort the currently running filter (if any).
 *
 * @param engine_       The engine to operate on.
 * @param filter_       The filter to abort.
 * @param reason_       The reason the filter is being terminated.
 */
DLLEXPORT void V8Engine_TerminateFilter(V8Engine_t* engine_, V8Filter_t* filter_, const char* reason_);

#ifdef __cplusplus
}
#endif

#endif // V8_WIN_H
