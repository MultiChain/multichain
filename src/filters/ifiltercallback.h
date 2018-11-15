#ifndef IFILTERCALLBACK_H
#define IFILTERCALLBACK_H

#ifndef WIN32
#include "json/json_spirit_value.h"
#endif // WIN32

class IFilterCallback
{
  public:
//#ifdef WIN32
    virtual void UbjCallback(const char *name, const unsigned char *args, unsigned char **result, size_t *resultSize) = 0;
//#else
    virtual void JspCallback(std::string name, json_spirit::Array args, json_spirit::Value &result) = 0;
//#endif // WIN32

    virtual ~IFilterCallback()
    {
    }
}; // class IFilterCallback

#endif // IFILTERCALLBACK_H
