// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8filter.h"
#include "v8utils.h"
#include "v8isolatemanager.h"
#include "callbacks.h"
#include "utils/define.h"
#include "utils/util.h"
#include <cassert>

namespace mc_v8
{


static std::string jsFixture =
        R"(
Math.random = function() {
    return 0;
};

Date.now = function() {
    return 0;
};

var bind = Function.bind;
var unbind = bind.bind(bind);

function instantiate(constructor, args) {
    return new (unbind(constructor, null).apply(null, args));
}

Date = function (Date) {
    var names = Object.getOwnPropertyNames(Date);
    // Loop through them
    for (var i = 0; i < names.length; i++) {
        // Skip props already in the MyDate object
        if (names[i] in MyDate) continue;
        // Get property description from o
        var desc = Object.getOwnPropertyDescriptor(Date, names[i]);
        // Use it to create property on MyDate
        Object.defineProperty(MyDate, names[i], desc);
    }

    return MyDate;

    function MyDate() {
        if (arguments.length == 0) {
            arguments = [0];
        }
        return instantiate(Date, arguments);
    }
}(Date);

console.log(`Math.randon()=${Math.random()}`);
console.log(`Date.now()=${Date.now()}`);
console.log(`Date()=${Date()}`);
console.log("Finished loading fixture.js");
)";

void V8Filter::Zero()
{
    m_isolate = nullptr;
}

int V8Filter::Destroy()
{
    m_filterFunction.Reset();
    m_context.Reset();
    this->Zero();
    return MC_ERR_NOERROR;
}

#define REGISTER_RPC(name) global->Set(String2V8(m_isolate, #name), v8::FunctionTemplate::New(m_isolate, filter_##name))

int V8Filter::Initialize(std::string script, std::string functionName, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Filter::Initialize\n");
    strResult.clear();
    this->MaybeCreateIsolate();
    v8::Locker locker(m_isolate);
    v8::Isolate::Scope isolateScope(m_isolate);
    v8::HandleScope handleScope(m_isolate);
    auto global = v8::ObjectTemplate::New(m_isolate);
    REGISTER_RPC(getfiltertxid);
    REGISTER_RPC(getfiltertransaction);
    REGISTER_RPC(setfilterparam);
    REGISTER_RPC(gettxout);
    REGISTER_RPC(getblock);
    REGISTER_RPC(getlastblockinfo);
    REGISTER_RPC(listassets);
    REGISTER_RPC(liststreams);
    REGISTER_RPC(listpermissions);
    auto context = v8::Context::New(m_isolate, nullptr, global);
    m_context.Reset(m_isolate, context);

    int status = this->CompileAndLoadScript(jsFixture, "", "fixture", strResult);
    if (status != MC_ERR_NOERROR || !strResult.empty())
    {
        m_context.Reset();
        return status;
    }
    status = this->CompileAndLoadScript(script, functionName, "<script>", strResult);
    if (status != MC_ERR_NOERROR)
    {
        m_context.Reset();
    }
    return status;
}

void V8Filter::MaybeCreateIsolate()
{
    if (m_isolate == nullptr)
    {
        m_isolate = V8IsolateManager::Instance()->GetIsolate();
    }
}

int V8Filter::Run(std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Filter::Run\n");

    strResult.clear();
    if (m_context.IsEmpty() || m_filterFunction.IsEmpty())
    {
        strResult = "Trying to run an invalid filter";
        return MC_ERR_NOERROR;
    }
    v8::Locker locker(m_isolate);
    v8::Isolate::Scope isolateScope(m_isolate);
    v8::HandleScope handleScope(m_isolate);
    v8::TryCatch tryCatch(m_isolate);
    auto context = v8::Local<v8::Context>::New(m_isolate, m_context);
    v8::Context::Scope contextScope(context);
    v8::Local<v8::Value> result;
    auto filterFunction = v8::Local<v8::Function>::New(m_isolate, m_filterFunction);
    if (!filterFunction->Call(context, context->Global(), 0, nullptr).ToLocal(&result))
    {
        assert(tryCatch.HasCaught());
        this->ReportException(&tryCatch, strResult);
        return MC_ERR_NOERROR;
    }

    if (result->IsString())
    {
        strResult = V82String(m_isolate, result);
    }

    return MC_ERR_NOERROR;
}

