
#include "napi-thread-safe-callback.hpp"
#include "service.hpp"
#include "utils.hpp"
#include <windows.h>
#include <codecvt>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <iostream>

namespace {
    class ServiceInstance {
        public:
            ServiceInstance(const Napi::CallbackInfo &info)
            : env_(info.Env()),
              name_(get_name(env_, info[0])),
              init_callback_(new ThreadSafeCallback(info[1].As<Napi::Function>())),
              stop_callback_(new ThreadSafeCallback(info[2].As<Napi::Function>()))
            {}

            const std::wstring& name() { return name_; }

            inline void set_status(DWORD state, DWORD exit_code, DWORD wait_hint);
            inline DWORD handler(DWORD control, DWORD event_type, void *event_data);
            inline void service_main(DWORD argc, LPWSTR *argv);

            // NodeJS stuff
            Napi::Env             env_;
            std::unique_ptr<ThreadSafeCallback> init_callback_;
            std::unique_ptr<ThreadSafeCallback> stop_callback_;

            // Service stuff
            std::wstring          name_;
            SERVICE_STATUS        service_status_{0};
            SERVICE_STATUS_HANDLE service_status_handle_{0};
    };

    // Global singleton instance
    std::shared_ptr<ServiceInstance> instance;

    DWORD WINAPI HandlerEx(DWORD control, DWORD event_type, void *event_data, void *context) {
        return static_cast<ServiceInstance*>(context)->handler(control, event_type, event_data);
    }

    void WINAPI ServiceMain(DWORD argc, LPWSTR *argv) {
        if (instance) {
            instance->service_main(argc, argv);
        }
    }

    void nodejs_exit_handler(Napi::CallbackInfo& info) {
        auto env = info.Env();
        auto rc = static_cast<DWORD>(info[0].As<Napi::Number>().Int64Value());
        auto instance_ref = instance;
        if (instance_ref) {
            instance_ref->set_status(SERVICE_STOPPED, rc, 0);
        }
    }
    
    void ServiceInstance::set_status(DWORD state, DWORD exit_code, DWORD wait_hint)
    {
        service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        service_status_.dwCurrentState = state;
        if (state == SERVICE_START_PENDING)
            service_status_.dwControlsAccepted = 0;
        else
            service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        service_status_.dwWin32ExitCode = exit_code;
        service_status_.dwServiceSpecificExitCode = 0;
        service_status_.dwCheckPoint = 0;
        service_status_.dwWaitHint = wait_hint;
        SetServiceStatus(service_status_handle_, &service_status_);
    }

    void ServiceInstance::service_main(DWORD argc, LPWSTR *argv)
    {
        // The ServiceMain function should immediately call the RegisterServiceCtrlHandlerEx
        // function to specify a HandlerEx function to handle control requests.
        service_status_handle_ = RegisterServiceCtrlHandlerExW(name_.c_str(), &HandlerEx, this);
        if (!service_status_handle_)
            return;

        // Next, it should call the SetServiceStatus function to send status
        // information to the service control manager.
        set_status(SERVICE_START_PENDING, NO_ERROR, 0);

        // After these calls, the function should complete the initialization of the service.
        (*init_callback_)([argc, argv](Napi::Env env, std::vector<napi_value>& args) {
            args.push_back(env.Undefined());
            for (DWORD i=0; i<argc; ++i)
                args.push_back(Napi::String::New(env, converter.to_bytes(argv[i])));
        }).get();
        init_callback_.reset();

        // The Service Control Manager (SCM) waits until the service reports a status of
        // SERVICE_RUNNING. It is recommended that the service reports this status as quickly
        // as possible...
        set_status(SERVICE_RUNNING, NO_ERROR, 0);
    }

    DWORD ServiceInstance::handler(DWORD control, DWORD event_type, void *event_data) {
        switch (control)
        {
        case SERVICE_CONTROL_STOP:
            set_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
            stop_callback_->call();
            stop_callback_.reset();
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
    }
}

void run(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    if (instance)
        throw Napi::Error::New(env, "run may only be called once");

    // Create service singleton
    instance = std::make_shared<ServiceInstance>(info);

    // StartServiceCtrlDispatcher blocks (potentially), so it must run in its own thread
    std::thread([] {
        SERVICE_TABLE_ENTRYW dispatch_table[] = {
            {const_cast<wchar_t*>(instance->name().c_str()), ServiceMain},
            {nullptr, nullptr}
        };
        if (!StartServiceCtrlDispatcherW(dispatch_table))
            instance->init_callback_->error(error_message("QueryServiceStatusEx"));
        instance.reset();
    }).detach();

    // Install exit handler
    auto process = env.Global().Get("process").As<Napi::Object>();
    auto process_once = process.Get("once").As<Napi::Function>();
    process_once.Call(process, {
        Napi::String::New(env, "exit"),
        Napi::Function::New(env, nodejs_exit_handler),
    });
}