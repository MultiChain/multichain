// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef FILTERCALLBACK_H
#define FILTERCALLBACK_H

#include "filters/ifiltercallback.h"
#include "json/json_spirit.h"

/**
 * A filter callback object that can create a callback log for debugging.
 */
class FilterCallback : public IFilterCallback
{
  public:
    virtual ~FilterCallback() override
    {
    }

    /**
     * Get the debugging callback log.
     */
    json_spirit::Array GetCallbackLog() const
    {
        return m_callbackLog;
    }

    /**
     * Set the flasg for creating a debugging callback log.
     *
     * @param value The value to set to.
     */
    void SetCreateCallbackLog(bool value = true)
    {
        m_createCallbackLog = value;
    }

    /**
     * Clear the debugging callback log.
     */
    void ResetCallbackLog()
    {
        m_callbackLog.clear();
    }

#ifdef WIN32
    /**
     * Callback using UBJSON to pass arguments and a return value.
     *
     * @param name          The name of the API function to invoke.
     * @param argsBlob      The UBJSON content of the function arguments.
     * @param resultBlob    The UBJSON content of the function return value.
     */
    void UbjCallback(const char *name, Blob_t* argsBlob, Blob_t* resultBlob) override;
#else
    /**
     * Callback using json_spirit to pass arguments and a return value.
     *
     * @param name      The name of the API function to invoke.
     * @param args      The function arguments.
     * @param result    The function return value.
     */
    void JspCallback(std::string name, json_spirit::Array args, json_spirit::Value &result) override;
#endif // WIN32

  private:
    json_spirit::Array m_callbackLog;
    bool m_createCallbackLog = false;

    void CreateCallbackLog(std::string name, json_spirit::Array args, json_spirit::Value result);
    void CreateCallbackLogError(std::string name, json_spirit::Array args, json_spirit::Object &e);
    void CreateCallbackLogError(std::string name, json_spirit::Array args, std::exception &e);
}; // class FilterCallback

#endif // FILTERCALLBACK_H
