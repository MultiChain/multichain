// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/v8json_spirit.h"
#include "v8_win/v8utils.h"

namespace mc_v8
{
v8::Local<v8::Value> Jsp2V8(v8::Isolate *isolate, const json_spirit::Value &j)
{
    v8::Isolate::Scope isolateScope(isolate);
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Value> result = v8::Null(isolate);
    switch (j.type())
    {
    case json_spirit::obj_type:
    {
        auto v8obj = v8::Object::New(isolate);
        for (json_spirit::Pair property : j.get_obj())
        {
            v8obj->Set(String2V8(isolate, property.name_), Jsp2V8(isolate, property.value_));
        }
        result = v8obj;
        break;
    }

    case json_spirit::array_type:
    {
        auto jspArray = j.get_array();
        auto v8array = v8::Array::New(isolate, static_cast<int>(jspArray.size()));
        for (unsigned i = 0; i < jspArray.size(); ++i)
        {
            v8array->Set(i, Jsp2V8(isolate, jspArray[i]));
        }
        result = v8array;
        break;
    }

    case json_spirit::str_type:
        result = String2V8(isolate, j.get_str());
        break;

    case json_spirit::bool_type:
        result = v8::Boolean::New(isolate, j.get_bool());
        break;

    case json_spirit::int_type:
        v8::Integer::New(isolate, j.get_int());
        break;

    case json_spirit::real_type:
        result = v8::Number::New(isolate, j.get_real());
        break;

    case json_spirit::null_type:
        result = v8::Null(isolate);
        break;
    };

    return handleScope.Escape(result);
}

json_spirit::Value V82Jsp(v8::Isolate *isolate, v8::Local<v8::Value> v)
{
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope contextScope(context);

    if (v.IsEmpty())
    {
        return json_spirit::Value();
    }

    if (v->IsObject())
    {
        auto v8obj = v->ToObject(context).ToLocalChecked();
        v8::Local<v8::Array> v8propNames = v8obj->GetOwnPropertyNames(context).ToLocalChecked();
        json_spirit::Object jspObj;
        for (unsigned i = 0; i < v8propNames->Length(); ++i)
        {
            v8::Local<v8::String> v8propName =
                    v8propNames->Get(context, i).ToLocalChecked()->ToString(context).ToLocalChecked();
            v8::Local<v8::Value> v8value = v8obj->Get(context, v8propName).ToLocalChecked();
            jspObj.push_back(json_spirit::Pair(V82String(isolate, v8propName), V82Jsp(isolate, v8value)));
        }
        return jspObj;
    }

    if (v->IsArray())
    {
        auto v8array = v8::Local<v8::Array>::Cast(v);
        json_spirit::Array jspArray;
        for (unsigned i = 0; i < v8array->Length(); ++i)
        {
            v8::Local<v8::Value> v8value = v8array->Get(context, i).ToLocalChecked();
            jspArray.push_back(V82Jsp(isolate, v8value));
        }
        return jspArray;
    }

    if (v->IsString())
    {
        return json_spirit::Value(V82String(isolate, v));
    }

    if (v->IsBoolean())
    {
        return json_spirit::Value(v->BooleanValue(context).FromJust());
    }

    if (v->IsInt32())
    {
        return json_spirit::Value(v->Int32Value(context).FromJust());
    }

    if (v->IsNumber())
    {
        return json_spirit::Value(v->NumberValue(context).FromJust());
    }

    // if (fDebug)
    //    LogPrint("v8filter", "v8filter: V8 type '%s' is not recognized\n",
    //             V82String(isolate, v->TypeOf(isolate)).c_str());
    return json_spirit::Value();
}

} // namespace mc_v8
