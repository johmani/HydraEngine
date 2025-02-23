#pragma once

int HydraEngine::Main(int argc, char** argv)
{
#ifdef HE_ENABLE_LOGGING
	Log::Init("HydraEngine");
#endif

	while (Application::IsApplicationRunning())
	{
		auto app = HydraEngine::CreateApplication({ argv, argc });
		app->Run();
		delete app;
		app = nullptr;
	}

#ifdef HE_ENABLE_LOGGING
	Log::Shutdown();
#endif 

	return 0;
}

#if defined(HE_PLATFORM_WINDOWS) && defined(HE_DIST)
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
#else
int main(int __argc, char** __argv)
#endif
{
	return HydraEngine::Main(__argc, __argv);
}
