#include "Game/SuperSprint.h"

#include "AI/PathGraph.h"
#include "AI/RaceOpponent.h"
#include "Core/FileSystem.h"
#include "Core/Log.h"
#include "Game.h"
#include "P3D/P3DFile.h"
#include "P3D/P3D.generated.h"
#include "Render/Mesh.h"
#include "Render/OpenGL/ShaderProgram.h"
#include "ResourceManager.h"
#include "Scripting/ScriptEngine.h"
#include "Vehicle.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>

namespace Donut
{

namespace
{
constexpr double kCountdownSeconds = 3.0;
// A nitro burns this long, the car's top speed and pull this many times its
// own. Not read from the game: chosen to be felt without flinging the car
// off the track's tight corners.
constexpr double kNitroSeconds = 1.5;
constexpr float kNitroBoost = 1.6f;
constexpr int kMaxNitros = 5;
// Each car starts with one.
constexpr int kStartNitros = 1;
// A pickup taken is back after this long.
constexpr double kPickupRespawnSeconds = 10.0;
constexpr float kPickupReach = 3.0f;
// A blow sharper than this (m/s lost in a frame, sideways or head on) dents
// the car, a hundredth of its health a m/s past it.
constexpr float kDentSpeed = 8.0f;
constexpr float kDentPerSpeed = 0.012f;
// A car put back waits this long off the track first.
constexpr double kLostSeconds = 1.2;
// A player's car this slow this long with the gas down is put back; a
// computer's, gone no further than this.
constexpr double kStuckSeconds = 4.0;
constexpr float kStuckMetres = 3.0f;
// The computer's: long enough for its own backing out of a wedge.
constexpr double kComputerStuckSeconds = 8.0;
// A ramp's run-up launches a car heading on at least this fast (m/s): not
// the game's figure, the one that clears B07's gap.
constexpr float kRampSpeed = 26.0f;
// Once the players are over the line, the others have this long.
constexpr double kStragglerSeconds = 15.0;
// The computer racing alone: once the winner is.
constexpr double kComputerStragglerSeconds = 30.0;
// The braking a car gets below this speed is the reverse gear (as in game).
constexpr float kReverseKmh = 5.0f;

Quaternion facing(float heading)
{
	return Quaternion(Vector3(0.0f, 1.0f, 0.0f), heading);
}

float headingOf(const Quaternion& rotation)
{
	const Vector3 forward = rotation * Vector3(0.0f, 0.0f, 1.0f);
	return std::atan2(forward.X, forward.Z);
}

// The track's two files: B07's are capitals on the CD.
std::string trackFile(int track, const char* suffix)
{
	for (const char* prefix : {"b0", "B0"})
		if (const std::string name = fmt::format("{}{}{}.p3d", prefix, track, suffix);
		    FileSystem::exists("./art/" + name))
			return name;
	return fmt::format("b0{}{}.p3d", track, suffix);
}
} // namespace

std::vector<std::string> SuperSprint::TrackFiles(int track)
{
	return {trackFile(track, ""), trackFile(track, "data")};
}

SuperSprint::SuperSprint(Level& level, WorldPhysics& physics, const PathGraph& roads, const Setup& setup)
    : _level(level), _physics(physics), _setup(setup)
{
	_setup.track = std::clamp(_setup.track, 1, kTracks);
	_setup.laps = std::clamp(_setup.laps, 1, 9);
	readCamera(_setup.track);
	readTrack(_setup.track);

	const std::vector<Vector3> circuit = aiCircuit(roads, _setup.track);
	auto& script = Game::GetInstance().GetScriptEngine();
	for (int i = 0; i < kRacers; ++i)
	{
		Racer racer;
		racer.entry = _setup.entries[i];
		const std::string start = fmt::format("car{}", i + 1);
		Vector3 position = _level.GetLocatorPosition(start);
		float heading = 0.0f;
		_level.GetLocatorHeading(start, heading);
		if (_setup.reverse)
			heading += 3.14159265f;
		std::string model = "art/cars/" + racer.entry.car + ".p3d";
		if (!FileSystem::exists(model))
			model = "art/cars/famil_v.p3d";
		racer.vehicle = std::make_unique<Vehicle>(racer.entry.car);
		racer.vehicle->LoadModel(model);
		script.applyCarScript(*racer.vehicle);
		racer.vehicle->CreatePhysicsBody(_physics, position + Vector3(0.0f, 0.5f, 0.0f));
		racer.vehicle->Teleport(position + Vector3(0.0f, 0.5f, 0.0f), facing(heading));
		racer.safePosition = position + Vector3(0.0f, 0.5f, 0.0f);
		racer.safeRotation = facing(heading);
		racer.nitros = kStartNitros;
		if (racer.entry.player < 0 && circuit.size() >= 2)
			racer.ai = std::make_unique<RaceOpponent>(*racer.vehicle, circuit, false, true);
		_racers.push_back(std::move(racer));
	}
	Log::Info("SuperSprint: track {} {}, {} laps, {} checkpoints, {} pickups, AI circuit of {} points", _setup.track,
	          _setup.reverse ? "reversed" : "forwards", _setup.laps, _checkpoints.size(), _pickups.size(),
	          circuit.size());
}

SuperSprint::~SuperSprint()
{
	for (auto& racer : _racers)
	{
		racer.ai.reset();
		if (racer.vehicle)
			racer.vehicle->DestroyPhysics(_physics);
	}
}

void SuperSprint::readCamera(int track)
{
	const std::string file = "./art/" + trackFile(track, "");
	if (!FileSystem::exists(file))
		return;
	const P3D::P3DFile p3d(file);
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (!chunk->IsType(P3D::ChunkType::Camera))
			continue;
		const auto camera = P3D::Camera::Load(*chunk);
		if (camera->GetName() != "sprint_camShape")
			continue;
		_camera.position = camera->GetPosition();
		_camera.look = camera->GetForward().Normalized();
		_camera.up = camera->GetUp().Normalized();
		_camera.fov = camera->GetFov();
		// Its own near plane is 100 m and its far 1000 m, short of the track
		// 2200 m off: as the game draws it, the far plane is not kept.
		const float distance = camera->GetPosition().Length();
		_camera.nearPlane = std::max(1.0f, distance - 300.0f);
		_camera.farPlane = distance + 300.0f;
	}
}

