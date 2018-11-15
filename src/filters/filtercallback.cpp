#include "filters/filtercallback.h"
#include "rpc/rpcserver.h"
#include "utils/util.h"
#include "json/json_spirit_ubjson.h"
#include "json/json_spirit_writer.h"

const int MAX_DEPTH = 100;
#define CALLBACK_LOOKUP(name) { #name, &name }

static std::map<std::string, rpcfn_type> FilterCallbackFunctions{
    CALLBACK_LOOKUP(getfiltertxid),
    CALLBACK_LOOKUP(getfiltertransaction),
    CALLBACK_LOOKUP(getfilterstreamitem),
    CALLBACK_LOOKUP(getfilterassetbalances),
    CALLBACK_LOOKUP(setfilterparam),
    CALLBACK_LOOKUP(getfiltertxinput),
    CALLBACK_LOOKUP(getlastblockinfo),
    CALLBACK_LOOKUP(getassetinfo),
    CALLBACK_LOOKUP(getstreaminfo),
    CALLBACK_LOOKUP(verifypermission),
    CALLBACK_LOOKUP(verifymessage)
};

#ifdef WIN32
void FilterCallback::UbjCallback(const char *name, const unsigned char *args, unsigned char **result, size_t *resultSize)
{
    int err;
    Array jspArgs = ubjson_read(args, 1, MAX_DEPTH, &err).get_array();
    Value jspResult;
    try
    {
        jspResult = FilterCallbackFunctions[name](jspArgs, false);
        this->CreateCallbackLog(name, jspArgs, jspResult);
    }
    catch (Object &e)
    {
        this->CreateCallbackLogError(name, jspArgs, e);
    }
    catch (exception &e)
    {
        this->CreateCallbackLogError(name, jspArgs, e);
    }
    mc_Script script;
    script.AddElement();
    ubjson_write(jspResult, &script, MAX_DEPTH);
    unsigned char *result_;
    int resultSize_;
    if (script.GetRawData(&result_, &resultSize_) == MC_ERR_NOERROR)
    {
        *resultSize = static_cast<size_t>(resultSize_);
        *result = new unsigned char[*resultSize];
        memcpy(*result, result_, *resultSize);
    }
}
#else
void FilterCallback::JspCallback(string name, Array args, Value &result)
{
    result = Value::null;
    try
    {
        result = FilterCallbackFunctions[name](args, false);
        this->CreateCallbackLog(name, args, result);
    }
    catch (Object &e)
    {
        this->CreateCallbackLogError(name, args, e);
    }
    catch (exception &e)
    {
        this->CreateCallbackLogError(name, args, e);
    }
}
#endif // WIN32

void FilterCallback::CreateCallbackLog(string name, Array args, Value result)
{
    if (!m_createCallbackLog)
        return;

    Object callbackData;
    callbackData.push_back(Pair("method", name));
    callbackData.push_back(Pair("params", args));

    bool success = true;
    if (result.type() == obj_type)
    {
        auto obj = result.get_obj();
        auto it = std::find_if(obj.begin(), obj.end(), [](const Pair &pair) -> bool { return pair.name_ == "code"; });
        success = (it == obj.end());
    }
    callbackData.push_back(Pair("success", success));
    callbackData.push_back(Pair(success ? "result" : "error", result));
    m_callbackLog.push_back(callbackData);
}

void FilterCallback::CreateCallbackLogError(string name, Array args, Object &e)
{
    if (!m_createCallbackLog)
        return;

    Object callbackData;
    callbackData.push_back(Pair("method", name));
    callbackData.push_back(Pair("params", args));

    callbackData.push_back(json_spirit::Pair("success", false));
    callbackData.push_back(json_spirit::Pair("error", e));

    m_callbackLog.push_back(callbackData);
}

void FilterCallback::CreateCallbackLogError(string name, Array args, exception &e)
{
    if (!m_createCallbackLog)
        return;

    Object callbackData;
    callbackData.push_back(Pair("method", name));
    callbackData.push_back(Pair("params", args));

    callbackData.push_back(json_spirit::Pair("success", false));
    callbackData.push_back(json_spirit::Pair("error", e.what()));

    m_callbackLog.push_back(callbackData);
}
