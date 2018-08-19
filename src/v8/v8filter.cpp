// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8filter.h"
#include "v8utils.h"
#include "callbacks.h"
#include "utils/define.h"
#include "utils/util.h"
#include <cassert>

namespace mc_v8
{

V8IsolateManager* V8IsolateManager::m_instance = nullptr;

V8IsolateManager* V8IsolateManager::Instance()
{
    if (m_instance == nullptr)
    {
        m_instance = new V8IsolateManager();
    }
    return m_instance;
}

v8::Isolate* V8IsolateManager::GetIsolate()
{
    if (m_isolate == nullptr)
    {
        m_createParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        m_isolate = v8::Isolate::New(m_createParams);
    }
    return m_isolate;
}

V8IsolateManager::~V8IsolateManager()
{
    if (m_isolate != nullptr)
    {
        m_isolate->Dispose();
        m_isolate = nullptr;
        delete m_createParams.array_buffer_allocator;
    }
}

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
//    LogPrintf("V8Filter: Zero\n");
    m_isolate = nullptr;
}

int V8Filter::Destroy()
{
//    LogPrintf("V8Filter: Destroy\n");
    m_filterFunction.Reset();
    m_context.Reset();
    this->Zero();
    return MC_ERR_NOERROR;
}

#define REGISTER_RPC(name) global->Set(String2V8(m_isolate, #name), v8::FunctionTemplate::New(m_isolate, filter_##name))

int V8Filter::Initialize(std::string script, std::string functionName, std::string& strResult)
{
    LogPrintf("V8Filter: Initialize\n");
    strResult.clear();
    this->MaybeCreateIsolate();
    v8::Locker locker(m_isolate);
    v8::Isolate::Scope isolateScope(m_isolate);
    v8::HandleScope handleScope(m_isolate);
    auto global = v8::ObjectTemplate::New(m_isolate);
    REGISTER_RPC(getfiltertxid);
    REGISTER_RPC(getrawtransaction);
    REGISTER_RPC(gettxout);
    REGISTER_RPC(listassets);
    REGISTER_RPC(liststreams);
    REGISTER_RPC(listpermissions);
    auto context = v8::Context::New(m_isolate, nullptr, global);
    m_context.Reset(m_isolate, context);

    int status = this->CompileAndLoadScript(jsFixture, "", "fixture", strResult);
    if (status == MC_ERR_NOERROR && !script.empty())
    {
        status = this->CompileAndLoadScript(script, functionName, "<script>", strResult);
    }
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
    LogPrintf("V8Filter::Run\n");

    strResult.clear();
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
        return MC_ERR_INTERNAL_ERROR;
    }

    if (result->IsString())
    {
        strResult = V82String(m_isolate, result);
        if (!strResult.empty())
        {
            return MC_ERR_INTERNAL_ERROR;
        }
    }

    return MC_ERR_NOERROR;
}

int V8Filter::CompileAndLoadScript(std::string script, std::string functionName, std::string source,
        std::string& strResult)
{
    LogPrintf("V8Filter: CompileAndLoadScript %s\n", source.c_str());

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
        return MC_ERR_INTERNAL_ERROR;
    }

    v8::Local<v8::Value> result;
    if (!compiledScript->Run(context).ToLocal(&result))
    {
        assert(tryCatch.HasCaught());
        this->ReportException(&tryCatch, strResult);
        return MC_ERR_INTERNAL_ERROR;
    }

    if (!functionName.empty())
    {
        v8::Local<v8::String> processName = String2V8(m_isolate, functionName);
        v8::Local<v8::Value> processVal;
        if (!context->Global()->Get(context, processName).ToLocal(&processVal) || !processVal->IsFunction())
        {
            LogPrintf("V8Filter: Cannot find function '%s' in script\n", functionName.c_str());
            return MC_ERR_INTERNAL_ERROR;
        }
        m_filterFunction.Reset(m_isolate, v8::Local<v8::Function>::Cast(processVal));
    }
    return MC_ERR_NOERROR;
}

void V8Filter::ReportException(v8::TryCatch* tryCatch, std::string& strResult)
{
    LogPrintf("V8Filter: ReportException\n");
    v8::HandleScope handlecope(m_isolate);
    strResult = V82String(m_isolate, tryCatch->Exception());
    v8::Local<v8::Message> message = tryCatch->Message();
    if (message.IsEmpty())
    {
        LogPrintf("V8Filter: %s\n", strResult.c_str());
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
        LogPrintf("V8Filter: %s:%d %s\n", filename.c_str(), linenum, strResult.c_str());
        std::string sourceline = V82String(m_isolate, message->GetSourceLine(context).ToLocalChecked());
        LogPrintf("V8Filter: %s\n", sourceline.c_str());
        LogPrintf("V8Filter: %s%s\n", std::string(start, ' ').c_str(), std::string(end - start, '^').c_str());
//        v8::Local<v8::Value> stackTraceString;
//        if (try_catch->StackTrace(context).ToLocal(&stackTraceString) && stackTraceString->IsString()
//                && v8::Local<v8::String>::Cast(stackTraceString)->Length() > 0)
//        {
//            std::string stackTrace = V82String(m_isolate, stackTraceString);
//            LogPrintf("V8Filter: %s\n", stackTrace.c_str());
//        }
    }
}

} // namespace mc_v8