void SuperSprint::readTrack(int track)
{
	const std::string file = "./art/" + trackFile(track, "data");
	if (!FileSystem::exists(file))
	{
		Log::Warn("SuperSprint: no {}", file);
		return;
	}
	const P3D::P3DFile p3d(file);
	std::map<std::string, Vector3> waypoints;
	for (const auto& chunk : p3d.GetRoot().GetChildren())
	{
		if (!chunk->IsType(P3D::ChunkType::Locator2))
			continue;
		const auto locator = P3D::Locator2::Load(*chunk);
		const std::string& name = locator->GetName();
		const auto& data = locator->GetData();
		const auto type = static_cast<Level::LocatorType>(locator->GetType());
		std::string lower = name;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
		if (type == Level::LocatorType::Action)
		{
			// Their action is "Nitro" or "Wrench"; b02's second nitro says
			// ToggleOnOff, its name nitro1.
			if (lower.compare(0, 5, "nitro") == 0)
				_pickups.push_back({locator->GetPosition(), true});
			else if (lower.compare(0, 6, "wrench") == 0 || lower.compare(0, 6, "repair") == 0)
				_pickups.push_back({locator->GetPosition(), false});
			continue;
		}
		if (type != Level::LocatorType::Event || data.empty())
			continue;
		if (data[0] == 2 && (lower.compare(0, 3, "wpr") == 0 || lower.compare(0, 3, "wpl") == 0))
		{
			if ((lower[2] == 'l') == _setup.reverse)
				waypoints[lower] = locator->GetPosition();
		}
		else if (data[0] == 2 && data.size() >= 2)
			_checkpoints.push_back({name, static_cast<int>(data[1]), locator->GetPosition()});
		else if (data[0] == 4)
			_lostZones.push_back(name);
		else if (data[0] == 0x30)
			_jumps.push_back(name);
	}
	std::sort(_checkpoints.begin(), _checkpoints.end(),
	          [](const Checkpoint& a, const Checkpoint& b) { return a.order < b.order; });
	// The other way round: the finish, then the rest backwards.
	if (_setup.reverse && _checkpoints.size() > 1)
		std::reverse(_checkpoints.begin() + 1, _checkpoints.end());
	_lowest = 0.0f;
	for (const auto& checkpoint : _checkpoints)
		_lowest = std::min(_lowest, checkpoint.position.Y);
	for (const auto& [name, position] : waypoints)
		_lowest = std::min(_lowest, position.Y);
}

