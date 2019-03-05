// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "filters/filtercallback.h"

#ifdef WIN32
void FilterCallback::UbjCallback(const char *, Blob_t*, Blob_t*)
{
}
#else
void FilterCallback::JspCallback(std::string, json_spirit::Array, json_spirit::Value &)
{
}
#endif // WIN32

void FilterCallback::CreateCallbackLog(std::string, json_spirit::Array, json_spirit::Value)
{
}

void FilterCallback::CreateCallbackLogError(std::string, json_spirit::Array, json_spirit::Object &)
{
}

void FilterCallback::CreateCallbackLogError(std::string, json_spirit::Array, std::exception &)
{
}
