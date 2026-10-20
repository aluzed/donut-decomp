#pragma once

#include <map>
#include <set>
#include <string>

namespace Donut
{

// The game's save, one file (donut_game.dat): the story reached, the coins, the
// cards, gags and wasp cameras found, the bonus missions and street races won,
// the cars and clothes bought on each level and those in use, the missions'
// best times.
//
// Values by key ("coins", "progress", "time:L4:m1sd") and sets of entries by
// name ("card", "won", "car:1"), a line each:
//
//   donut-save 1
//   value<TAB>coins<TAB>60
//   set<TAB>card<TAB>card11 120 -45
//
// Without the file, what the separate files of before kept (donut_progress.dat,
// donut_coins.dat, donut_cards.dat, donut_gags.dat, donut_wasps.dat,
// donut_save.dat) is read instead, and written into it at the first save; those
// files are left as they were.
class SaveGame
{
public:
	void Load();
	// Writes the whole of it, to a file beside it then in its place.
	void Save() const;

	// A value, or `otherwise`.
	std::string Get(const std::string& key, const std::string& otherwise = {}) const;
	void Set(const std::string& key, const std::string& value);
	void Erase(const std::string& key);
	// The values whose keys start with `prefix`, by the rest of their keys.
	std::map<std::string, std::string> GetAll(const std::string& prefix) const;

	const std::set<std::string>& Entries(const std::string& set) const;
	// True when it was not there yet.
	bool Add(const std::string& set, const std::string& entry);

private:
	std::map<std::string, std::string> _values;
	std::map<std::string, std::set<std::string>> _sets;
	// The separate files of before.
	void importOldFiles();
};

} // namespace Donut