std::vector<Vector3> SuperSprint::aiCircuit(const PathGraph& roads, int track) const
{
	// The waypoints of the way round, in their names' order, threaded through
	// the track's roads.
	const std::string file = "./art/" + trackFile(track, "data");
	std::vector<Vector3> points;
	{
		std::map<std::string, Vector3> named;
		const P3D::P3DFile p3d(file);
		for (const auto& chunk : p3d.GetRoot().GetChildren())
		{
			if (!chunk->IsType(P3D::ChunkType::Locator2))
				continue;
			const auto locator = P3D::Locator2::Load(*chunk);
			std::string lower = locator->GetName();
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
			if (lower.size() > 3 && lower.compare(0, 2, "wp") == 0 && (lower[2] == 'l') == _setup.reverse &&
			    (lower[2] == 'l' || lower[2] == 'r'))
				named[lower] = locator->GetPosition();
		}
		for (const auto& [name, position] : named)
			points.push_back(position);
	}
	if (points.size() < 2 || _checkpoints.size() < 2)
		return points;

	// They are numbered round the way the track runs; should they run the
	// other way, as the checkpoints say, they are turned round.
	const int count = static_cast<int>(_checkpoints.size());
	auto nearest = [&](const Vector3& at) {
		int best = 0;
		for (int i = 1; i < count; ++i)
			if ((_checkpoints[i].position - at).LengthSquared() < (_checkpoints[best].position - at).LengthSquared())
				best = i;
		return best;
	};
	int onwards = 0;
	for (std::size_t i = 0; i < points.size(); ++i)
	{
		const int step = (nearest(points[(i + 1) % points.size()]) - nearest(points[i]) + count) % count;
		if (step == 0)
			continue;
		onwards += step <= count / 2 ? 1 : -1;
	}
	if (onwards < 0)
		std::reverse(points.begin(), points.end());

	std::vector<Vector3> circuit;
	for (std::size_t i = 0; i < points.size(); ++i)
	{
		const Vector3& from = points[i];
		const Vector3& to = points[(i + 1) % points.size()];
		circuit.push_back(from);
		const auto leg = roads.FindRoute(from, to);
		for (std::size_t k = 1; k + 1 < leg.size(); ++k)
			circuit.push_back(leg[k]);
	}
	return circuit;
}

bool SuperSprint::inside(const std::string& locator, const Vector3& position) const
{
	for (const auto& trigger : _level.GetTriggers())
		if (trigger.locator == locator && trigger.Contains(position))
			return true;
	return false;
}

bool SuperSprint::overLostZone(const Vector3& position) const
{
	// Above one or in it: the ground under it is where cars are lost.
	for (const auto& zone : _lostZones)
		for (const auto& trigger : _level.GetTriggers())
			if (trigger.locator == zone && trigger.Contains(Vector3(position.X, trigger.position.Y, position.Z)))
				return true;
	return false;
}

