// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8filter.h"
#include "v8_win/callbacks.h"
#include "v8_win/v8engine.h"
#include "v8_win/v8utils.h"

namespace mc_v8
{
// clang-format off
static std::string jsFixture = R"(
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
)";

static std::string jsFixtureDateFunctions = R"(
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

Date.prototype.getDate = function() {return 0;};
Date.prototype.getFullYear = function() {return 0;};
Date.prototype.getHours = function() {return 0;};
Date.prototype.getMonth = function() {return 0;};
Date.prototype.getMinutes = function() {return 0;};
Date.prototype.getDay = function() {return 0;};
Date.prototype.getYear = function() {return 0;};
Date.prototype.getSeconds = function() {return 0;};
Date.prototype.getMilliseconds = function() {return 0;};
Date.prototype.getTimezoneOffset = function() {return 0;};

Date.prototype.toDateString = function() {return "";};
Date.prototype.toGMTString = function() {return "";};
Date.prototype.toISOString = function() {return "";};
Date.prototype.toJSON = function() {return "";};
Date.prototype.toLocaleDateString = function() {return "";};
Date.prototype.toLocaleFormat = function() {return "";};
Date.prototype.toLocaleString = function() {return "";};
Date.prototype.toLocaleTimeString = function() {return "";};
Date.prototype.toString = function() {return "";};
Date.prototype.toTimeString = function() {return "";};
Date.prototype.toUTCString = function() {return "";};

Date.prototype.setDate = function() {return 0;};
Date.prototype.setFullYear = function() {return 0;};
Date.prototype.setHours = function() {return 0;};
Date.prototype.setMinutes = function() {return 0;};
Date.prototype.setMonth = function() {return 0;};
Date.prototype.setYear = function() {return 0;};
Date.prototype.setSeconds = function() {return 0;};
Date.prototype.setMilliseconds = function() {return 0;};

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
        if (arguments.length >= 0) {
            arguments = [0];
        }
        return instantiate(Date, arguments);
    }
}(Date);
)";

static std::string jsLimitMathSet = R"(
var mathKeep = new Set(["abs", "ceil", "floor", "max", "min", "round", "sign", "trunc", "log", "log10", "log2", "pow",
    "sqrt", "E", "LN10", "LN2", "LOG10E", "LOG2E", "PI", "SQRT1_2", "SQRT2" ]);
for (var fn of Object.getOwnPropertyNames(Math)) {
    if (! mathKeep.has(fn)) {
        delete Math[fn];
    }
}
delete Date.now;
)";

static std::string jsDeleteDateParse = R"(
delete Date.parse;
)";

// clang-format on

V8Filter::~V8Filter()
{
    if (m_isRunning)
    {
        m_engine->GetIsolate()->TerminateExecution();
    }
    m_filterFunction.Reset();
    m_context.Reset();
}

int V8Filter::Initialize(V8Engine *engine, std::string script, std::string functionName,
                         const std::vector<std::string> &callbackNames, int jsInjectionParams,
                         std::string &strResult)
{
    logger->debug("V8Filter::Initialize - enter");

    m_engine = engine;
    v8::Isolate *isolate = m_engine->GetIsolate();
    strResult.clear();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    auto global = v8::ObjectTemplate::New(isolate);
    auto filterCallback = v8::External::New(isolate, m_engine->GetFilterCallback());
    logger->debug("  Processing RPC callbacks");
    for (std::string functionName : callbackNames)
    {
        if (callbackLookup.find(functionName) == callbackLookup.end())
        {
            logger->error("Undefined callback name: {}", functionName);
            strResult = strprintf("Undefined callback name: {}", functionName);
            return MC_ERR_INTERNAL_ERROR;
        }
        logger->debug("    RPC callback: {}", functionName);
        global->Set(String2V8(isolate, functionName),
                    v8::FunctionTemplate::New(isolate, callbackLookup[functionName], filterCallback));
    }

    logger->debug("  Prepare context");
    auto context = v8::Context::New(isolate, nullptr, global);
    m_context.Reset(isolate, context);

    std::string jsPreamble = jsFixture;
    if(jsInjectionParams & MC_V8W_JS_INJECTION_FIXED_DATE_FUNCTIONS)
    {
        jsPreamble=jsFixtureDateFunctions;
    }
    if(jsInjectionParams & MC_V8W_JS_INJECTION_LIMITED_MATH_SET)
    {
        jsPreamble += jsLimitMathSet;
    }
    if(jsInjectionParams & MC_V8W_JS_INJECTION_DISABLED_DATE_PARSE)
    {
        jsPreamble += jsDeleteDateParse;
    }

    logger->debug("  Processing preamble");
    int status = this->CompileAndLoadScript(jsPreamble, "", "preamble", strResult);
    if (status != MC_ERR_NOERROR || !strResult.empty())
    {
        logger->error("    error={} strResult='{}'", status, strResult);
        m_context.Reset();
        return status;
    }

    logger->debug("  Processing <script>");
    status = this->CompileAndLoadScript(script, functionName, "<script>", strResult);
    if (status != MC_ERR_NOERROR)
    {
        logger->error("    error={} strResult='{}'", status, strResult);
        m_context.Reset();
    }
    return status;
}

