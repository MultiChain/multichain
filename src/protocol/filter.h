// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTER_H
#define MULTICHAIN_FILTER_H

#include <string>

class mc_FilterEngine;

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

    /**
     * Initialize the transaction filter.
     *
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(std::string &strResult);

private:
    void *m_Impl;

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
     * @param script    The filter JS code.
     * @param main_name The expected name of the filtering function in the script.
     * @param filter    The user-defined transaction filter to initialize.
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int CreateFilter(std::string script, std::string main_name, mc_Filter* filter, std::string& strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param filter    The user-defined transaction filter to use.
     * @param strResult Reason for script failure or transaction rejection.
     * @return          MC_ERR_NOERROR if the transaction was accepted, MC_ERR_INTERNAL_ERROR if not.
     */
    int RunFilter(const mc_Filter& filter, std::string& strResult);

private:
    void *m_Impl;

}; // class mc_FilterEngine

#endif /* MULTICHAIN_FILTER_H */
