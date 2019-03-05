// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8/v8ubjson.h"
#include "utils/define.h"
#include "v8/v8blob.h"
#include "v8/v8utils.h"
#include <cstring>

const int MAX_FORMATTED_DATA_DEPTH = 100;
extern void _v8_internal_Print_Object(void *object);

namespace mc_v8
{
#define UBJ_UNDEFINED 0
#define UBJ_NULLTYPE 1
#define UBJ_NOOP 2
#define UBJ_BOOL_TRUE 3
#define UBJ_BOOL_FALSE 4
#define UBJ_CHAR 5
#define UBJ_STRING 6
#define UBJ_HIGH_PRECISION 7
#define UBJ_INT8 8
#define UBJ_UINT8 9
#define UBJ_INT16 10
#define UBJ_INT32 11
#define UBJ_INT64 12
#define UBJ_FLOAT32 13
#define UBJ_FLOAT64 14
#define UBJ_ARRAY 15
#define UBJ_OBJECT 16
#define UBJ_STRONG_TYPE 17
#define UBJ_COUNT 18

static char UBJ_TYPE[19] = {'?', 'Z', 'N', 'T', 'F', 'C', 'S', 'H', 'i', 'U',
                            'I', 'l', 'L', 'd', 'D', '[', '{', '$', '#'};
static int UBJ_SIZE[19] = {0, 0, 0, 0, 0, 1, -1, -1, 1, 1, 2, 4, 8, 4, 8, -2, -3, -4, -5};
static char UBJ_ISINT[19] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
// clang-format off
static int UBJ_INTERNAL_TYPE[256] =
{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, UBJ_COUNT, UBJ_STRONG_TYPE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, UBJ_CHAR, UBJ_FLOAT64, -1, UBJ_BOOL_FALSE, -1, UBJ_HIGH_PRECISION, UBJ_INT16, -1, -1, UBJ_INT64, -1, UBJ_NOOP, -1,
    -1, -1, -1, UBJ_STRING, UBJ_BOOL_TRUE, UBJ_UINT8, -1, -1, -1, -1, UBJ_NULLTYPE, UBJ_ARRAY, -1, -1, -1, -1,
    -1, -1, -1, -1, UBJ_FLOAT32, -1, -1, -1, -1, UBJ_INT8, -1, -1, UBJ_INT32, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, UBJ_OBJECT, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};
// clang-format on

const size_t size_t_max = std::numeric_limits<size_t>::max();

static void swap_bytes(void *ptrDst, void *ptrSrc, int size)
{
    int i;
    unsigned char *pDst;
    unsigned char *pSrc;
    pDst = static_cast<unsigned char *>(ptrDst);
    pSrc = static_cast<unsigned char *>(ptrSrc) + size - 1;
    for (i = 0; i < size; i++)
    {
        *pDst = *pSrc;
        pDst++;
        pSrc--;
    }
}

int ubjson_best_negative_int_type(int64_t int64_value)
{
    char type;
    type = UBJ_INT8;
    if (int64_value + 0x80 < 0)
    {
        if (int64_value + 0x8000 >= 0)
        {
            type = UBJ_INT16;
        }
        else
        {
            if (int64_value + 0x80000000 >= 0)
            {
                type = UBJ_INT32;
            }
            else
            {
                type = UBJ_INT64;
            }
        }
    }
    return type;
}

static int ubjson_best_int_type(int64_t int64_value, int *not_uint8)
{
    char type;
    uint64_t uint64_value;

    if (int64_value < 0)
    {
        if (not_uint8)
        {
            *not_uint8 = 1;
        }
        return ubjson_best_negative_int_type(int64_value);
    }

    uint64_value = *reinterpret_cast<uint64_t *>(&int64_value);
    if (uint64_value < 0x80)
    {
        type = UBJ_INT8;
    }
    else
    {
        if (uint64_value < 0x100)
        {
            type = UBJ_UINT8;
        }
        else
        {
            if (not_uint8)
            {
                *not_uint8 = 1;
            }
            if (uint64_value < 0x8000)
            {
                type = UBJ_INT16;
            }
            else
            {
                if (uint64_value < 0x80000000)
                {
                    type = UBJ_INT32;
                }
                else
                {
                    type = UBJ_INT64;
                }
            }
        }
    }

    return type;
}

static int ubjson_best_type(v8::Isolate *isolate, v8::Local<v8::Value> value, int last_type, int *not_uint8,
                            int64_t *usize, int64_t *ssize)
{
    v8::Isolate::Scope isolateScope(isolate);
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope contextScope(context);

    int64_t int64_value;
    int size;

    int type = UBJ_UNDEFINED;

    if (value->IsNullOrUndefined())
    {
        type = UBJ_NULLTYPE;
    }
    else if (value->IsBoolean())
    {
        type = UBJ_BOOL_FALSE;
        if (value->BooleanValue(context).FromJust())
        {
            type = UBJ_BOOL_TRUE;
        }
    }
    else if (value->IsInt32())
    {
        int64_value = value->Int32Value(context).FromJust();
        type = ubjson_best_int_type(int64_value, not_uint8);
    }
    else if (value->IsNumber())
    {
        type = UBJ_FLOAT64;
    }
    else if (value->IsString())
    {
        type = UBJ_STRING;
    }
    else if (value->IsArray())
    {
        type = UBJ_ARRAY;
    }
    else if (value->IsObject())
    {
        type = UBJ_OBJECT;
    }
    //    else
    //    {
    //        // non-covertible type
    //    }

    size = UBJ_SIZE[type];
    if (usize)
    {
        if (size >= 0)
        {
            *usize += size;
            if (ssize)
            {
                *ssize += size;
                if (type == UBJ_UINT8)
                {
                    *ssize += 1;
                }
            }
        }
    }

    if (last_type != UBJ_UNDEFINED)
    {
        if (type != last_type)
        {
            if (UBJ_ISINT[last_type] & UBJ_ISINT[type])
            {
                if (type < last_type)
                {
                    type = last_type;
                }
            }
            else
            {
                type = -1;
            }
        }
    }

    return type;
}

static int64_t ubjson_int64_read(const unsigned char *ptrStart, const unsigned char *ptrEnd, int known_type,
                                 size_t *offset, int *err)
{
    int ubj_type = known_type;
    int n, c;
    int64_t v;
    const unsigned char *ptr = ptrStart;
    unsigned char *ptrOut;

    v = 0;
    *err = MC_ERR_NOERROR;

    //    ptr = const_cast<unsigned char *>(ptrStart);

    if (ubj_type == UBJ_UNDEFINED)
    {
        if (ptr + 1 > ptrEnd)
        {
            *err = MC_ERR_ERROR_IN_SCRIPT;
            goto exitlbl;
        }
        ubj_type = UBJ_INTERNAL_TYPE[*ptr];
        if (ubj_type < 0)
        {
            *err = MC_ERR_ERROR_IN_SCRIPT;
            goto exitlbl;
        }
        ptr++;
    }

    if (UBJ_ISINT[ubj_type] == 0)
    {
        *err = MC_ERR_ERROR_IN_SCRIPT;
        goto exitlbl;
    }

    n = UBJ_SIZE[ubj_type];
    if (ptr + n > ptrEnd)
    {
        *err = MC_ERR_ERROR_IN_SCRIPT;
        goto exitlbl;
    }

    if (ubj_type != UBJ_UINT8)
    {
        if (*ptr & 0x80)
        {
            v = -1;
        }
    }

    ptrOut = static_cast<unsigned char *>(static_cast<void *>(&v)) + n - 1;

    for (c = 0; c < n; c++)
    {
        *ptrOut = *ptr;
        ptr++;
        ptrOut--;
    }

exitlbl:

    *offset = static_cast<size_t>(ptr - ptrStart);
    return v;
}

union Number {
    int64_t int64_value;
    float_t float_value;
    double_t double_value;
};

static v8::Local<v8::Value> ubjson_read_internal(v8::Isolate *isolate, const unsigned char *ptrStart, size_t bytes,
                                                 int known_type, int max_depth, size_t *offset, int *err)
{
    v8::Isolate::Scope isolateScope(isolate);
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope contextScope(context);

    int ubj_type;
    unsigned char *ptr;
    unsigned char *ptrEnd;
    size_t size, i, sh;
    union Number number;

    v8::Local<v8::Value> result = v8::Null(isolate);
    *err = MC_ERR_NOERROR;

    if (max_depth == 0)
    {
        *err = MC_ERR_NOT_SUPPORTED;
        goto exitlbl;
    }

    ptr = const_cast<unsigned char *>(ptrStart);
    ptrEnd = ptr + bytes;

    ubj_type = known_type;

    if (ubj_type == UBJ_UNDEFINED)
    {
        if (ptr + 1 > ptrEnd)
        {
            *err = MC_ERR_ERROR_IN_SCRIPT;
            goto exitlbl;
        }
        ubj_type = UBJ_INTERNAL_TYPE[*ptr];
        if (ubj_type < 0)
        {
            *err = MC_ERR_ERROR_IN_SCRIPT;
            goto exitlbl;
        }
        ptr++;
    }

    if (UBJ_ISINT[ubj_type])
    {
        int64_t int64_value = ubjson_int64_read(ptr, ptrEnd, ubj_type, &sh, err);
        if (sh <= sizeof(int32_t) + 1)
        {
            result = v8::Integer::New(isolate, static_cast<int32_t>(int64_value));
        }
        else
        {
            result = v8::BigInt::New(isolate, int64_value);
        }
        ptr += sh;
    }
    else
    {
        switch (ubj_type)
        {
        case UBJ_NULLTYPE:
            result = v8::Null(isolate);
            break;
        case UBJ_BOOL_FALSE:
            result = v8::Boolean::New(isolate, false);
            break;
        case UBJ_BOOL_TRUE:
            result = v8::Boolean::New(isolate, true);
            break;
        case UBJ_FLOAT32:
            if (ptr + UBJ_SIZE[ubj_type] > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            swap_bytes(&number.int64_value, ptr, sizeof(float));
            result = v8::Number::New(isolate, static_cast<double>(number.float_value));
            ptr += UBJ_SIZE[ubj_type];
            break;
        case UBJ_FLOAT64:
            if (ptr + UBJ_SIZE[ubj_type] > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            swap_bytes(&number.int64_value, ptr, sizeof(double));
            result = v8::Number::New(isolate, number.double_value);
            ptr += UBJ_SIZE[ubj_type];
            break;
        case UBJ_CHAR:
            if (ptr + UBJ_SIZE[ubj_type] > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            result = String2V8(isolate, std::string(ptr[0], 1));
            ptr += UBJ_SIZE[ubj_type];
            break;
        case UBJ_STRING:
        case UBJ_HIGH_PRECISION:
            size = static_cast<size_t>(ubjson_int64_read(ptr, ptrEnd, UBJ_UNDEFINED, &sh, err));
            if (*err)
            {
                goto exitlbl;
            }
            ptr += sh;
            if (ptr + size > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            if (size)
            {
                auto str_value = std::string(reinterpret_cast<char *>(ptr), size);
                result = String2V8(isolate, str_value);
            }
            else
            {
                result = String2V8(isolate, "");
            }
            ptr += size;
            break;
        case UBJ_ARRAY:

            if (max_depth <= 1)
            {
                *err = MC_ERR_NOT_SUPPORTED;
                goto exitlbl;
            }

            ubj_type = UBJ_UNDEFINED;
            if (ptr + 1 > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            size = size_t_max;
            if (*ptr == UBJ_TYPE[UBJ_STRONG_TYPE])
            {
                ptr++;
                if (ptr + 1 > ptrEnd)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
                ubj_type = UBJ_INTERNAL_TYPE[*ptr];
                if (ubj_type < 0)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
                ptr++;
                if (ptr + 1 > ptrEnd)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }

                if (*ptr != UBJ_TYPE[UBJ_COUNT])
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
            }
            if (*ptr == UBJ_TYPE[UBJ_COUNT])
            {
                ptr++;
                size = static_cast<size_t>(ubjson_int64_read(ptr, ptrEnd, UBJ_UNDEFINED, &sh, err));
                if (*err)
                {
                    goto exitlbl;
                }
                ptr += sh;
            }

            {
                auto array_value = v8::Array::New(isolate, (size < size_t_max) ? static_cast<int>(size) : 0);
                if (size < size_t_max)
                {
                    i = 0;
                    while (i < size)
                    {
                        array_value->Set(static_cast<unsigned>(i),
                                         ubjson_read_internal(isolate, ptr, static_cast<size_t>(ptrEnd - ptr), ubj_type,
                                                              max_depth - 1, &sh, err));
                        if (*err)
                        {
                            goto exitlbl;
                        }
                        ptr += sh;
                        i++;
                    }
                }
                else
                {
                    if (ptr + 1 > ptrEnd)
                    {
                        *err = MC_ERR_ERROR_IN_SCRIPT;
                        goto exitlbl;
                    }
                    i = 0;
                    while (*ptr != ']')
                    {
                        array_value->Set(static_cast<unsigned>(i++),
                                         ubjson_read_internal(isolate, ptr, static_cast<size_t>(ptrEnd - ptr), ubj_type,
                                                              max_depth - 1, &sh, err));
                        if (*err)
                        {
                            goto exitlbl;
                        }
                        ptr += sh;
                        if (ptr + 1 > ptrEnd)
                        {
                            *err = MC_ERR_ERROR_IN_SCRIPT;
                            goto exitlbl;
                        }
                    }
                    ptr++;
                }
                result = array_value;
            }
            break;

        case UBJ_OBJECT:

            if (max_depth <= 1)
            {
                *err = MC_ERR_NOT_SUPPORTED;
                goto exitlbl;
            }

            ubj_type = UBJ_UNDEFINED;
            if (ptr + 1 > ptrEnd)
            {
                *err = MC_ERR_ERROR_IN_SCRIPT;
                goto exitlbl;
            }
            size = size_t_max;
            if (*ptr == UBJ_TYPE[UBJ_STRONG_TYPE])
            {
                ptr++;
                if (ptr + 1 > ptrEnd)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
                ubj_type = UBJ_INTERNAL_TYPE[*ptr];
                if (ubj_type < 0)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
                ptr++;
                if (ptr + 1 > ptrEnd)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }

                if (*ptr != UBJ_TYPE[UBJ_COUNT])
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
            }
            if (*ptr == UBJ_TYPE[UBJ_COUNT])
            {
                ptr++;
                size = static_cast<size_t>(ubjson_int64_read(ptr, ptrEnd, UBJ_UNDEFINED, &sh, err));
                if (*err)
                {
                    goto exitlbl;
                }
                ptr += sh;
            }

            auto obj_value = v8::Object::New(isolate);
            if (size < size_t_max)
            {
                i = 0;
                while (i < size)
                {
                    v8::Local<v8::Value> string_value = ubjson_read_internal(
                        isolate, ptr, static_cast<size_t>(ptrEnd - ptr), UBJ_STRING, max_depth - 1, &sh, err);
                    if (*err)
                    {
                        goto exitlbl;
                    }
                    ptr += sh;

                    v8::Local<v8::Value> value_value = ubjson_read_internal(
                        isolate, ptr, static_cast<size_t>(ptrEnd - ptr), ubj_type, max_depth - 1, &sh, err);
                    obj_value->Set(string_value, value_value);
                    if (*err)
                    {
                        goto exitlbl;
                    }
                    ptr += sh;

                    i++;
                }
            }
            else
            {
                if (ptr + 1 > ptrEnd)
                {
                    *err = MC_ERR_ERROR_IN_SCRIPT;
                    goto exitlbl;
                }
                while (*ptr != '}')
                {
                    v8::Local<v8::Value> string_value = ubjson_read_internal(
                        isolate, ptr, static_cast<size_t>(ptrEnd - ptr), UBJ_STRING, max_depth - 1, &sh, err);
                    //                    std::cout << "ubjson_read_internal UBJ_OBJECT key=" << V82String(isolate,
                    //                    string_value)
                    //                              << std::endl;
                    if (*err)
                    {
                        goto exitlbl;
                    }
                    ptr += sh;

                    v8::Local<v8::Value> value_value = ubjson_read_internal(
                        isolate, ptr, static_cast<size_t>(ptrEnd - ptr), ubj_type, max_depth - 1, &sh, err);
                    //                    std::cout << "ubjson_read_internal UBJ_OBJECT value=";
                    //                    _v8_internal_Print_Object(*((v8::internal::Object **)(*value_value)));
                    //                    std::cout << std::endl;
                    obj_value->Set(string_value, value_value);
                    //                    std::cout << "ubjson_read_internal UBJ_OBJECT object=";
                    //                    _v8_internal_Print_Object(*((v8::internal::Object **)(*obj_value)));
                    //                    std::cout << std::endl;
                    if (*err)
                    {
                        goto exitlbl;
                    }
                    ptr += sh;

                    if (ptr + 1 > ptrEnd)
                    {
                        *err = MC_ERR_ERROR_IN_SCRIPT;
                        goto exitlbl;
                    }
                }
                ptr++;
            }
            result = obj_value;
            break;
        }
    }

exitlbl:

    if (*err)
    {
        return handleScope.Escape(v8::Null(isolate));
    }

    if (offset)
    {
        *offset = static_cast<unsigned>(ptr - ptrStart);
    }

    return handleScope.Escape(result);
}

<<<<<<< HEAD
static int ubjson_int64_write(int64_t int64_value, int known_type, Blob *blob)
=======
static int ubjson_int64_write(int64_t int64_value, int known_type, BlobPtr blob)
>>>>>>> local/2.0-dev
{
    int type = known_type;
    size_t n, sh;
    uint64_t v;
    unsigned char buf[9];
    unsigned char *ptr;

    sh = 1;
    if (type == UBJ_UNDEFINED)
    {
        type = ubjson_best_int_type(int64_value, nullptr);
        sh = 0;
    }

    buf[0] = static_cast<unsigned char>(UBJ_TYPE[type]);
    n = static_cast<size_t>(UBJ_SIZE[type]);

    v = *reinterpret_cast<uint64_t *>(&int64_value);
    ptr = buf + n;
    while (ptr > buf)
    {
        *ptr = static_cast<unsigned char>(v % 256);
        v = v >> 8;
        ptr--;
    }

    blob->Append(buf + sh, n + 1 - sh);

    return MC_ERR_NOERROR;
}

static int ubjson_write_internal(v8::Isolate *isolate, v8::Local<v8::Value> value, int known_type, BlobPtr blob,
                                 int max_depth)
{
    v8::Isolate::Scope isolateScope(isolate);
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope contextScope(context);

    int err = MC_ERR_NOERROR;

    if (max_depth == 0)
    {
        return MC_ERR_NOT_SUPPORTED;
    }

    int ubj_type = known_type;
    if (ubj_type == UBJ_UNDEFINED)
    {
        ubj_type = ubjson_best_type(isolate, value, 0, nullptr, nullptr, nullptr);
        char type = UBJ_TYPE[ubj_type];
        blob->Append(&type, 1);
    }
    if (UBJ_ISINT[ubj_type])
    {
        ubjson_int64_write(value->Int32Value(context).FromJust(), ubj_type, blob);
    }
    else
    {
        switch (ubj_type)
        {
        case UBJ_NULLTYPE:
            // fall through
        case UBJ_BOOL_FALSE:
            // fall through
        case UBJ_BOOL_TRUE:
            break;

        case UBJ_FLOAT64:
        {
            int64_t int64_value;
            double double_value = value->NumberValue(context).FromJust();
            swap_bytes(&int64_value, &double_value, sizeof(double));
            blob->Append(&int64_value, sizeof(double));
            break;
        }
        case UBJ_CHAR:
            // fall through
        case UBJ_STRING:
        {
            std::string string_value = V82String(isolate, value);
            ubjson_int64_write(static_cast<int64_t>(string_value.size()), UBJ_UNDEFINED, blob);
            char *c_string = const_cast<char *>(string_value.c_str());
            blob->Append(c_string, static_cast<unsigned>(string_value.size()));
            break;
        }
        case UBJ_ARRAY:
        {
            if (max_depth <= 1)
            {
                err = MC_ERR_NOT_SUPPORTED;
                goto exitlbl;
            }
            int optimized = 1;
            int last_type = UBJ_UNDEFINED;
            int not_uint8 = 0;
            int64_t usize = 0;
            int64_t ssize = 0;
            unsigned i = 0;

            auto array_value = v8::Local<v8::Array>::Cast(value);
            while (i < array_value->Length())
            {
                ubj_type = ubjson_best_type(isolate, array_value->Get(context, i).ToLocalChecked(), last_type,
                                            &not_uint8, &usize, &ssize);
                if (ubj_type > 0)
                {
                    last_type = ubj_type;
                    i++;
                }
                else
                {
                    optimized = 0;
                    i = array_value->Length();
                }
            }
            if (last_type == UBJ_UNDEFINED)
            {
                optimized = 0;
            }
            if (optimized)
            {
                if (last_type == UBJ_UINT8)
                {
                    if (not_uint8)
                    {
                        last_type = UBJ_INT16;
                    }
                    else
                    {
                        ssize = usize;
                    }
                }
                v8::Local<v8::Integer> length_value =
                    v8::Integer::New(isolate, static_cast<int32_t>(array_value->Length()));
                ubj_type = ubjson_best_type(isolate, length_value, 0, nullptr, nullptr, nullptr);
                int64_t length = static_cast<int64_t>(array_value->Length());
                if (ssize + length + 1 <= length * UBJ_SIZE[last_type] + UBJ_SIZE[ubj_type] + 4)
                {
                    optimized = 0;
                }
            }
            i = 0;
            if (optimized)
            {
                char type = '$';
                blob->Append(&type, 1);
                type = UBJ_TYPE[last_type];
                blob->Append(&type, 1);
                type = '#';
                blob->Append(&type, 1);
                ubjson_int64_write(static_cast<int64_t>(array_value->Length()), UBJ_UNDEFINED, blob);
            }
            else
            {
                last_type = UBJ_UNDEFINED;
            }
            while (i < array_value->Length())
            {
                if (optimized && UBJ_ISINT[last_type])
                {
                    ubjson_int64_write(array_value->Get(context, i).ToLocalChecked()->IntegerValue(context).FromJust(),
                                       last_type, blob);
                }
                else
                {
                    err = ubjson_write_internal(isolate, array_value->Get(context, i).ToLocalChecked(), last_type, blob,
                                                max_depth - 1);
                    if (err)
                    {
                        goto exitlbl;
                    }
                }
                i++;
            }
            if (!optimized)
            {
                char type = ']';
                blob->Append(&type, 1);
            }
            break;
        }
        case UBJ_OBJECT:
        {
            if (max_depth <= 1)
            {
                err = MC_ERR_NOT_SUPPORTED;
                goto exitlbl;
            }
            int optimized = 1;
            int last_type = UBJ_UNDEFINED;
            int not_uint8 = 0;
            int64_t usize = 0;
            int64_t ssize = 0;
            unsigned i = 0;

            auto obj_value = v8::Local<v8::Object>::Cast(value);
            v8::Local<v8::Array> prop_names = obj_value->GetOwnPropertyNames(context).ToLocalChecked();
            while (i < prop_names->Length())
            {
                v8::Local<v8::String> name_ = prop_names->Get(context, i).ToLocalChecked()->ToString();
                v8::Local<v8::Value> value_ = obj_value->Get(context, name_).ToLocalChecked();
                ubj_type = ubjson_best_type(isolate, value_, last_type, &not_uint8, &usize, &ssize);
                if (ubj_type > 0)
                {
                    last_type = ubj_type;
                    i++;
                }
                else
                {
                    optimized = 0;
                    i = prop_names->Length();
                }
            }
            if (last_type == UBJ_UNDEFINED)
            {
                optimized = 0;
            }
            if (optimized)
            {
                if (last_type == UBJ_UINT8)
                {
                    if (not_uint8)
                    {
                        last_type = UBJ_INT16;
                    }
                    else
                    {
                        ssize = usize;
                    }
                }
                v8::Local<v8::Integer> length_value =
                    v8::Integer::New(isolate, static_cast<int32_t>(prop_names->Length()));
                ubj_type = ubjson_best_type(isolate, length_value, 0, nullptr, nullptr, nullptr);
                int64_t length = static_cast<int64_t>(prop_names->Length());
                if (ssize + length + 1 <= length * UBJ_SIZE[last_type] + UBJ_SIZE[ubj_type] + 4)
                {
                    optimized = 0;
                }
            }
            i = 0;
            if (optimized)
            {
                char type = '$';
                blob->Append(&type, 1);
                type = UBJ_TYPE[last_type];
                blob->Append(&type, 1);
                type = '#';
                blob->Append(&type, 1);
                ubjson_int64_write(static_cast<int64_t>(prop_names->Length()), UBJ_UNDEFINED, blob);
            }
            else
            {
                last_type = UBJ_UNDEFINED;
            }
            while (i < prop_names->Length())
            {
                v8::Local<v8::String> name_ = prop_names->Get(context, i).ToLocalChecked()->ToString();
                v8::Local<v8::Value> value_ = obj_value->Get(context, name_).ToLocalChecked();
                err = ubjson_write_internal(isolate, name_, UBJ_STRING, blob, max_depth - 1);
                if (err)
                {
                    goto exitlbl;
                }
                if (optimized && UBJ_ISINT[last_type])
                {
                    ubjson_int64_write(value_->IntegerValue(context).FromJust(), last_type, blob);
                }
                else
                {
                    err = ubjson_write_internal(isolate, value_, last_type, blob, max_depth - 1);
                    if (err)
                    {
                        goto exitlbl;
                    }
                }
                i++;
            }
            if (!optimized)
            {
                char type = '}';
                blob->Append(&type, 1);
            }
            break;
        }
        default:
            return MC_ERR_NOT_SUPPORTED;
        }
    }

exitlbl:

    return err;
}

int V82Ubj(v8::Isolate *isolate, v8::Local<v8::Value> value, BlobPtr blob)
{
    blob->Reset();
    return ubjson_write_internal(isolate, value, UBJ_UNDEFINED, blob, MAX_FORMATTED_DATA_DEPTH);
}

v8::Local<v8::Value> Ubj2V8(v8::Isolate *isolate, BlobPtr blob, int *err)
{
    size_t offset = 0;
    return ubjson_read_internal(isolate, blob->Data(), blob->DataSize(), UBJ_UNDEFINED, MAX_FORMATTED_DATA_DEPTH,
                                &offset, err);
}

} // namespace mc_v8
