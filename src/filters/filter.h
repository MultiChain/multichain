// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTER_H
#define MULTICHAIN_FILTER_H

#include "filters/filtercallback.h"
#include "json/json_spirit.h"

class mc_FilterEngine;
class Watchdog;

/**
 * A user-defined transaction filter.
 */
class mc_Filter
{
    friend class mc_FilterEngine;

  public:
    mc_Filter()
    {
        Zero();
    }

    ~mc_Filter()
    {
        Destroy();
    }

    void Zero();
    int Destroy();

    int Timeout() const
    {
        return m_timeout;
    }

    void SetTimeout(int timeout)
    {
        m_timeout = timeout;
    }

    /**
     * Initialize the transaction filter.
     *
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(std::string &strResult);

  private:
    void *m_Impl;
    int m_timeout;

}; // class mc_Filter

/**
 * An environment for creating and defining transaction filters.
 */
class mc_FilterEngine
{
  public:
    mc_FilterEngine()
    {
        Zero();
    }

    ~mc_FilterEngine()
    {
        Destroy();
    }

    void Zero();
    int Destroy();

    /**
     * Initialize the environment.
     *
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(std::string &strResult);

    /**
     * Initialize a user-defined transaction filter.
     *
     * @param script         The filter JS code.
     * @param main_name      The expected name of the filtering function in the script.
     * @param callback_names A list of callback function names to register for the filter.
     *                       If empty, register no callback functions.
     * @param filter         The user-defined transaction filter to initialize.
     * @param strResult      Reason for failure if unsuccessful.
     * @return               MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                     mc_Filter *filter, std::string &strResult);

    /**
     * Create a filter with an execution timeout.
     *
     * @param script         The filter JS code.
     * @param main_name      The expected name of the filtering function in the script.
     * @param callback_names A list of callback function names to register for the filter.
     *                       If empty, register no callback functions.
     * @param filter         The user-defined transaction filter to initialize.
     * @param timeout        The execution timeout, in milliseconds.
     * @param strResult      Reason for failure if unsuccessful.
     * @return               MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                     mc_Filter *filter, int timeout, std::string &strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param filter            The user-defined transaction filter to use.
     * @param strResult         Reason for script failure or transaction rejection.
     * @param createCallbackLog Indicate if a callbacklog is requested.
     * @param callbacks         An array of RPC callback call data.
     * @return                  MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int RunFilter(const mc_Filter *filter, std::string &strResult, bool createCallbackLog = false,
                  json_spirit::Array *callbacks = nullptr);

    /**
     * Run the filter function in the JS script.
     *
     * This variant provides detailed data about RPC callback calls: parameters, results, success/failure and errors.
     *
     * @param filter    The user-defined transaction filter to use.
     * @param strResult Reason for script failure or transaction rejection.
     * @param callbacks An array of RPC callback call data.
     * @return          MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int RunFilterWithCallbackLog(const mc_Filter *filter, std::string &strResult, json_spirit::Array *callbacks);

    /**
     * Abort the currently running filter (if any).
     *
     * @param reason    The reason the filter is being terminated.
     */
    void TerminateFilter(std::string reason);

  private:
    void *m_Impl;
    const mc_Filter *m_runningFilter;
    Watchdog *m_watchdog;
    FilterCallback m_filterCallback;

    void SetRunningFilter(const mc_Filter *filter);
    void ResetRunningFilter();
}; // class mc_FilterEngine

#endif /* MULTICHAIN_FILTER_H */
