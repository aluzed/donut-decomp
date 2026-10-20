#pragma once

#include "Core/Math/Matrix4x4.h"

#include <memory>
#include <string>

namespace Donut
{

// The game's movies (movies/*.rmv: Bink video, 640 x 480 at 30 frames a
// second, four soundtracks, a language each), decoded with FFmpeg: the
// pictures drawn over the whole screen, as large as it holds them, the
// soundtrack played through OpenAL. Its sound is the clock the pictures keep
// to. Built without FFmpeg, no movie opens.
class MoviePlayer
{
public:
	MoviePlayer();
	~MoviePlayer();

	// Whether movies can be played at all.
	static bool IsAvailable();

	// Starts `path` with its soundtrack `track` (0 English, 1 French, 2
	// German, 3 Spanish; the first if it has fewer) at `gain`; false if it
	// cannot be read.
	bool Open(const std::string& path, int track, float gain);
	bool IsPlaying() const;
	// Decodes up to where `dt` more seconds take it, its picture then shown.
	void Update(double dt);
	// Over a black screen of `width` x `height`.
	void Draw(const Matrix4x4& proj, int width, int height);
	void Stop();
	// Seconds into it.
	double GetTime() const;

private:
	struct Decoder;
	std::unique_ptr<Decoder> _decoder;
};

} // namespace Donut
