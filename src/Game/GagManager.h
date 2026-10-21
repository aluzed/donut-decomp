#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Donut
{

class CompositeModel;
class Level;
class SceneSkins;
struct VertexAnimation;
namespace GL
{
class ShaderProgram;
}

// The level's interactive gags (level.mfk's GagBegin ... GagEnd): Moe's
// pickle jar, the Simpsons' couch. Each plays its sound when walked into
// ("touch") or worked by hand ("action"), and pays its coins the first time.
// What playing one does (its sound, its movie, the coins) is the game's; this
// keeps the gags, finds the one at hand and draws them.
class GagManager
{
public:
	struct Gag
	{
		std::string model;
		// Its model, as it stands before it plays (art/nis/gags/), where
		// GagSetPosition puts it. Not animated.
		std::shared_ptr<CompositeModel> drawable;
		// The characters it holds, if any (Homer on the couch).
		std::shared_ptr<SceneSkins> skins;
		// A mesh of it that changes shape (the blow-up doll inflating).
		std::shared_ptr<VertexAnimation> morph;
		Vector3 placed;
		// GagSetCycle: "cycle" plays round and round; "single" plays once when
		// triggered and stays as it ends; "reset" plays once and goes back.
		std::string cycle;
		// When it was last triggered, < 0 when it stands at rest.
		double animStartedAt = -1.0;
		// GagSetRandom: one of the gags sharing its trigger is picked each time
		// (the Simpsons' couch has six, from Grandpa walking in to Lisa).
		bool random = false;
		std::string sound;
		bool byHand = false;
		Vector3 position;
		float radius = 1.0f;
		int coins = 0;
		// GagSetCameraShake: the power plant's meltdown shakes the view.
		bool shakes = false;
		// GagPlayFMV: the observatory's telescope shows a movie ("tele.rmv").
		std::string movie;
		bool played = false;
		bool inside = false;
		double lastPlayed = -100.0;
		// GagSetInterior: the building it is in, empty outdoors.
		std::string interior;
		// GagSetPersist: played once ("single"), it stays as it ended, on
		// the level's next load too; and it does so now.
		bool persists = false;
		bool ended = false;
		// GagSetSparkle: it glints until it is found.
		bool sparkle = false;
	};
	// GagCheckMovie: the Aztec's box office (level 3), worked by hand, shows
	// the Itchy & Scratchy Movie to who holds its ticket (all the cards
	// found); to the others, the clerk's refusal (a conversation, "aztec").
	// Not a gag found.
	struct MovieGag
	{
		Vector3 position;
		float radius = 1.0f;
		std::string movie, refusal;
	};
	// GagCheckCollCards: Comic Book Guy's Itchy & Scratchy Movie ticket (level
	// 3), worked by hand: traded for a whole set of collector cards. The
	// conversations of the trade, of its terms, of a refusal.
	struct CardGag
	{
		Vector3 position;
		float radius = 1.0f;
		std::string interior;
		std::string accept, terms, refusal;
		bool sparkle = false;
	};

	// Reads scripts/<levelDirectory>/level.mfk's gags, their triggers at
	// `level`'s locators; `found` holds the ids of those found before.
	void Load(const std::string& levelDirectory, const Level& level, const std::set<std::string>& found);

	// A gag's id in the save, by level, model and place.
	std::string Id(const Gag& gag) const;
	std::vector<std::string> GetFoundIds() const;
	int GetFound() const;
	int GetTotal() const { return static_cast<int>(_gags.size()); }

	// Whether it is where the player is: in `interior`, or outdoors when empty.
	static bool IsHere(const Gag& gag, const std::string& interior);

	// The gags walked into by a player at `player`, each played by `play`,
	// once on the way in and not again for a few seconds.
	void Update(const Vector3& player, const std::string& interior, double now, const std::function<void(Gag&)>& play);
	// The gag worked by hand within reach of `player`, or none.
	Gag* FindByHand(const Vector3& player, const std::string& interior);
	const MovieGag* FindMovieGag(const Vector3& player) const;
	const CardGag* FindCardGag(const Vector3& player, const std::string& interior) const;
	// Where the glinting gags not yet found are, within `range` of `player`;
	// the card trade's too while `cardsTraded` is false.
	std::vector<Vector3> GetSparkles(const Vector3& player, const std::string& interior, float range, bool cardsTraded) const;

	void Draw(GL::ShaderProgram& worldShader, GL::ShaderProgram& skinShader, const Matrix4x4& viewProjection,
	          const Vector3& camera, const std::string& interior, double now);

private:
	std::string _levelDirectory;
	std::vector<Gag> _gags;
	std::vector<MovieGag> _movieGags;
	std::vector<CardGag> _cardGags;
	// False until the first look after loading: a gag the player starts the
	// level standing in is not walked into.
	bool _primed = false;
};

} // namespace Donut
