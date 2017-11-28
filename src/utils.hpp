#include <napi.h>
#include <windows.h>
#include <codecvt>
#include <memory>
#include <string>

struct SC_HANDLE_closer {
    void operator()(SC_HANDLE handle) {
        CloseServiceHandle(handle);
    }
};
using SC_HANDLE_pointee = std::remove_reference<decltype(*SC_HANDLE{})>::type; 
using SC_HANDLE_ptr     = std::unique_ptr<SC_HANDLE_pointee, SC_HANDLE_closer>;

extern std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

std::wstring get_name(const Napi::Env& env, const Napi::Value& val);
std::wstring array_to_double_null_string(const Napi::Env& env, const Napi::Array& array);
std::string error_message(const char* prefix);
SC_HANDLE_ptr get_manager(const Napi::Env& env, DWORD access);
SC_HANDLE_ptr get_service(const Napi::Env& env, SC_HANDLE manager, const wchar_t* name, DWORD access);
LPQUERY_SERVICE_CONFIGW get_config(const Napi::Env& env, SC_HANDLE service, std::vector<char>& buffer);
template<typename T>
inline T* get_config2(const Napi::Env& env, SC_HANDLE service, DWORD info_level, std::vector<char>& buffer);
SERVICE_STATUS_PROCESS get_status(const Napi::Env& env, SC_HANDLE service);

Napi::Object status_to_object(const Napi::Env& env, const SERVICE_STATUS_PROCESS& status);

extern const std::vector<std::pair<DWORD, const char*>> service_type_values;
extern const std::vector<std::pair<DWORD, const char*>> service_controls_accepted_values;
extern const std::vector<std::pair<DWORD, const char*>> service_flags_values;
Napi::Array extract_flags(const Napi::Env& env, DWORD flags,
                          const std::vector<std::pair<DWORD, const char*>>& names);

const char* start_type(DWORD type);
const char* error_control(DWORD control);
const char* service_state(DWORD state);

// Implementation

template<typename T>
inline T* get_config2(const Napi::Env& env, SC_HANDLE service, DWORD info_level, std::vector<char>& buffer) {
    DWORD size = 0;
    while (!QueryServiceConfig2W(service, info_level, buffer.empty() ? nullptr : (LPBYTE)buffer.data(), buffer.size(), &size)) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            buffer.resize(size);
        else
            throw Napi::Error::New(env, error_message("QueryServiceConfig2"));
    }
    return (T*)buffer.data();
}