void SuperSprint::Update(double dt, const std::array<Controls, kRacers>& controls)
{
	_events = Events();
	if (_phase == Phase::Countdown)
	{
		const int before = static_cast<int>(std::ceil(_countdown));
		_countdown = std::max(0.0, _countdown - dt);
		const int after = static_cast<int>(std::ceil(_countdown));
		if (after != before && after > 0)
			_events.countdownTick = after;
		if (_countdown <= 0.0)
		{
			_phase = Phase::Racing;
			_events.started = true;
		}
		for (auto& racer : _racers)
		{
			if (racer.ai)
				racer.ai->Hold();
			else
				racer.vehicle->ApplyInput(0.0f, 0.0f, 1.0f, 1.0f);
			racer.lastVelocity = racer.vehicle->GetVelocity();
		}
		for (auto& racer : _racers)
			racer.vehicle->Update(dt);
		rank();
		return;
	}

	_raceTime += dt;
	for (auto& pickup : _pickups)
		pickup.goneFor = std::max(0.0, pickup.goneFor - dt);
	float leader = 0.0f;
	for (const auto& racer : _racers)
		if (racer.entry.player >= 0)
			leader = std::max(leader, racer.progress);
	for (int i = 0; i < kRacers; ++i)
		updateRacer(_racers[i], i, dt, controls[i], leader);
	for (auto& racer : _racers)
		racer.vehicle->Update(dt);
	rank();

	if (_phase == Phase::Racing)
	{
		bool everyone = true, players = true, anyPlayer = false;
		for (const auto& racer : _racers)
		{
			everyone = everyone && racer.finished;
			if (racer.entry.player >= 0)
			{
				anyPlayer = true;
				players = players && racer.finished;
			}
		}
		// The others have a while once the players are over the line; with
		// none (the computer racing alone), once the winner is.
		bool anyone = false;
		for (const auto& racer : _racers)
			anyone = anyone || racer.finished;
		if ((players && anyPlayer) || (!anyPlayer && anyone))
			_finishedFor += dt;
		const double grace = anyPlayer ? kStragglerSeconds : kComputerStragglerSeconds;
		if (everyone || _finishedFor > grace)
		{
			_phase = Phase::Finished;
			Log::Info("SuperSprint: race over at {:.1f}s", _raceTime);
		}
	}
}

