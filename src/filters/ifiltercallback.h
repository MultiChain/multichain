// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef IFILTERCALLBACK_H
#define IFILTERCALLBACK_H

#include <cstddef>

#ifndef WIN32
#include "json/json_spirit.h"
#else
#include "v8_win/v8_win.h"
#endif // WIN32

/**
 * Interface for platform-dependent filter callback API functions.
 */
class IFilterCallback
{
  public:
#ifdef WIN32
    /**
     * Callback using UBJSON to pass arguments and a return value.
     *
     * @param name          The name of the API function to invoke.
     * @param argsBlob      The UBJSON content of the function arguments.
     * @param resultBlob    The UBJSON content of the function return value.
     */
    virtual void UbjCallback(const char *name, Blob_t* argsBlob, Blob_t* resultBlob) = 0;
#else
    /**
     * Callback using json_spirit to pass arguments and a return value.
     *
     * @param name      The name of the API function to invoke.
     * @param args      The function arguments.
     * @param result    The function return value.
     */
    virtual void JspCallback(std::string name, json_spirit::Array args, json_spirit::Value &result) = 0;
#endif // WIN32

    virtual ~IFilterCallback()
    {
    }
}; // class IFilterCallback

#endif // IFILTERCALLBACK_H