int V8Filter::Run(std::string &strResult)
{
    logger->debug("V8Filter::Run - enter");

    strResult.clear();
    if (m_context.IsEmpty() || m_filterFunction.IsEmpty())
    {
        strResult = "Trying to run an invalid filter";
        return MC_ERR_NOERROR;
    }
    v8::Isolate *isolate = m_engine->GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::TryCatch tryCatch(isolate);
    auto context = v8::Local<v8::Context>::New(isolate, m_context);
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Value> result;
    auto filterFunction = v8::Local<v8::Function>::New(isolate, m_filterFunction);
    logger->debug( "  Call filter function");
    m_isRunning = true;
    bool ok = filterFunction->Call(context, context->Global(), 0, nullptr).ToLocal(&result);
    m_isRunning = false;
    logger->debug("  Filter function done");
    if (!ok)
    {
        if (tryCatch.Exception()->IsNull() && tryCatch.Message().IsEmpty())
        {
            strResult = m_engine->TerminationReason();
        }
        else
        {
            this->ReportException(&tryCatch, strResult);
        }
        return MC_ERR_NOERROR;
    }

    if (result->IsString())
    {
        strResult = V82String(isolate, result);
    }

    logger->debug("V8Filter::Run - leave");
    return MC_ERR_NOERROR;
}

int V8Filter::CompileAndLoadScript(std::string script, std::string functionName, std::string source,
                                   std::string &strResult)
{
    logger->debug("V8Filter::CompileAndLoadScript(source={}) - enter", source);

    strResult.clear();
    v8::Isolate *isolate = m_engine->GetIsolate();
    v8::HandleScope handleScope(isolate);
    v8::TryCatch tryCatch(isolate);
    auto context = v8::Local<v8::Context>::New(isolate, m_context);
    v8::Context::Scope contextScope(context);
    v8::ScriptOrigin scriptOrigin(String2V8(isolate, source));
    v8::Local<v8::String> v8script = String2V8(isolate, script);

    logger->debug("  Compile code");
    v8::Local<v8::Script> compiledScript;
    if (!v8::Script::Compile(context, v8script, &scriptOrigin).ToLocal(&compiledScript))
    {
        this->ReportException(&tryCatch, strResult);
        return MC_ERR_NOERROR;
    }

    logger->debug("  Run code");
    v8::Local<v8::Value> result;
    try
    {
        if (!compiledScript->Run(context).ToLocal(&result))
        {
            this->ReportException(&tryCatch, strResult);
            return MC_ERR_NOERROR;
        }
    }
    catch (std::exception &e)
    {
        logger->error("  Unknown error in ->Run(): {}", e.what());
        return MC_ERR_NOERROR;
    }
    catch (...)
    {
        logger->error("  Unknown error in ->Run()");
        return MC_ERR_NOERROR;
    }

    if (!functionName.empty())
    {
        logger->debug("  Locate function '{}'", functionName);
        v8::Local<v8::String> processName = String2V8(isolate, functionName);
        v8::Local<v8::Value> processVal;
        if (!context->Global()->Get(context, processName).ToLocal(&processVal) || !processVal->IsFunction())
        {
            logger->warn("Cannot find function '{}' in script", functionName);
            strResult = tfm::format("Cannot find function '%s' in script", functionName);
            return MC_ERR_NOERROR;
        }
        m_filterFunction.Reset(isolate, v8::Local<v8::Function>::Cast(processVal));
    }
    logger->debug("V8Filter::CompileAndLoadScript - leave strResult='{}'", strResult);
    return MC_ERR_NOERROR;
}

void V8Filter::ReportException(v8::TryCatch *tryCatch, std::string &strResult)
{
    logger->debug("V8Filter: ReportException - enter");

    v8::Isolate *isolate = m_engine->GetIsolate();
    v8::HandleScope handlecope(isolate);
    strResult = V82String(isolate, tryCatch->Exception());
    v8::Local<v8::Message> message = tryCatch->Message();
    if (message.IsEmpty())
    {
        logger->debug(strResult);
    }
    else
    {
        std::string filename = V82String(isolate, message->GetScriptResourceName());
        auto context = v8::Local<v8::Context>::New(isolate, m_context);
        v8::Context::Scope contextScope(context);
        int linenum = message->GetLineNumber(context).FromJust();
        int start = message->GetStartColumn(context).FromJust();
        int end = message->GetEndColumn(context).FromJust();
        std::string sourceline = V82String(isolate, message->GetSourceLine(context).ToLocalChecked());
        logger->debug("{}:{} {}", filename, linenum, strResult);
        logger->debug(sourceline);
        logger->debug("{}{}", std::string(static_cast<std::string::size_type>(start), ' '),
                      std::string(static_cast<std::string::size_type>(end - start), '^'));
    }
}
} // namespace mc_v8
