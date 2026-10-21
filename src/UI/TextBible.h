// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
	// The compiled bible beside it (srr2.p3d), for the terms the export lacks:
	// the PC's own (GAME_SETTINGS, MOUSE_SENSITIVITY_X ...). Its terms are
	// hashed: h = (h * 64 + c) modulo its table's own number.
	bool LoadCompiled(const std::string& path, char language);

	// The phrase for `term`, or empty when there is none. A phrase the data
	// leaves untranslated ("???", the whole Italian column and some of the
	// PC's own terms in French) counts as none, and the fallback's is given.
	const std::string& Get(const std::string& term) const;
	// Asked for what this one has not (the English bible).
	void SetFallback(const TextBible* fallback) { _fallback = fallback; }

private:
	std::unordered_map<std::string, std::string> _phrases;
	// Each compiled bible loaded, in turn: scrooby2's has fewer of the PC's
	// terms translated than scrooby's.
	struct Compiled
	{
		uint32_t modulo = 0;
		std::unordered_map<uint32_t, std::string> phrases;
	};
	std::vector<Compiled> _compiled;
	const TextBible* _fallback = nullptr;
};

} // namespace Donut
