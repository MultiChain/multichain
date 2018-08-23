// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8ISOLATEMANAGER_H_
#define V8ISOLATEMANAGER_H_

#include <v8.h>
#include <libplatform/libplatform.h>

namespace mc_v8
{

/**
 * Manage v8::Isolate objects.
 *
 * Currently support a single Isolate. In the future might support one for each calling thread.
 * Implemented as a Singleton.
 */
class V8IsolateManager
{
public:
    /**
     * Get the (single) instance of this class.
     */
    static V8IsolateManager* Instance();

    /**
     * Get a usable Isolate for use in the calling thread.
     */
    v8::Isolate* GetIsolate();

private:
    V8IsolateManager();
    ~V8IsolateManager();

    static V8IsolateManager* m_instance;
    std::unique_ptr<v8::Platform> m_platform;
    v8::Isolate::CreateParams m_createParams;
    v8::Isolate* m_isolate = nullptr;
};

} /* namespace mc_v8 */

#endif /* V8ISOLATEMANAGER_H_ */
