module;

#include <Windows.h>

module HE;

bool HE::FileSystem::Open(const std::filesystem::path& path)
{
	return (INT_PTR)::ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT) > 32;
}
