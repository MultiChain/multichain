// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_V8FILTER_H_
#define MULTICHAIN_V8FILTER_H_

#include <v8.h>

namespace mc_v8
{

/**
 * Implementation of a transaction filter based on the V8 JS engine.
 */
class V8Filter
{
public:
    V8Filter()
    {
        Zero();
    }

    ~V8Filter()
    {
        Destroy();
    }
    void Zero();
    int Destroy();

    v8::Isolate* GetIsolate()
    {
        return m_isolate;
    }

    /**
     * Initialize the filter to run the function @p functionName in the JS @p script.
     *
     * @param script    The filter JS code.
     * @param main_name The expected name of the filtering function in the script.
     * @param strResult Reason for failure if unsuccessful.
     * @return          MC_ERR_NOERROR if successful, MC_ERR_INTERNAL_ERROR if not.
     */
    int Initialize(std::string script, std::string functionName, std::string& strResult);

    /**
     * Run the filter function in the JS script.
     *
     * @param strResult Reason for script failure or transaction rejection.
     * @return          MC_ERR_NOERROR if the transaction was accepted, MC_ERR_INTERNAL_ERROR if not.
     */
    int Run(std::string& strResult);

private:
    void MaybeCreateIsolate();
    int CompileAndLoadScript(std::string script, std::string functionName, std::string source, std::string& strResult);
    void ReportException(v8::TryCatch* tryCatch, std::string& strResult);

    v8::Isolate* m_isolate;
    v8::Global<v8::Context> m_context;
    v8::Global<v8::Function> m_filterFunction;
};

} // namespace mc_v8

#endif /* MULTICHAIN_V8FILTER_H_ */
