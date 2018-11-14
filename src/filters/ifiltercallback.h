#ifndef IFILTERCALLBACK_H
#define IFILTERCALLBACK_H

#include "json/json_spirit_value.h"

class IFilterCallback
{
  public:
    virtual void UbjCallback(const char *name, const unsigned char *args, unsigned char **result, int *resultSize) = 0;
    virtual void JspCallback(std::string name, json_spirit::Array args, json_spirit::Value &result) = 0;

    virtual ~IFilterCallback()
    {
    }
}; // class IFilterCallback

using IFilterCallbackPtr = std::shared_ptr<IFilterCallback>;

#endif // IFILTERCALLBACK_H
