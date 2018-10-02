// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_V8ENGINE_H_
#define MULTICHAIN_V8ENGINE_H_

#include "json/json_spirit.h"
#include <v8.h>

namespace mc_v8
{

class V8Filter;

/**
 * Interface to the Google V8 engine to create and run transaction filters.
 */
class V8Engine
{
public:
    V8Engine()
    {
        Zero();
    }

    ~V8Engine()
    {
        Destroy();
    }

    void Zero();
    int Destroy();

    /**
     * Initialize the V8 engine.
     *
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(std::string& strResult);

    /**
     * Create a new transaction filter.
     *
     * @param script         The filter JS code.
     * @param main_name      The expected name of the filtering function in the script.
     * @param callback_names A list of callback function names to register for the filter.
     *                       If empty, register all possible functions.
     * @param filter         The filter object to initialize.
     * @param strResult      Reason for failure if unsuccessful.
     * @return               MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int CreateFilter(std::string script, std::string main_name, std::vector<std::string>& callback_names,
            V8Filter* filter, std::string& strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param filter    The user-defined transaction filter to use.
     * @param strResult Reason for script failure or transaction rejection.
     * @return          MC_ERR_INTERNAL_ERROR if the engine failed, MC_ERR_NOERROR otherwise.
     */
    int RunFilter(V8Filter* filter, std::string& strResult);

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
    int RunFilterWithCallbackLog(V8Filter* filter, std::string& strResult, json_spirit::Array& callbacks);
};

} // namespace mc_v8

#endif /* MULTICHAIN_V8ENGINE_H_ */
