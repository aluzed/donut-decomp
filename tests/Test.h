// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

// A test harness of a few lines: TEST(name) { ... } registers a test, CHECK
// fails it (and goes on), SKIP ends it as not run. main runs them all, or
// those whose name holds argv[1], and fails if any failed.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace Test
{

struct Case
{
	const char* name;
	std::function<void()> run;
};

inline std::vector<Case>& Cases()
{
	static std::vector<Case> cases;
	return cases;
}

struct Register
{
	Register(const char* name, std::function<void()> run) { Cases().push_back({name, std::move(run)}); }
};

struct Skipped
{
	std::string why;
};

inline int& Failures()
{
	static int failures = 0;
	return failures;
}

inline void Fail(const char* file, int line, const std::string& what)
{
	++Failures();
	std::fprintf(stderr, "%s:%d: FAILED: %s\n", file, line, what.c_str());
}

} // namespace Test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                                                   \
	static void TEST_CONCAT(test_, name)();                                                          \
	static Test::Register TEST_CONCAT(register_, name)(#name, &TEST_CONCAT(test_, name));           \
	static void TEST_CONCAT(test_, name)()
#define CHECK(condition)                                    \
	do                                                      \
	{                                                       \
		if (!(condition))                                   \
			Test::Fail(__FILE__, __LINE__, #condition);     \
	} while (false)
#define REQUIRE(condition)                                  \
	do                                                      \
	{                                                       \
		if (!(condition))                                   \
		{                                                   \
			Test::Fail(__FILE__, __LINE__, #condition);     \
			return;                                         \
		}                                                   \
	} while (false)
#define SKIP(why) throw Test::Skipped {why}
