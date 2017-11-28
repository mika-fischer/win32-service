#include "service-control.hpp"
#include "utils.hpp"
#include <sstream>
#include <iostream>

namespace {
    // TODO: This blocks one of Nodes worker threads for a potentially long period of time
    // It would be better to move this into its own thread
    class StatusWaitWorker : public Napi::AsyncWorker {
        public:
            StatusWaitWorker(const std::wstring& name, DWORD initial_state, DWORD target_state, const Napi::Function& callback)
            : Napi::AsyncWorker(callback), name_(name), initial_state_(initial_state), target_state_(target_state)
            {}
        
        protected:
            void Execute() override {
                auto manager = get_manager(Env(), GENERIC_READ);
                auto service = get_service(Env(), manager.get(), name_.c_str(), SERVICE_QUERY_STATUS);
                DWORD start = GetTickCount();
                while (true) {
                    auto status = get_status(Env(), service.get());
                    if (status.dwCurrentState == target_state_) {
                        break;
                    } else if (status.dwCurrentState != initial_state_) {
                        std::wostringstream oss;
                        oss << "State of service " << name_ << " changed to " << service_state(status.dwCurrentState);
                        throw Napi::Error::New(Env(), converter.to_bytes(oss.str()));
                    } else {
                        if (GetTickCount() - start > timeout) {
                            std::wostringstream oss;
                            oss << "State of service " << name_ << " did not change to "
                                << service_state(target_state_) << " after " << (timeout / 1000) << " seconds";
                            throw Napi::Error::New(Env(), converter.to_bytes(oss.str()));
                        }
                        Sleep(sleep_interval);
                    }
                }
            }

        private:
            static const DWORD timeout = 60 * 1000;
            static const DWORD sleep_interval = 100;
            std::wstring name_;
            DWORD initial_state_;
            DWORD target_state_;
    };
}

Napi::Object sc_names(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto type = info[0].As<Napi::Number>().Uint32Value();
    const auto state = info[1].As<Napi::Number>().Uint32Value();

    const auto manager = get_manager(env, SC_MANAGER_ENUMERATE_SERVICE);

    std::vector<char> buffer;
    DWORD size = 0, n_services = 0;
    while (!EnumServicesStatusExW(manager.get(), SC_ENUM_PROCESS_INFO, type, state,
                                  buffer.empty() ? nullptr : (LPBYTE)buffer.data(),
                                  buffer.size(), &size, &n_services, nullptr, nullptr)) {
        if (GetLastError() == ERROR_MORE_DATA)
            buffer.resize(size);
        else
            throw Napi::Error::New(env, error_message("QueryServiceConfig"));
    }
    LPENUM_SERVICE_STATUS_PROCESSW services = (LPENUM_SERVICE_STATUS_PROCESSW) buffer.data();

    auto result = Napi::Array::New(env);
    for (DWORD i=0; i<n_services; ++i)
        result[i] = converter.to_bytes(services[i].lpServiceName);
    return result;
}

Napi::Object sc_enumerate(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto type = info[0].As<Napi::Number>().Uint32Value();
    const auto state = info[1].As<Napi::Number>().Uint32Value();

    const auto manager = get_manager(env, SC_MANAGER_ENUMERATE_SERVICE);

    std::vector<char> buffer;
    DWORD size = 0, n_services = 0;
    while (!EnumServicesStatusExW(manager.get(), SC_ENUM_PROCESS_INFO, type, state,
                                  buffer.empty() ? nullptr : (LPBYTE)buffer.data(),
                                  buffer.size(), &size, &n_services, nullptr, nullptr)) {
        if (GetLastError() == ERROR_MORE_DATA)
            buffer.resize(size);
        else
            throw Napi::Error::New(env, error_message("QueryServiceConfig"));
    }
    LPENUM_SERVICE_STATUS_PROCESSW services = (LPENUM_SERVICE_STATUS_PROCESSW) buffer.data();

    auto result = Napi::Object::New(env);
    for (DWORD i=0; i<n_services; ++i) {
        auto obj = status_to_object(env, services[i].ServiceStatusProcess);
        if (services[i].lpDisplayName)
            obj.Set("displayName", Napi::String::New(env, converter.to_bytes(services[i].lpDisplayName)));
        result.Set(converter.to_bytes(services[i].lpServiceName), obj);
    }

    return result;
}

