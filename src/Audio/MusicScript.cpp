#include "MusicScript.h"

#include "Core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>

namespace Donut
{

namespace
{
struct Token
{
	std::size_t begin, end; // end past its terminating zero
	std::string text;
};

bool nameChar(uint8_t c)
{
	return std::isalnum(c) || c == '_' || c == '\\' || c == '.' || c == ' ';
}

std::string lower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
	return text;
}

bool contains(const std::string& text, const char* part)
{
	return lower(text).find(part) != std::string::npos;
}

std::string baseName(const std::string& path)
{
	const auto slash = path.find_last_of('\\');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

// A sound file's name as the archives keep it.
std::string soundFile(const std::string& name)
{
	return "sound\\music\\" + lower(name) + ".rsd";
}
} // namespace

std::string MusicScript::key(const std::string& name)
{
	std::string out;
	for (const char c : name)
		if (c != '_')
			out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

bool MusicScript::Load(const std::vector<uint8_t>& data)
{
	_cues.clear();
	// The objects' names start with the composition's own.
	static const char kRoot[] = "my_comp";
	const auto root = std::search(data.begin(), data.end(), kRoot, kRoot + sizeof(kRoot));
	if (root == data.end())
		return false;

	// Every name: a letter, two more of a name's characters at least, a zero.
	std::vector<Token> tokens;
	for (std::size_t at = static_cast<std::size_t>(root - data.begin()); at < data.size();)
	{
		if (!std::isalpha(data[at]))
		{
			++at;
			continue;
		}
		std::size_t end = at;
		while (end < data.size() && nameChar(data[end]))
			++end;
		if (end < data.size() && data[end] == 0 && end - at >= 3)
		{
			tokens.push_back({at, end + 1, std::string(reinterpret_cast<const char*>(&data[at]), end - at)});
			at = end + 1;
		}
		else
			++at;
	}
	if (tokens.empty())
		return false;

	// Runs of names with nothing between them: a group's clips. The others are
	// each followed by their data: regions, then events.
	std::vector<std::vector<std::string>> runs;
	for (std::size_t i = 1; i < tokens.size(); ++i)
	{
		if (!runs.empty() && i > 1 && tokens[i].begin == tokens[i - 1].end)
			runs.back().push_back(tokens[i].text);
		else
			runs.push_back({tokens[i].text});
	}
	std::vector<Group> groups;
	for (const auto& run : runs)
	{
		const bool clips = run.size() >= 2 &&
		                   std::none_of(run.begin(), run.end(), [](const std::string& n) {
			                   return n.size() > 7 && lower(n).compare(n.size() - 7, 7, "_region") == 0;
		                   });
		if (clips)
		{
			if (groups.empty() || !groups.back().regions.empty())
				groups.emplace_back();
			// A sound file then its clip ("Homer\Tuba_001", "Tuba_001";
			// "Motown_Intro", "M5_Intro"), or a clip alone.
			for (std::size_t i = 0; i < run.size(); ++i)
			{
				const std::string& name = run[i];
				const bool pair = i + 1 < run.size() &&
				                  (name.find('\\') != std::string::npos || baseName(name) == run[i + 1] ||
				                   lower(baseName(name)).rfind(lower(run[i + 1]), 0) == 0);
				if (pair)
				{
					groups.back().clips.emplace_back(run[i + 1], soundFile(name));
					++i;
				}
				else
					groups.back().clips.emplace_back(name, std::string());
			}
			continue;
		}
		for (const auto& name : run)
		{
			if (groups.empty())
				continue;
			if (name.size() > 7 && lower(name).compare(name.size() - 7, 7, "_region") == 0)
				groups.back().regions.push_back(name);
			else if (!groups.back().regions.empty())
				groups.back().events.push_back(name);
		}
	}

	// Every clip's sound file, wherever it was declared: a group may play
	// another's (the bonus missions, level 1 m1's).
	std::map<std::string, std::string> files;
	for (const auto& group : groups)
		for (const auto& [clip, file] : group.clips)
			if (!file.empty())
				files.emplace(clip, file);

	for (const auto& group : groups)
	{
		std::map<std::string, std::string> regions; // by key
		for (const auto& region : group.regions)
			regions[key(region)] = region;
		const auto clipsWhere = [&](auto&& keep) {
			std::vector<std::string> out;
			for (const auto& [clip, file] : group.clips)
				if (keep(clip))
				{
					const auto found = files.find(clip);
					if (found != files.end())
						out.push_back(found->second);
				}
			return out;
		};
		// A region's music, by what its name says it is.
		const auto cueFor = [&](const std::string& region) -> Cue {
			const std::string r = lower(region);
			Cue cue;
			const auto has = [](const char* part) { return [part](const std::string& c) { return contains(c, part); }; };
			if (r.find("end_pos") != std::string::npos || r.rfind("win_", 0) == 0)
				cue.once = clipsWhere(has("end_pos"));
			else if (r.find("end_neg") != std::string::npos || r.rfind("lose_", 0) == 0 ||
			         r.find("caught") != std::string::npos)
				cue.once = clipsWhere(has("end_neg"));
			else if (r.find("end_sus") != std::string::npos || r.find("getoutofcar") != std::string::npos ||
			         r.find("_sus_") != std::string::npos)
				cue.once = clipsWhere(has("end_sus"));
			else if (r.find("drama") != std::string::npos)
			{
				cue.once = clipsWhere(has("drama_pickup"));
				cue.loop = clipsWhere([](const std::string& c) { return contains(c, "drama") && !contains(c, "pickup"); });
			}
			else if (r.find("10sec") != std::string::npos)
				cue.loop = clipsWhere(has("10sec"));
			else
			{
				// A start, main or plain region: what is not an ending, a drama,
				// a reward; a pickup or an intro first.
				const auto plain = [](const std::string& c) {
					return !contains(c, "end_") && !contains(c, "drama") && !contains(c, "10sec") &&
					       !contains(c, "reward") && !contains(c, "win_") && !contains(c, "lose_");
				};
				cue.once = clipsWhere([&](const std::string& c) {
					return plain(c) && (contains(c, "pickup") || contains(c, "intro"));
				});
				cue.loop = clipsWhere([&](const std::string& c) {
					return plain(c) && !contains(c, "pickup") && !contains(c, "intro");
				});
			}
			return cue;
		};
		// An event's region, by its name.
		static const std::map<std::string, std::string> kNamed = {
		    {"ofexploremission", "ofexploreregion"},     {"hitandrun", "hitrunregion"},
		    {"hitandruncaught", "hitruncaughtregion"},   {"sundaydrivestart", "sundaydriveregion"},
		    {"sundaydrivegetoutofcar", "sundaydrivesusregion"}, {"ofkiwkemart", "kwikemartregion"},
		    {"enterstonecutterstunnel", "stonecutterstunnelregion"}};
		static const std::regex kMission("(.*)(start|win|lose|getoutofcar|drama|10sectogo)");
		static const std::regex kRace("streetrace0(\\d)(start|win|lose|getoutofcar)");
		for (const auto& event : group.events)
		{
			const std::string e = key(event);
			std::string region;
			const auto pick = [&](const std::string& candidate) {
				if (region.empty() && regions.count(candidate))
					region = regions[candidate];
			};
			if (const auto named = kNamed.find(e); named != kNamed.end())
				pick(named->second);
			pick(e + "region");
			std::smatch match;
			if (std::regex_match(e, match, kRace))
			{
				const std::string n = match[1], what = match[2];
				pick(what == "start" ? "streetrace" + n + "startregion" : what + "streetrace" + n + "region");
			}
			if (std::regex_match(e, match, kMission))
			{
				const std::string p = match[1], what = match[2];
				if (what == "start")
				{
					pick(p + "startregion");
					pick(p + "mainregion");
				}
				else if (what == "win")
					pick(p + "endposregion");
				else if (what == "lose")
					pick(p + "endnegregion");
				else if (what == "getoutofcar")
					pick(p + "endsusregion");
				else if (what == "drama")
					pick(p + "dramaregion");
				else
					pick(p + "10secregion");
			}
			// Going into a building fades the music out, which it is left to do
			// on its own here: its clip is the Kwik-E-Mart's muzak.
			if (region.empty() || key(region) == "enteringbuildingregion")
				continue;
			Cue cue = cueFor(region);
			if (!cue.once.empty() || !cue.loop.empty())
				_cues.emplace(e, std::move(cue));
		}
	}
	Log::Info("MusicScript: {} groups, {} events with music", groups.size(), _cues.size());
	return !groups.empty();
}

const MusicScript::Cue* MusicScript::Find(const std::string& event) const
{
	const auto found = _cues.find(key(event));
	return found != _cues.end() ? &found->second : nullptr;
}

} // namespace Donut
