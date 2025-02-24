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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	return HydraEngine::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return HydraEngine::Main(argc, argv);
}

#endif