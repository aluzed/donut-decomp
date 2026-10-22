// Copyright 2019-2020 the donut authors. See AUTHORS.md

// The text bible's spreadsheet export: its columns by the "Languages" line, its
// quoted fields unquoted.

#include "Test.h"

#include "UI/TextBible.h"

#include <filesystem>
#include <fstream>

using namespace Donut;

TEST(TextBibleQuotedFields)
{
	const auto path = std::filesystem::temp_directory_path() / "donut_test_bible.txt";
	{
		std::ofstream file(path);
		file << "Languages\tEFG\t\t\t\t\n";
		file << "Screen\tPHRASE TABLE\tSPACE\tE\tF\tG\n";
		file << "INGAME\tPLAIN\t\tPlain\tSimple\tEinfach\n";
		file << "INGAME\tQUOTED\t\t\"FOLLOW THE POLICE TO \"\"LARD LAD'S\"\".\"\tx\t"
		        "\"FOLG DER POLIZEI ZU \"\"LARD LAD'S\"\".\"\n";
		file << "INGAME\tTITLE\t\t\"Hello!\"\tx\t\"SUCH CHIEF WIGGUM!\"\n";
	}
	TextBible english, german;
	CHECK(english.Load(path.string(), 'E'));
	CHECK(german.Load(path.string(), 'G'));
	std::filesystem::remove(path);
	CHECK(english.Get("PLAIN") == "Plain");
	CHECK(german.Get("PLAIN") == "Einfach");
	CHECK(english.Get("QUOTED") == "FOLLOW THE POLICE TO \"LARD LAD'S\".");
	CHECK(german.Get("QUOTED") == "FOLG DER POLIZEI ZU \"LARD LAD'S\".");
	CHECK(german.Get("TITLE") == "SUCH CHIEF WIGGUM!");
}
