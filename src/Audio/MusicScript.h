#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Donut
{

// A level's music script (sound\music\l<n>_music.rms, a Radical "radmusic"
// composition): which of the level's music a game event calls for -- "M1_start"
// a mission's theme, "M1_win" its ending, "Hit_and_Run" the chase.
//
// The composition is a serialized object graph (groups, rsd files, clips,
// regions, sequences, events, transitions) whose pointers are run-time
// addresses; it is not decoded here. Its names are read instead, in the order
// it keeps them, group by group: first the group's clips -- a run of names with
// nothing between them, each a sound file followed by its clip ("Collection02_
// End_Pos_b1_3", "Collection02_End_Pos"), or a clip of another group's alone --
// then its regions ("M1_start_region", "M1_end_Pos_region"), then its events
// ("M1_start", "M1_win"), each with its data after it. An event plays the
// region of its group its name calls for, and a region the clips of its group
// its name calls for: a start or main region the pickup (once) then the main
// (round and round), a drama region its drama, an ending its ending.
class MusicScript
{
public:
	// False when it holds no group.
	bool Load(const std::vector<uint8_t>& data);

	struct Cue
	{
		// Sound files ("sound\music\collection02_pickup.rsd"): played once, in
		// order, then the loop, round and round. No loop: the cue is over.
		std::vector<std::string> once;
		std::vector<std::string> loop;
	};
	// The music an event calls for (its name as the game gives it, any case,
	// underscores or not), or null.
	const Cue* Find(const std::string& event) const;
	std::size_t GetEventCount() const { return _cues.size(); }

private:
	struct Group
	{
		// Each clip, and its sound file when the group declares it.
		std::vector<std::pair<std::string, std::string>> clips;
		std::vector<std::string> regions;
		std::vector<std::string> events;
	};
	std::map<std::string, Cue> _cues;
	// The name an event goes by: lower case, without underscores.
	static std::string key(const std::string& name);
};

} // namespace Donut
