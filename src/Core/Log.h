// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <fmt/format.h>
#include <fmt/printf.h>

#include <string>

namespace Donut
{
namespace Log
{

enum class Level
{
	Debug = 0,
	Info = 1,
	Warn = 2,
	Error = 3,
};

// Minimum level emitted. Messages below this severity are dropped.
// Defaults to Debug (everything is printed) to preserve historical behaviour.
void SetLevel(Level level);
Level GetLevel();

template <typename... Args>
inline void Debug(std::string_view fmt, const Args&... args)
{
	if (GetLevel() > Level::Debug) return;
	fmt::print(fmt, args...);
	fmt::print("\n");
}

template <typename... Args>
inline void Info(std::string_view fmt, const Args&... args)
{
	if (GetLevel() > Level::Info) return;
	fmt::print(fmt, args...);
	fmt::print("\n");
}

template <typename... Args>
inline void Warn(std::string_view fmt, const Args&... args)
{
	if (GetLevel() > Level::Warn) return;
	fmt::print(stderr, "[WARN] ");
	fmt::print(stderr, fmt, args...);
	fmt::print(stderr, "\n");
}

template <typename... Args>
inline void Error(std::string_view fmt, const Args&... args)
{
	if (GetLevel() > Level::Error) return;
	fmt::print(stderr, "[ERROR] ");
	fmt::print(stderr, fmt, args...);
	fmt::print(stderr, "\n");
}

}
} // namespace Donut
