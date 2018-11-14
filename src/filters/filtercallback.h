#ifndef FILTERCALLBACK_H
#define FILTERCALLBACK_H

#include "filters/ifiltercallback.h"

class FilterCallback : public IFilterCallback
{
  public:
    virtual ~FilterCallback() override
    {
    }

    json_spirit::Array GetCallbackLog() const
    {
        return m_callbackLog;
    }

    void SetCreateCallbackLog(bool value = true)
    {
        m_createCallbackLog = value;
    }

    void ResetCallbackLog()
    {
        m_callbackLog.clear();
    }

    void UbjCallback(const char *name, const unsigned char *args, unsigned char **result, int *resultSize) override;
    void JspCallback(std::string name, json_spirit::Array args, json_spirit::Value &result) override;

  private:
    json_spirit::Array m_callbackLog;
    bool m_createCallbackLog = false;

    void CreateCallbackLog(std::string name, json_spirit::Array args, json_spirit::Value result);
    void CreateCallbackLogError(std::string name, json_spirit::Array args, json_spirit::Object &e);
    void CreateCallbackLogError(std::string name, json_spirit::Array args, std::exception &e);
}; // class FilterCallback

#endif // FILTERCALLBACK_H
