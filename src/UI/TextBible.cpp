// Copyright 2019-2020 the donut authors. See AUTHORS.md

#include "UI/TextBible.h"
#include "Core/Log.h"

#include <fstream>
#include <iterator>
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

bool TextBible::LoadCompiled(const std::string& path, char language)
{
	std::ifstream file(path, std::ios::binary);
	const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const auto u32 = [&data](std::size_t at) {
		return at + 4 <= data.size() ? static_cast<uint32_t>(data[at] | data[at + 1] << 8 | data[at + 2] << 16 | data[at + 3] << 24) : 0u;
	};
	// The file's header, the bible's chunk (0x1800D: its name), then a chunk
	// a language (0x1800E): its name, its letter, the count of its phrases,
	// the modulo, the size of its text; the hashes, sorted, their phrases'
	// offsets into the text, the text, UTF-16 phrases each ended by a zero.
	if (data.size() < 24 || u32(12) != 0x1800D)
		return false;
	for (std::size_t chunk = 12 + u32(16); chunk + 12 <= data.size() && u32(chunk + 8) > 0; chunk += u32(chunk + 8))
	{
		std::size_t at = chunk + 12;
		if (u32(chunk) != 0x1800E || at >= data.size())
			continue;
		at += 1 + data[at];
		if (at >= data.size() || static_cast<char>(data[at]) != language)
			continue;
		const uint32_t count = u32(at + 1), modulo = u32(at + 5), size = u32(at + 9);
		const std::size_t hashes = at + 13, offsets = hashes + 4 * count, text = offsets + 4 * count;
		if (text + size > data.size())
			return false;
		for (uint32_t i = 0; i < count; ++i)
		{
			// Latin-1, as the export's.
			std::string phrase;
			for (std::size_t c = text + u32(offsets + 4 * i); c + 1 < text + size && (data[c] | data[c + 1]); c += 2)
			{
				const unsigned unit = data[c] | data[c + 1] << 8;
				phrase += static_cast<char>(unit < 256 ? unit : '?');
			}
			_compiled[u32(hashes + 4 * i)] = std::move(phrase);
		}
		_modulo = modulo;
		Log::Info("TextBible: {} compiled phrases in language '{}' from {}", _compiled.size(), language, path);
		return true;
	}
	return false;
}

const std::string& TextBible::Get(const std::string& term) const
{
	static const std::string kNone;
	const auto found = _phrases.find(term);
	if (found != _phrases.end())
		return found->second;
	if (_modulo == 0)
		return kNone;
	uint64_t hash = 0;
	for (const unsigned char c : term)
		hash = (hash * 64 + c) % _modulo;
	const auto compiled = _compiled.find(static_cast<uint32_t>(hash));
	return compiled == _compiled.end() ? kNone : compiled->second;
}

} // namespace Donut
