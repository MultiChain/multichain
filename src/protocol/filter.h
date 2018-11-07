// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTER_H
#define MULTICHAIN_FILTER_H

#include "json/json_spirit.h"

class mc_FilterEngine;
class mc_FilterWatchdog;

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

    int CreateFilter(std::string script, std::string main_name, std::vector<std::string> &callback_names,
                     mc_Filter *filter, int timeout, std::string &strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param filter    The user-defined transaction filter to use.
     * @param strResult Reason for script failure or transaction rejection.
     * @return          MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int RunFilter(const mc_Filter *filter, std::string &strResult);

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
    int RunFilterWithCallbackLog(const mc_Filter *filter, std::string &strResult, json_spirit::Array &callbacks);

    /**
     * Abort the currently running filter (if any).
     *
     * @param reason    The reason the filter is being terminated.
     */
    void TerminateFilter(std::string reason);

  private:
    void *m_Impl;
    const mc_Filter *m_runningFilter;
    mc_FilterWatchdog *m_watchdog;

    void SetRunningFilter(const mc_Filter *filter);
    void ResetRunningFilter();
}; // class mc_FilterEngine

/**
 * @brief Monitor filter execution and stop the filer function if it takes more than a specified timeout.
 */
class mc_FilterWatchdog
{
  public:
    mc_FilterWatchdog()
    {
        Zero();
    }
    ~mc_FilterWatchdog()
    {
        Destroy();
    }

    void Zero();
    int Destroy();

    int Initialize();

    /**
     * @brief Notfies the watchdog that a filter started runnug, with a given timeout.
     * @param timeout   The number of millisecond to allow the filtr to run.
     */
    void FilterStarted(int timeout = 1000);

    /**
     * @brief Notfies the watchdog that a filter stopped running.
     */
    void FilterEnded();

    /**
     * @brief Terminate the watchdog.
     */
    void Shutdown();

  private:
    void *m_Impl;
};

#endif /* MULTICHAIN_FILTER_H */