void SuperSprint::updateRacer(Racer& racer, int index, double dt, const Controls& controls, float leaderProgress)
{
	Vehicle& car = *racer.vehicle;
	const Vector3 position = car.GetPosition();
	const int count = static_cast<int>(_checkpoints.size());

	// Off the track a moment, then back at its last checkpoint.
	if (racer.lostFor > 0.0)
	{
		racer.lostFor -= dt;
		car.ApplyInput(0.0f, 0.0f, 1.0f, 1.0f);
		if (racer.lostFor <= 0.0)
			putBack(racer);
		racer.lastVelocity = car.GetVelocity();
		return;
	}

	if (!racer.finished && count > 0)
	{
		// Through the next checkpoint, in order: one missed is gone back for.
		// The computer's own way of freeing a car wedged in a corner puts it
		// further on, past one at times: it may take the one after.
		bool through = inside(_checkpoints[racer.next].locator, position);
		if (!through && racer.ai && count > 2 && inside(_checkpoints[(racer.next + 1) % count].locator, position))
		{
			// The finish missed still counts its lap; the car is in the one
			// after it, taken next frame.
			if (racer.next != 0)
				racer.next = (racer.next + 1) % count;
			through = true;
		}
		if (through)
		{
			if (racer.next == 0)
			{
				const double lap = _raceTime - racer.lapStartedAt;
				racer.lapStartedAt = _raceTime;
				if (racer.bestLap < 0.0 || lap < racer.bestLap)
					racer.bestLap = lap;
				++racer.laps;
				_events.lapsDone.push_back(index);
				Log::Info("SuperSprint: {} ({}) lap {} in {:.1f}s", index + 1, racer.entry.car, racer.laps, lap);
				if (racer.laps >= _setup.laps)
				{
					racer.finished = true;
					racer.totalTime = _raceTime;
					Log::Info("SuperSprint: {} ({}) finished in {:.1f}s", index + 1, racer.entry.car, racer.totalTime);
				}
			}
			racer.next = (racer.next + 1) % count;
			// Put back where it went through, wheels down and over no place
			// where cars are lost; B07's fifth is in the air, over the water,
			// halfway through a jump: there the last spot stands.
			if (!car.IsAirborne() && !overLostZone(position))
			{
				racer.safePosition = position + Vector3(0.0f, 0.5f, 0.0f);
				racer.safeRotation = facing(headingOf(car.GetRotation()));
			}
		}
		const Vector3& from = _checkpoints[(racer.next + count - 1) % count].position;
		const Vector3& to = _checkpoints[racer.next].position;
		const float span = std::max(1.0f, (to - from).Length());
		const float along = std::clamp(1.0f - (to - position).Length() / span, 0.0f, 1.0f);
		racer.progress = static_cast<float>(racer.laps * count + (racer.next + count - 1) % count) + along;
	}

	// The pickups it drives through.
	for (auto& pickup : _pickups)
	{
		if (pickup.goneFor > 0.0 || (pickup.position - position).Length() > kPickupReach)
			continue;
		pickup.goneFor = kPickupRespawnSeconds;
		if (pickup.nitro)
		{
			racer.nitros = std::min(kMaxNitros, racer.nitros + 1);
			_events.pickedNitro.push_back(index);
		}
		else
		{
			racer.health = 1.0f;
			_events.pickedWrench.push_back(index);
		}
	}

	// Dented by what it runs into: the velocity it loses in a frame. A
	// landing (the fall stopped) does not count.
	const Vector3 velocity = car.GetVelocity();
	{
		Vector3 change = velocity - racer.lastVelocity;
		change.Y = 0.0f;
		const float blow = change.Length();
		if (blow > kDentSpeed && _raceTime > 0.5)
			racer.health = std::max(0.0f, racer.health - (blow - kDentSpeed) * kDentPerSpeed);
	}
	racer.lastVelocity = velocity;

	// Lost: wrecked, off the edge, in the water, fallen.
	const char* why = racer.health <= 0.0f ? "wrecked" : position.Y < _lowest - 15.0f ? "fallen" : nullptr;
	for (const auto& zone : _lostZones)
		if (!why && inside(zone, position))
			why = "in a death zone";
	if (!racer.finished)
	{
		// A player's car stuck with the gas down; the computer's going
		// nowhere (wedged on a kerb, against a wall): it is put back sooner
		// than its own way out, which drops it further round, past a
		// checkpoint or two.
		if (racer.ai)
		{
			if ((position - racer.stuckAt).Length() > kStuckMetres)
			{
				racer.stuckAt = position;
				racer.stuckFor = 0.0;
			}
			else
				racer.stuckFor += dt;
		}
		else if (std::fabs(car.GetSpeedKmh()) < 3.0f && (controls.gas > 0.5f || controls.brake > 0.5f))
			racer.stuckFor += dt;
		else
			racer.stuckFor = 0.0;
		if (!why && racer.stuckFor > (racer.ai ? kComputerStuckSeconds : kStuckSeconds))
			why = "stuck";
	}
	if (why)
	{
		Log::Info("SuperSprint: {} ({}) lost at ({:.1f}, {:.1f}, {:.1f}): {}", index + 1, racer.entry.car, position.X,
		          position.Y, position.Z, why);
		// Lost again as soon as put back: the spot it was put back at is no
		// good; the checkpoint's own, then, facing the next.
		if (_raceTime - racer.putBackAt < 3.0 && count > 0)
		{
			// The last checkpoint passed over solid ground.
			int back = (racer.next + count - 1) % count;
			for (int tries = 0; tries < count && overLostZone(_checkpoints[back].position); ++tries)
				back = (back + count - 1) % count;
			const Vector3 ahead = _checkpoints[(back + 1) % count].position - _checkpoints[back].position;
			racer.safePosition = _checkpoints[back].position + Vector3(0.0f, 0.5f, 0.0f);
			racer.safeRotation = facing(std::atan2(ahead.X, ahead.Z));
		}
		// Stuck again where it was stuck last: put back past the trap, at the
		// checkpoint it was making for.
		if (std::string(why) == "stuck" && (position - racer.lastStuckAt).Length() < 6.0f && count > 1)
		{
			const Vector3 ahead = _checkpoints[(racer.next + 1) % count].position - _checkpoints[racer.next].position;
			racer.safePosition = _checkpoints[racer.next].position + Vector3(0.0f, 0.5f, 0.0f);
			racer.safeRotation = facing(std::atan2(ahead.X, ahead.Z));
		}
		if (std::string(why) == "stuck")
			racer.lastStuckAt = position;
		racer.lostFor = kLostSeconds;
		racer.stuckFor = 0.0;
		racer.nitroFor = 0.0;
		_events.lost.push_back(index);
		return;
	}

	// Up a ramp's run-up, heading on: launched fast enough to clear its gap
	// (B07's is 31 m across, wider than the cars clear at the speed the
	// track's corners allow).
	bool ramp = false;
	for (const auto& jump : _jumps)
		ramp = ramp || inside(jump, position);
	if (ramp)
	{
		Vector3 flat(velocity.X, 0.0f, velocity.Z);
		const Vector3 ahead = car.GetRotation() * Vector3(0.0f, 0.0f, 1.0f);
		const float speed = flat.Length();
		// Not on landing, coming down the other side.
		if (speed > 5.0f && speed < kRampSpeed && velocity.Y > -2.0f && flat.Dot(ahead) > 0.7f * speed)
		{
			flat = flat * (kRampSpeed / speed);
			car.SetVelocity(Vector3(flat.X, velocity.Y, flat.Z));
			racer.lastVelocity = car.GetVelocity();
		}
	}

	racer.nitroFor = std::max(0.0, racer.nitroFor - dt);
	const bool wantsNitro = racer.ai ? racer.nitros > 0 && racer.place > 1 && car.GetSpeedKmh() > 40.0f &&
	                                       std::fmod(_raceTime + index * 1.7, 6.0) < dt
	                                 : controls.nitro;
	if (wantsNitro && racer.nitros > 0 && racer.nitroFor <= 0.0 && !racer.finished)
	{
		--racer.nitros;
		racer.nitroFor = kNitroSeconds;
		_events.nitrosUsed.push_back(index);
	}
	const float boost = racer.nitroFor > 0.0 ? kNitroBoost : 1.0f;

	if (racer.ai)
	{
		racer.ai->SetExtraBoost(boost);
		racer.ai->SetFloored(ramp);
		// The players' leader in laps, for the computer to keep up with.
		racer.ai->Update(dt, count > 0 ? leaderProgress / static_cast<float>(count) : -1.0f);
		return;
	}
	if (racer.finished)
	{
		car.ApplyInput(0.0f, 0.0f, 0.4f, 1.0f);
		return;
	}
	float throttle = controls.gas, brake = controls.brake;
	if (brake > 0.0f && car.GetSpeedKmh() < kReverseKmh)
	{
		throttle = -brake;
		brake = 0.0f;
	}
	car.ApplyInput(throttle, controls.steer, brake, boost);
}

