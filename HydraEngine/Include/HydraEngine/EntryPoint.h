#pragma once

int HE::Main(int argc, char** argv)
{
#ifdef HE_ENABLE_LOGGING
	Log::Init("HydraEngine");
#endif

	while (Application::IsApplicationRunning())
	{
		auto app = HE::CreateApplication({ argv, argc });
		if (app)
		{
			app->Run();
			delete app;
			app = nullptr;
		}
		else break;
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
	return HE::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return HE::Main(argc, argv);
}

#endif