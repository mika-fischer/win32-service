#include "utils.hpp"
#include <sstream>

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

std::wstring get_name(const Napi::Env& env, const Napi::Value& val) {
    if (val.IsString())
        return converter.from_bytes(val.As<Napi::String>());
    else
        throw Napi::TypeError::New(env, "String argument required");
}

std::string error_message(const char* prefix)
{
    auto ec = ::GetLastError();
    auto message = std::system_category().message(ec);
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
        message.pop_back();
    std::ostringstream oss;
    oss << prefix << ": " << message << " (" << ec << ")";
    return oss.str();
}

SC_HANDLE_ptr get_manager(const Napi::Env& env, DWORD access) {
    auto manager = SC_HANDLE_ptr(OpenSCManagerW(nullptr, nullptr, access));
    if (manager)
        return manager;
    else
        throw Napi::Error::New(env, error_message("OpenSCManager"));
}

SC_HANDLE_ptr get_service(const Napi::Env& env, SC_HANDLE manager, const wchar_t* name, DWORD access) {
    auto service = SC_HANDLE_ptr(OpenServiceW(manager, name, access));
    if (service)
        return service;
    else
        throw Napi::Error::New(env, error_message("OpenService"));
}

LPQUERY_SERVICE_CONFIGW get_config(const Napi::Env& env, SC_HANDLE service, std::vector<char>& buffer) {
    DWORD size = 0;
    while (!QueryServiceConfigW(service, buffer.empty() ? nullptr : (LPQUERY_SERVICE_CONFIGW)buffer.data(), buffer.size(), &size)) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            buffer.resize(size);
        else
            throw Napi::Error::New(env, error_message("QueryServiceConfig"));
    }
    return (LPQUERY_SERVICE_CONFIGW)buffer.data();
}

SERVICE_STATUS_PROCESS get_status(const Napi::Env& env, SC_HANDLE service) {
    SERVICE_STATUS_PROCESS status;
    DWORD size = 0;
    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &size))
        return status;
    else
        throw Napi::Error::New(env, error_message("QueryServiceStatusEx"));
}

Napi::Object status_to_object(const Napi::Env& env, const SERVICE_STATUS_PROCESS& status) {
    auto result = Napi::Object::New(env);
    result["serviceType"]      = extract_flags(env, status.dwServiceType, service_type_values);
    result["state"]            = std::string(service_state(status.dwCurrentState));
    result["controlsAccepted"] = extract_flags(env, status.dwControlsAccepted, service_controls_accepted_values);
    result["exitCode"]         = static_cast<double>(status.dwWin32ExitCode);
    if (status.dwWin32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR)
        result["serviceSpecificExitCode"] = static_cast<double>(status.dwServiceSpecificExitCode);
    result["checkPoint"]       = static_cast<double>(status.dwCheckPoint);
    result["waitHint"]         = static_cast<double>(status.dwWaitHint);
    result["processId"]        = static_cast<double>(status.dwProcessId);
    result["serviceFlags"]     = extract_flags(env, status.dwServiceFlags, service_flags_values);
    return result;

}

const std::vector<std::pair<DWORD, const char*>> service_type_values {
    { SERVICE_KERNEL_DRIVER,       "KERNEL_DRIVER"       },
    { SERVICE_FILE_SYSTEM_DRIVER,  "FILE_SYSTEM_DRIVER"  },
    { SERVICE_ADAPTER,             "ADAPTER"             },
    { SERVICE_RECOGNIZER_DRIVER,   "RECOGNIZER_DRIVER"   },
    { SERVICE_WIN32_OWN_PROCESS,   "WIN32_OWN_PROCESS"   },
    { SERVICE_WIN32_SHARE_PROCESS, "WIN32_SHARE_PROCESS" },
    { SERVICE_INTERACTIVE_PROCESS, "INTERACTIVE_PROCESS" },
};

const std::vector<std::pair<DWORD, const char*>> service_controls_accepted_values {
    { SERVICE_ACCEPT_STOP,                  "STOP"                  },
    { SERVICE_ACCEPT_PAUSE_CONTINUE,        "PAUSE_CONTINUE"        },
    { SERVICE_ACCEPT_SHUTDOWN,              "SHUTDOWN"              },
    { SERVICE_ACCEPT_PARAMCHANGE,           "PARAMCHANGE"           },
    { SERVICE_ACCEPT_NETBINDCHANGE,         "NETBINDCHANGE"         },
    { SERVICE_ACCEPT_HARDWAREPROFILECHANGE, "HARDWAREPROFILECHANGE" },
    { SERVICE_ACCEPT_POWEREVENT,            "POWEREVENT"            },
    { SERVICE_ACCEPT_SESSIONCHANGE,         "SESSIONCHANGE"         },
    { SERVICE_ACCEPT_PRESHUTDOWN,           "PRESHUTDOWN"           },
    { SERVICE_ACCEPT_TIMECHANGE,            "TIMECHANGE"            },
    { SERVICE_ACCEPT_TRIGGEREVENT,          "TRIGGEREVENT"          },
};

const std::vector<std::pair<DWORD, const char*>> service_flags_values {
    { SERVICE_RUNS_IN_SYSTEM_PROCESS, "RUNS_IN_SYSTEM_PROCESS" },
};

Napi::Array extract_flags(const Napi::Env& env, DWORD flags,
                          const std::vector<std::pair<DWORD, const char*>>& names)
{
    auto result = Napi::Array::New(env);
    for (const auto& kv : names)
        if (flags & kv.first)
            result[result.Length()] = kv.second;
    return result;
}

const char* start_type(DWORD type) {
    switch (type) {
        case SERVICE_BOOT_START:   return "BOOT_START";
        case SERVICE_SYSTEM_START: return "SYSTEM_START";
        case SERVICE_AUTO_START:   return "AUTO_START";
        case SERVICE_DEMAND_START: return "DEMAND_START";
        case SERVICE_DISABLED:     return "DISABLED";
        default:                   return "UNKNOWN";
    }
}

const char* error_control(DWORD control) {
    switch (control) {
        case SERVICE_ERROR_IGNORE:   return "IGNORE";
        case SERVICE_ERROR_NORMAL:   return "NORMAL";
        case SERVICE_ERROR_SEVERE:   return "SEVERE";
        case SERVICE_ERROR_CRITICAL: return "CRITICAL";
        default:                     return "UNKNOWN";
    }
}

const char* service_state(DWORD state)
{
    switch (state) {
        case SERVICE_CONTINUE_PENDING: return "CONTINUE_PENDING";
        case SERVICE_PAUSE_PENDING:    return "PAUSE_PENDING";
        case SERVICE_PAUSED:           return "PAUSED";
        case SERVICE_RUNNING:          return "RUNNING";
        case SERVICE_START_PENDING:    return "START_PENDING";
        case SERVICE_STOP_PENDING:     return "STOP_PENDING";
        case SERVICE_STOPPED:          return "STOPPED";
        default:                       return "UNKNOWN";
    }
}
