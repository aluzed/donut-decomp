// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "UI/TextBible.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace Donut
{
namespace
{
std::vector<std::string> splitTabs(const std::string& line)
{
	std::vector<std::string> fields;
	std::stringstream stream(line);
	std::string field;
	while (std::getline(stream, field, '\t'))
		fields.push_back(field);
	return fields;
}
} // namespace

bool TextBible::Load(const std::string& path, char language)
{
	std::ifstream file(path);
	if (!file.good())
	{
		Log::Warn("TextBible: '{}' not found", path);
		return false;
	}

	// The layout, from the file itself: a "Languages EFGIS" line naming the
	// columns in order, then rows of screen, term, critical flag and one phrase
	// per language.
	constexpr std::size_t kFirstPhraseColumn = 3;
	std::size_t column = 0;
	std::string line;
	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		const auto fields = splitTabs(line);
		if (fields.size() < 2)
			continue;

		if (fields[0] == "Languages")
		{
			const auto at = fields[1].find(language);
			if (at == std::string::npos)
			{
				Log::Warn("TextBible: '{}' has no language '{}' (it has {})", path, language, fields[1]);
				return false;
			}
			column = kFirstPhraseColumn + at;
			continue;
		}

		if (column == 0 || fields.size() <= column || fields[1].empty() || fields[column].empty())
			continue;
		_phrases[fields[1]] = fields[column];
	}

	Log::Info("TextBible: {} phrases in language '{}' from {}", _phrases.size(), language, path);
	return !_phrases.empty();
}

const std::string& TextBible::Get(const std::string& term) const
{
	static const std::string kNone;
	const auto found = _phrases.find(term);
	return found == _phrases.end() ? kNone : found->second;
}

} // namespace Donut