Napi::Object sc_config(Napi::CallbackInfo& info) {
    std::vector<char> buffer;
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), SERVICE_QUERY_CONFIG);

    auto result = Napi::Object::New(env);

    {
        auto config = get_config(env, service.get(), buffer);
        result["serviceType"] = extract_flags(env, config->dwServiceType, service_type_values);
        result["startType"] = std::string(start_type(config->dwStartType));
        result["errorControl"] = std::string(error_control(config->dwErrorControl));
        if (config->lpBinaryPathName)
            result["binaryPathName"] = converter.to_bytes(config->lpBinaryPathName);
        if (config->lpLoadOrderGroup)
            result["loadOrderGroup"] = converter.to_bytes(config->lpLoadOrderGroup);
        result["tagId"] = static_cast<double>(config->dwTagId);
        auto dependencies = Napi::Array::New(env);
        for (auto s = config->lpDependencies; *s; s += wcslen(s) + 1)
            dependencies[dependencies.Length()] = converter.to_bytes(s);
        result["dependencies"] = dependencies;
        if (config->lpServiceStartName)
            result["serviceStartName"] = converter.to_bytes(config->lpServiceStartName);
        if (config->lpDisplayName)
            result["displayName"] = converter.to_bytes(config->lpDisplayName);
    }

    {
        auto description = get_config2<SERVICE_DESCRIPTIONW>(env, service.get(), SERVICE_CONFIG_DESCRIPTION, buffer);
        if (description->lpDescription)
            result["description"] = converter.to_bytes(description->lpDescription);
    }

    return result;
}

Napi::Object sc_status(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), SERVICE_QUERY_STATUS);
    auto status = get_status(env, service.get());
    return status_to_object(env, status);
}

void sc_start(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    if (info.Length() >= 2 && !info[1].IsFunction())
        throw Napi::TypeError::New(env, "Expected callback as second argument");

    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), SERVICE_START);
    if (!StartServiceW(service.get(), 0, nullptr))
        throw Napi::Error::New(env, error_message("StartService"));

    if (info.Length() >= 2)
        (new StatusWaitWorker(name, SERVICE_START_PENDING, SERVICE_RUNNING, info[1].As<Napi::Function>()))->Queue();
}

void sc_stop(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    if (info.Length() >= 2 && !info[1].IsFunction())
        throw Napi::TypeError::New(env, "Expected callback as second argument");

    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), SERVICE_STOP);
    SERVICE_STATUS status;
    if (!ControlService(service.get(), SERVICE_CONTROL_STOP, &status) && ::GetLastError() != ERROR_SERVICE_NOT_ACTIVE)
        throw Napi::Error::New(env, error_message("ControlService"));
    
    if (info.Length() >= 2)
        (new StatusWaitWorker(name, SERVICE_STOP_PENDING, SERVICE_STOPPED, info[1].As<Napi::Function>()))->Queue();
}

