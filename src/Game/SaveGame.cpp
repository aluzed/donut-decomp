#include "SaveGame.h"

#include "Core/FileSystem.h"
#include "Core/Log.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace Donut
{

namespace
{
constexpr const char* kFile = "donut_game.dat";
constexpr const char* kHeader = "donut-save 1";
} // namespace

void SaveGame::Load()
{
	_values.clear();
	_sets.clear();
	std::ifstream file(kFile);
	std::string line;
	if (!file || !std::getline(file, line) || line != kHeader)
	{
		importOldFiles();
		return;
	}
	while (std::getline(file, line))
	{
		const std::size_t first = line.find('\t');
		const std::size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
		if (second == std::string::npos)
			continue;
		const std::string kind = line.substr(0, first);
		const std::string name = line.substr(first + 1, second - first - 1);
		const std::string rest = line.substr(second + 1);
		if (kind == "value")
			_values[name] = rest;
		else if (kind == "set")
			_sets[name].insert(rest);
	}
	Log::Info("SaveGame: {} values, {} sets from {}", _values.size(), _sets.size(), kFile);
}

void SaveGame::Save() const
{
	// Beside it, then in its place: a write cut short leaves the last save.
	const std::string temporary = std::string(kFile) + ".tmp";
	{
		std::ofstream file(temporary, std::ios::trunc);
		if (!file)
			return;
		file << kHeader << "\n";
		for (const auto& [key, value] : _values)
			file << "value\t" << key << "\t" << value << "\n";
		for (const auto& [name, entries] : _sets)
			for (const auto& entry : entries)
				file << "set\t" << name << "\t" << entry << "\n";
		if (!file)
			return;
	}
	std::rename(temporary.c_str(), kFile);
}

std::string SaveGame::Get(const std::string& key, const std::string& otherwise) const
{
	const auto found = _values.find(key);
	return found != _values.end() ? found->second : otherwise;
}

void SaveGame::Set(const std::string& key, const std::string& value)
{
	_values[key] = value;
}

void SaveGame::Erase(const std::string& key)
{
	_values.erase(key);
}

std::map<std::string, std::string> SaveGame::GetAll(const std::string& prefix) const
{
	std::map<std::string, std::string> found;
	for (auto it = _values.lower_bound(prefix); it != _values.end() && it->first.compare(0, prefix.size(), prefix) == 0; ++it)
		found[it->first.substr(prefix.size())] = it->second;
	return found;
}

const std::set<std::string>& SaveGame::Entries(const std::string& set) const
{
	static const std::set<std::string> none;
	const auto found = _sets.find(set);
	return found != _sets.end() ? found->second : none;
}

bool SaveGame::Add(const std::string& set, const std::string& entry)
{
	return _sets[set].insert(entry).second;
}

void SaveGame::importOldFiles()
{
	const auto firstLine = [](const char* path) {
		std::ifstream file(path);
		std::string line;
		std::getline(file, line);
		return line;
	};
	// "<mission> <level>", and the coins, a line each.
	if (const std::string progress = firstLine("donut_progress.dat"); !progress.empty())
		_values["progress"] = progress;
	if (const std::string coins = firstLine("donut_coins.dat"); !coins.empty())
		_values["coins"] = coins;
	// A line a card, gag or wasp camera found.
	for (const auto& [path, set] : {std::pair<const char*, const char*>{"donut_cards.dat", "card"},
	                                {"donut_gags.dat", "gag"},
	                                {"donut_wasps.dat", "wasp"}})
	{
		std::ifstream file(path);
		for (std::string line; std::getline(file, line);)
			if (!line.empty())
				_sets[set].insert(line);
	}
	// "<key> <seconds>" a mission's best time ("L4:m1sd 3.00291").
	{
		std::ifstream file("donut_save.dat");
		for (std::string line; std::getline(file, line);)
		{
			std::istringstream fields(line);
			std::string key, seconds;
			if (fields >> key >> seconds && key.size() > 1 && key[0] == 'L' && key.find(':') != std::string::npos)
				_values["time:" + key] = seconds;
		}
	}
	if (!_values.empty() || !_sets.empty())
		Log::Info("SaveGame: no {}; {} values, {} sets read from the files of before", kFile, _values.size(), _sets.size());
}

} // namespace Donut
