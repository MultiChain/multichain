// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_V8ENGINE_H_
#define MULTICHAIN_V8ENGINE_H_

#include <v8.h>
#include <libplatform/libplatform.h>

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
     * @param script    The filter JS code.
     * @param main_name The expected name of the filtering function in the script.
     * @param filter    The filter object to initialize.
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int CreateFilter(std::string script, std::string main_name, V8Filter* filter, std::string& strResult);

private:

    std::unique_ptr<v8::Platform> m_platform;
};

} // namespace mc_v8

#endif /* MULTICHAIN_V8ENGINE_H_ */