void sc_change(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    const auto config = info[1].As<Napi::Object>();

    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), SERVICE_CHANGE_CONFIG);
    if (!ChangeServiceConfigW(
        service.get(),
        config["serviceType"].IsUndefined() ?
            SERVICE_NO_CHANGE :
            config["serviceType"].As<Napi::Number>().Uint32Value(),
        config["startType"].IsUndefined() ?
            SERVICE_NO_CHANGE :
            config["startType"].As<Napi::Number>().Uint32Value(),
        config["errorControl"].IsUndefined() ?
            SERVICE_NO_CHANGE :
            config["errorControl"].As<Napi::Number>().Uint32Value(),
        config["binaryPathName"].IsUndefined() ?
            nullptr :
            get_name(env, config["binaryPathName"]).c_str(),
        config["loadOrderGroup"].IsUndefined() ?
            nullptr :
            get_name(env, config["loadOrderGroup"]).c_str(),
        nullptr, // tag ID
        config["dependencies"].IsUndefined() ?
            nullptr :
            array_to_double_null_string(env, config["dependencies"].As<Napi::Array>()).c_str(),
        config["serviceStartName"].IsUndefined() ?
            nullptr :
            get_name(env, config["serviceStartName"]).c_str(),
        config["password"].IsUndefined() ?
            nullptr :
            get_name(env, config["password"]).c_str(),
        config["displayName"].IsUndefined() ?
            name.c_str() :
            get_name(env, config["displayName"]).c_str()
    ))
        throw Napi::Error::New(env, error_message("ChangeServiceConfig"));
    if (!config["description"].IsUndefined()) {
        auto description_str = get_name(env, config["description"]);
        SERVICE_DESCRIPTIONW description;
        description.lpDescription = const_cast<wchar_t*>(description_str.c_str());
        if (!ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &description))
            throw Napi::Error::New(env, error_message("ChangeServiceConfig2W"));
    }
}

void sc_create(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    const auto config = info[1].As<Napi::Object>();

    auto manager = get_manager(env, SC_MANAGER_CREATE_SERVICE);
    auto service = SC_HANDLE_ptr(CreateServiceW(
        manager.get(),
        name.c_str(),
        config["displayName"].IsUndefined() ?
            name.c_str() :
            get_name(env, config["displayName"]).c_str(),
        SERVICE_ALL_ACCESS, // desired access 
        config["serviceType"].IsUndefined() ?
            SERVICE_WIN32_OWN_PROCESS :
            config["serviceType"].As<Napi::Number>().Uint32Value(),
        config["startType"].IsUndefined() ?
            SERVICE_AUTO_START :
            config["startType"].As<Napi::Number>().Uint32Value(),
        config["errorControl"].IsUndefined() ?
            SERVICE_ERROR_NORMAL :
            config["errorControl"].As<Napi::Number>().Uint32Value(),
        get_name(env, config["binaryPathName"]).c_str(),
        config["loadOrderGroup"].IsUndefined() ?
            nullptr :
            get_name(env, config["loadOrderGroup"]).c_str(),
        nullptr, // no tag identifier 
        config["dependencies"].IsUndefined() ?
            nullptr :
            array_to_double_null_string(env, config["dependencies"].As<Napi::Array>()).c_str(),
        config["serviceStartName"].IsUndefined() ?
            nullptr :
            get_name(env, config["serviceStartName"]).c_str(),
        config["password"].IsUndefined() ?
            nullptr :
            get_name(env, config["password"]).c_str()
    ));
    if (!service)
        throw Napi::Error::New(env, error_message("CreateService"));
    if (!config["description"].IsUndefined()) {
        auto description_str = get_name(env, config["description"]);
        SERVICE_DESCRIPTIONW description;
        description.lpDescription = const_cast<wchar_t*>(description_str.c_str());
        if (!ChangeServiceConfig2W(service.get(), SERVICE_CONFIG_DESCRIPTION, &description))
            throw Napi::Error::New(env, error_message("ChangeServiceConfig2W"));
    }
}

void sc_remove(Napi::CallbackInfo& info) {
    const auto env = info.Env();
    const auto name = get_name(env, info[0]);
    auto manager = get_manager(env, GENERIC_READ);
    auto service = get_service(env, manager.get(), name.c_str(), DELETE);
    if (!DeleteService(service.get()))
        throw Napi::Error::New(env, error_message("DeleteService"));
}