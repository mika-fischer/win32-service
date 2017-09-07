#include <napi.h>

Napi::Object sc_enumerate(Napi::CallbackInfo& info);
Napi::Object sc_names(Napi::CallbackInfo& info);
Napi::Object sc_config(Napi::CallbackInfo& info);
Napi::Object sc_status(Napi::CallbackInfo& info);

void sc_start(Napi::CallbackInfo& info);
void sc_stop(Napi::CallbackInfo& info);

void sc_create(Napi::CallbackInfo& info);
void sc_change(Napi::CallbackInfo& info);
void sc_remove(Napi::CallbackInfo& info);