int V8Filter::CompileAndLoadScript(std::string script, std::string functionName, std::string source,
        std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Filter::CompileAndLoadScript %s\n", source);

    strResult.clear();
    v8::HandleScope handleScope(m_isolate);
    v8::TryCatch tryCatch(m_isolate);
    auto context = v8::Local<v8::Context>::New(m_isolate, m_context);
    v8::Context::Scope contextScope(context);
    v8::ScriptOrigin scriptOrigin(String2V8(m_isolate, source));
    v8::Local<v8::String> v8script = String2V8(m_isolate, script);

    v8::Local<v8::Script> compiledScript;
    if (!v8::Script::Compile(context, v8script, &scriptOrigin).ToLocal(&compiledScript))
    {
        assert(tryCatch.HasCaught());
        this->ReportException(&tryCatch, strResult);
        return MC_ERR_NOERROR;
    }

    v8::Local<v8::Value> result;
    if (!compiledScript->Run(context).ToLocal(&result))
    {
        assert(tryCatch.HasCaught());
        this->ReportException(&tryCatch, strResult);
        return MC_ERR_NOERROR;
    }

    if (!functionName.empty())
    {
        v8::Local<v8::String> processName = String2V8(m_isolate, functionName);
        v8::Local<v8::Value> processVal;
        if (!context->Global()->Get(context, processName).ToLocal(&processVal) || !processVal->IsFunction())
        {
            strResult = tfm::format("Cannot find function '%s' in script", functionName);
            return MC_ERR_NOERROR;
        }
        m_filterFunction.Reset(m_isolate, v8::Local<v8::Function>::Cast(processVal));
    }
    return MC_ERR_NOERROR;
}

void V8Filter::ReportException(v8::TryCatch* tryCatch, std::string& strResult)
{
    LogPrint("v8filter", "v8filter: V8Filter: ReportException\n");

    v8::HandleScope handlecope(m_isolate);
    strResult = V82String(m_isolate, tryCatch->Exception());
    v8::Local<v8::Message> message = tryCatch->Message();
    if (message.IsEmpty())
    {
        LogPrint("v8filter", "v8filter: %s", strResult);
    }
    else
    {
        std::string filename = V82String(m_isolate, message->GetScriptResourceName());
        auto context = v8::Local<v8::Context>::New(m_isolate, m_context);
        v8::Context::Scope contextScope(context);
        int linenum = message->GetLineNumber(context).FromJust();
        int start = message->GetStartColumn(context).FromJust();
        int end = message->GetEndColumn(context).FromJust();
        assert(linenum >= 0 && start >= 0 && end >= 0);
        LogPrint("v8filter", "v8filter: %s:%d %s\n", filename, linenum, strResult);
        std::string sourceline = V82String(m_isolate, message->GetSourceLine(context).ToLocalChecked());
        LogPrint("v8filter", "v8filter: %s\n", sourceline);
        LogPrint("v8filter", "v8filter: %s%s\n", std::string(start, ' '), std::string(end - start, '^'));
//        v8::Local<v8::Value> stackTraceString;
//        if (try_catch->StackTrace(context).ToLocal(&stackTraceString) && stackTraceString->IsString()
//                && v8::Local<v8::String>::Cast(stackTraceString)->Length() > 0)
//        {
//            std::string stackTrace = V82String(m_isolate, stackTraceString);
//            LogPrint("v8filter", "v8filter: %s\n", stackTrace);
//        }
    }
}

} // namespace mc_v8
