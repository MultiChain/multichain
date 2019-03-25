// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_V8ENGINE_H_
#define MULTICHAIN_V8ENGINE_H_

#include "filters/ifiltercallback.h"
#include <v8.h>

namespace mc_v8
{
class V8Filter;

/**
 * Interface to the Google V8 engine to create and run filters.
 */
class V8Engine
{
public:
    ~V8Engine();

    /**
     * Initialize the V8 engine.
     *
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(IFilterCallback *filterCallback, std::string dataDir_, bool fDebug_, std::string &strResult);

    /**
     * Get the Isolate used by the engine.
     */
    v8::Isolate *GetIsolate() { return m_isolate; }

    /**
     * Get the filter callback object for current filter.
     */
    IFilterCallback *GetFilterCallback() { return m_filterCallback; }

    /**
     * Create a new filter.
     *
     * @param script         The filter JS code.
     * @param mainName       The expected name of the filtering function in the script.
     * @param callbackNames  A list of callback function names to register for the filter.
     *                       If empty, register no callback functions.
     * @param filter         The filter object to initialize.
     * @param strResult      Reason for failure if unsuccessful.
     * @return               MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int CreateFilter(std::string script, std::string mainName, const std::vector<std::string> &callbackNames,
                     V8Filter *filter, int jsInjectionParams, std::string &strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param filter    The user-defined filter to use.
     * @param strResult Reason for script failure or rejection.
     * @return          MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int RunFilter(V8Filter *filter, std::string &strResult);

    /**
     * Abort the currently running filter (if any).
     *
     * @param filter    The filter to abort.
     * @param reason    The reason the filter is being terminated.
     */
    void TerminateFilter(V8Filter *filter, std::string reason);

    /**
     * Get the reason for the most recent forced filter termination.
     */
    std::string TerminationReason() const { return m_reason; }

private:
    IFilterCallback *m_filterCallback = nullptr;
    v8::Isolate *m_isolate = nullptr;
    static std::unique_ptr<v8::Platform> m_platform;
    static v8::Isolate::CreateParams m_createParams;
    static bool m_isV8Initialized;
    std::string m_reason;

    static void InitializeV8();
};

} // namespace mc_v8

#endif /* MULTICHAIN_V8ENGINE_H_ */
