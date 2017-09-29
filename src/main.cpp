#include "service.hpp"
#include "service-control.hpp"
#include <iostream>

Napi::Object init(Napi::Env env, Napi::Object exports) {
    // service-control
    exports["names"]     = Napi::Function::New(env, sc_names);

    exports["enumerate"] = Napi::Function::New(env, sc_enumerate);
    exports["config"]    = Napi::Function::New(env, sc_config);
    exports["status"]    = Napi::Function::New(env, sc_status);

    exports["start"]     = Napi::Function::New(env, sc_start);
    exports["stop"]      = Napi::Function::New(env, sc_stop);

    exports["create" ]   = Napi::Function::New(env, sc_create);
    exports["change"]    = Napi::Function::New(env, sc_change);
    exports["remove"]    = Napi::Function::New(env, sc_remove);

    // service
    exports["run"]       = Napi::Function::New(env, run);

    return exports;
}

NODE_API_MODULE(service, init);