// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Game.h"
#include "Core/Log.h"

#include <SDL.h>

#include <cstdlib>
#include <memory>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#endif

static Donut::Log::Level ParseLogLevel(std::string_view name)
{
	if (name == "debug" || name == "0") return Donut::Log::Level::Debug;
	if (name == "info" || name == "1") return Donut::Log::Level::Info;
	if (name == "warn" || name == "2") return Donut::Log::Level::Warn;
	if (name == "error" || name == "3") return Donut::Log::Level::Error;
	return Donut::Log::Level::Debug;
}

int main(int argc, char** argv)
{
	// Log level: env DONUT_LOG_LEVEL (debug/info/warn/error) or --log-level <lvl>.
	Donut::Log::Level level = Donut::Log::Level::Debug;
	if (const char* env = std::getenv("DONUT_LOG_LEVEL"))
		level = ParseLogLevel(env);
	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg = argv[i];
		if ((arg == "--log-level" || arg == "-l") && i + 1 < argc)
			level = ParseLogLevel(argv[++i]);
	}
	Donut::Log::SetLevel(level);

	Donut::Log::Info("+---------------------------------------------------+\n"
	          "|    _____        /--------------------------\\      |\n"
	          "|   /     \\      |                            |     |\n"
	          "| \\/\\/     |    /     donut game               \\    |\n"
	          "|  |  (o)(o)    |                              |    |\n"
	          "|  C   .---_)   \\_   _________________________/     |\n"
	          "|   | |.___|      | /                               |\n"
	          "|   |  \\__/      <_/                                |\n"
	          "|   /_____\\                                         |\n"
	          "|  /_____/ \\                                        |\n"
	          "| /         \\                                       |\n"
	          "+---------------------------------------------------+");

#ifdef NDEBUG
	try
	{
#endif
		const auto game = std::make_unique<Donut::Game>(argc, argv);
		game->Run();
#ifdef NDEBUG
	}
	catch (std::runtime_error& e)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);
		return EXIT_FAILURE;
	}
#endif

	return EXIT_SUCCESS;
}

#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return main(__argc, __argv);
}
#endif
