// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "Test.h"

#include <cstring>

int main(int argc, char** argv)
{
	int run = 0, skipped = 0;
	for (const auto& test : Test::Cases())
	{
		if (argc > 1 && !std::strstr(test.name, argv[1]))
			continue;
		const int before = Test::Failures();
		try
		{
			test.run();
			++run;
			std::printf("%s %s\n", Test::Failures() == before ? "ok  " : "FAIL", test.name);
		}
		catch (const Test::Skipped& skip)
		{
			++skipped;
			std::printf("skip %s: %s\n", test.name, skip.why.c_str());
		}
	}
	std::printf("%d run, %d skipped, %d failure(s)\n", run, skipped, Test::Failures());
	return Test::Failures() == 0 ? 0 : 1;
}