void SuperSprint::putBack(Racer& racer)
{
	racer.vehicle->Teleport(racer.safePosition, racer.safeRotation);
	racer.health = 1.0f;
	racer.lastVelocity = Vector3::Zero;
	racer.stuckAt = racer.safePosition;
	racer.putBackAt = _raceTime;
	racer.stuckFor = 0.0;
	if (racer.ai)
	{
		// Rejoins its circuit from where it now stands.
		racer.ai = std::make_unique<RaceOpponent>(*racer.vehicle, racer.ai->GetCircuit(), false, true);
	}
}

void SuperSprint::rank()
{
	std::vector<int> order(kRacers);
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
		const Racer& x = _racers[a];
		const Racer& y = _racers[b];
		if (x.finished != y.finished)
			return x.finished;
		if (x.finished)
			return x.totalTime < y.totalTime;
		return x.progress > y.progress;
	});
	for (int place = 0; place < kRacers; ++place)
		_racers[order[place]].place = place + 1;
}

Mesh* SuperSprint::committed(const std::string& name)
{
	Mesh* mesh = Game::GetInstance().GetResourceManager().GetGeometry(name);
	if (mesh && _committed.insert(mesh).second)
		mesh->Commit();
	return mesh;
}

void SuperSprint::Draw(const Matrix4x4& viewProj, GL::ShaderProgram& worldShader, float time)
{
	Matrix4x4 vp = viewProj;
	for (const bool opaque : {true, false})
		for (auto& racer : _racers)
			if (racer.lostFor <= 0.0 && racer.vehicle->HasModel())
				racer.vehicle->Draw(vp, worldShader, opaque);

	// The pickups, turning where they lie.
	const Matrix4x4 spin(Quaternion::MakeFromEuler(Vector3(0.0f, time * 2.5f, 0.0f)));
	Mesh* nitro = committed("nitro_geoShape");
	Mesh* wrench = committed("wrench7Shape");
	worldShader.Bind();
	for (const bool opaque : {true, false})
		for (const auto& pickup : _pickups)
		{
			Mesh* mesh = pickup.nitro ? nitro : wrench;
			if (!mesh || pickup.goneFor > 0.0)
				continue;
			worldShader.SetUniformValue("viewProj",
			                            viewProj * Matrix4x4::MakeTranslate(pickup.position + Vector3(0.0f, 1.0f, 0.0f)) *
			                                spin);
			mesh->Draw(worldShader, opaque);
		}
}

} // namespace Donut
