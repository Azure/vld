#include "CppUnitTest.h"

#include <vld.h>
#include <SDL.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace vldsdl
{
	TEST_CLASS(vldsdl)
	{
		void CreateAndDestroySDLWindow()
		{
			if (SDL_Init(SDL_INIT_VIDEO) != 0)
				std::abort();

			SDL_Window* pWindow = SDL_CreateWindow(
				"Programming 4 assignment",
				SDL_WINDOWPOS_CENTERED,
				SDL_WINDOWPOS_CENTERED,
				640,
				480,
				SDL_WINDOW_OPENGL
			);

			Assert::IsNotNull(pWindow, L"SDL_CreateWindow failed");

			SDL_DestroyWindow(pWindow);
			pWindow = nullptr;
			SDL_Quit();
		}

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			VLDMarkAllLeaksAsReported();
		}

		TEST_METHOD(TestSDLWindow)
		{
			int leaks = static_cast<int>(VLDGetLeaksCount());
			Assert::AreEqual(0, leaks);

			CreateAndDestroySDLWindow();

			leaks = static_cast<int>(VLDGetLeaksCount());
			Assert::AreEqual(0, leaks);
		}
	};
}
