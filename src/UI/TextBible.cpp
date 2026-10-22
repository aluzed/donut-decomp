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
	{
		// Quoted as a spreadsheet exports it: "FOLG DER POLIZEI ZU ""LARD
		// LAD'S""." is FOLG DER POLIZEI ZU "LARD LAD'S".
		if (field.size() >= 2 && field.front() == '"' && field.back() == '"')
		{
			std::string text;
			for (std::size_t i = 1; i + 1 < field.size(); ++i)
			{
				text += field[i];
				if (field[i] == '"' && field[i + 1] == '"' && i + 2 < field.size())
					++i;
			}
			field = text;
		}
		fields.push_back(field);
	}
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
		Compiled& table = _compiled.emplace_back();
		table.modulo = modulo;
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
			table.phrases[u32(hashes + 4 * i)] = std::move(phrase);
		}
		Log::Info("TextBible: {} compiled phrases in language '{}' from {}", table.phrases.size(), language, path);
		return true;
	}
	return false;
}

const std::string& TextBible::Get(const std::string& term) const
{
	static const std::string kNone;
	const auto usable = [](const std::string& phrase) { return !phrase.empty() && phrase != "???"; };
	if (const auto found = _phrases.find(term); found != _phrases.end() && usable(found->second))
		return found->second;
	for (const Compiled& table : _compiled)
	{
		if (table.modulo == 0)
			continue;
		uint64_t hash = 0;
		for (const unsigned char c : term)
			hash = (hash * 64 + c) % table.modulo;
		if (const auto found = table.phrases.find(static_cast<uint32_t>(hash));
		    found != table.phrases.end() && usable(found->second))
			return found->second;
	}
	return _fallback ? _fallback->Get(term) : kNone;
}

} // namespace Donut
