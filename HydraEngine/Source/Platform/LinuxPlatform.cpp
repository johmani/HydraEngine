module;

#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>

module HE;


//////////////////////////////////////////////////////////////////////////
// OS
//////////////////////////////////////////////////////////////////////////
#pragma region OS

using NativeHandleType = void*;
using NativeSymbolType = void*;

void* HE::Modules::SharedLib::Open(const char* path) noexcept
{
    HE_PROFILE_FUNCTION();

    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* HE::Modules::SharedLib::GetSymbolAddress(void* handle, const char* name) noexcept
{
    return dlsym((NativeHandleType)handle, name);
}

void HE::Modules::SharedLib::Close(void* handle) noexcept
{
    HE_PROFILE_FUNCTION();

    dlclose((NativeHandleType)lib);
}

std::string HE::Modules::SharedLib::GetError() noexcept
{
    HE_PROFILE_FUNCTION();

    auto description = dlerror();
    return (description == nullptr) ? "Unknown error (dlerror failed)" : description;
}


bool HE::FileSystem::Open(const std::filesystem::path& path)
{
    HE_PROFILE_FUNCTION();

    const char* args[] { "xdg-open", path.c_str(), NULL };
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (!pid)
    {
        execvp(args[0], const_cast<char **>(args));
        exit(-1);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status) == 0;
    }
}


void HE::OS::SetEnvVar(const char* var, const char* value)
{
    NOT_YET_IMPLEMENTED();
}

void HE::OS::RemoveEnvVar(const char* var)
{
    NOT_YET_IMPLEMENTED();
}

#pragma endregion
