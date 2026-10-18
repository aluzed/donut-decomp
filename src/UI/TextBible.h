// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <string>
#include <unordered_map>

namespace Donut
{

// The game's phrase table, from the tab-separated export the CD carries next to
// the compiled bible (art/frontend/scrooby2/resource/txtbible/srr2.txt): one row
// per term, one column per language. Mission objectives are rows like
// MISSION_OBJECTIVE_153 -> "COLLECT LISA'S SCIENCE PROJECT".
class TextBible
{
public:
	// `language` is the export's own code: E, F, G, I or S. False when the file
	// or the language is missing.
	bool Load(const std::string& path, char language);

	// The phrase for `term`, or empty when there is none.
	const std::string& Get(const std::string& term) const;

private:
	std::unordered_map<std::string, std::string> _phrases;
};

} // namespace Donut
