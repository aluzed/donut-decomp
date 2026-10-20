#include "MoviePlayer.h"

#include "Core/Log.h"
#include "Render/OpenGL/VertexBinding.h"
#include "Render/OpenGL/VertexBuffer.h"
#include "Render/OpenGL/glad/glad.h"
#include "Render/SpriteBatch.h"
#include "Render/Texture.h"

#include <AL/al.h>

#include <algorithm>
#include <deque>
#include <vector>

#ifdef DONUT_HAS_FFMPEG
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#endif

namespace Donut
{

#ifdef DONUT_HAS_FFMPEG

namespace
{
// The sound queued ahead of what is heard, and an OpenAL buffer's share of
// it, in samples a channel.
constexpr double kSoundAhead = 0.5;
constexpr int kBufferSamples = 4096;
} // namespace

struct MoviePlayer::Decoder
{
	AVFormatContext* format = nullptr;
	AVCodecContext* video = nullptr;
	AVCodecContext* audio = nullptr;
	int videoStream = -1, audioStream = -1;
	SwsContext* scale = nullptr;
	SwrContext* resample = nullptr;
	AVPacket* packet = nullptr;
	AVFrame* frame = nullptr;
	// Pictures decoded and not shown yet, oldest first, and when each shows.
	std::deque<std::pair<AVFrame*, double>> pictures;
	int64_t pictureCount = 0;
	std::vector<uint8_t> rgba;
	std::unique_ptr<Texture> texture;
	// No packet left to read; nothing left to show or hear.
	bool ended = false, over = false;

	// The sound: 16-bit stereo at its own rate, what is decoded and not
	// queued yet, the source and its buffers queued (with their samples) or
	// free, the samples queued and those heard to the end of their buffers.
	int rate = 0;
	std::vector<int16_t> pcm;
	ALuint source = 0;
	std::deque<std::pair<ALuint, int64_t>> queued;
	std::vector<ALuint> freeBuffers;
	int64_t samplesQueued = 0, samplesHeard = 0;
	bool started = false;
	double clock = 0.0;

	~Decoder()
	{
		if (source)
		{
			alSourceStop(source);
			alSourcei(source, AL_BUFFER, 0);
			alDeleteSources(1, &source);
		}
		for (const auto& [buffer, samples] : queued)
			alDeleteBuffers(1, &buffer);
		if (!freeBuffers.empty())
			alDeleteBuffers(static_cast<ALsizei>(freeBuffers.size()), freeBuffers.data());
		for (auto& [picture, time] : pictures)
			av_frame_free(&picture);
		sws_freeContext(scale);
		swr_free(&resample);
		avcodec_free_context(&video);
		avcodec_free_context(&audio);
		avformat_close_input(&format);
		av_packet_free(&packet);
		av_frame_free(&frame);
	}

	static AVCodecContext* openCodec(const AVStream* stream)
	{
		const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
		if (!codec)
			return nullptr;
		AVCodecContext* context = avcodec_alloc_context3(codec);
		if (avcodec_parameters_to_context(context, stream->codecpar) < 0 || avcodec_open2(context, codec, nullptr) < 0)
			avcodec_free_context(&context);
		return context;
	}

	void receivePictures()
	{
		const double timeBase = av_q2d(format->streams[videoStream]->time_base);
		while (avcodec_receive_frame(video, frame) == 0)
		{
			const int64_t at = frame->best_effort_timestamp;
			const double time = at != AV_NOPTS_VALUE ? at * timeBase : pictureCount / 30.0;
			AVFrame* picture = av_frame_alloc();
			av_frame_move_ref(picture, frame);
			pictures.emplace_back(picture, time);
			++pictureCount;
		}
	}

	void receiveSound()
	{
		while (avcodec_receive_frame(audio, frame) == 0)
		{
			const int most = swr_get_out_samples(resample, frame->nb_samples);
			const std::size_t had = pcm.size();
			pcm.resize(had + static_cast<std::size_t>(most) * 2);
			uint8_t* out = reinterpret_cast<uint8_t*>(pcm.data() + had);
			const int made = swr_convert(resample, &out, most, const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
			pcm.resize(had + static_cast<std::size_t>(std::max(made, 0)) * 2);
			av_frame_unref(frame);
		}
	}

	// One packet to its decoder; at the end, the decoders emptied.
	void readPacket()
	{
		if (av_read_frame(format, packet) < 0)
		{
			ended = true;
			if (video && avcodec_send_packet(video, nullptr) == 0)
				receivePictures();
			if (audio && avcodec_send_packet(audio, nullptr) == 0)
				receiveSound();
			return;
		}
		if (packet->stream_index == videoStream && avcodec_send_packet(video, packet) == 0)
			receivePictures();
		else if (packet->stream_index == audioStream && avcodec_send_packet(audio, packet) == 0)
			receiveSound();
		av_packet_unref(packet);
	}

	// The buffers heard to their end, free again.
	void reclaim()
	{
		ALint processed = 0;
		alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
		for (; processed > 0 && !queued.empty(); --processed)
		{
			ALuint buffer = 0;
			alSourceUnqueueBuffers(source, 1, &buffer);
			samplesHeard += queued.front().second;
			queued.pop_front();
			freeBuffers.push_back(buffer);
		}
	}

	// The sound decoded, a buffer at a time; what is left too, at the end.
	void queueSound()
	{
		while (pcm.size() >= kBufferSamples * 2 || (ended && !pcm.empty()))
		{
			const std::size_t samples = std::min<std::size_t>(pcm.size() / 2, kBufferSamples);
			ALuint buffer = 0;
			if (freeBuffers.empty())
				alGenBuffers(1, &buffer);
			else
			{
				buffer = freeBuffers.back();
				freeBuffers.pop_back();
			}
			alBufferData(buffer, AL_FORMAT_STEREO16, pcm.data(), static_cast<ALsizei>(samples * 2 * sizeof(int16_t)), rate);
			alSourceQueueBuffers(source, 1, &buffer);
			queued.emplace_back(buffer, static_cast<int64_t>(samples));
			samplesQueued += static_cast<int64_t>(samples);
			pcm.erase(pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(samples * 2));
		}
	}

	double heard() const
	{
		ALint offset = 0;
		alGetSourcei(source, AL_SAMPLE_OFFSET, &offset);
		return static_cast<double>(samplesHeard + offset) / rate;
	}
};

MoviePlayer::MoviePlayer() = default;
MoviePlayer::~MoviePlayer() = default;

bool MoviePlayer::IsAvailable()
{
	return true;
}

bool MoviePlayer::Open(const std::string& path, int track, float gain)
{
	Stop();
	av_log_set_level(AV_LOG_ERROR);
	auto decoder = std::make_unique<Decoder>();
	Decoder& d = *decoder;
	if (avformat_open_input(&d.format, path.c_str(), nullptr, nullptr) < 0)
	{
		Log::Info("MoviePlayer: cannot open {}", path);
		return false;
	}
	if (avformat_find_stream_info(d.format, nullptr) < 0)
		return false;
	// The picture, and the soundtrack asked for of the four.
	std::vector<int> soundtracks;
	for (unsigned i = 0; i < d.format->nb_streams; ++i)
	{
		const AVMediaType type = d.format->streams[i]->codecpar->codec_type;
		if (type == AVMEDIA_TYPE_VIDEO && d.videoStream < 0)
			d.videoStream = static_cast<int>(i);
		else if (type == AVMEDIA_TYPE_AUDIO)
			soundtracks.push_back(static_cast<int>(i));
	}
	if (!soundtracks.empty())
		d.audioStream = soundtracks[track >= 0 && track < static_cast<int>(soundtracks.size()) ? track : 0];
	for (unsigned i = 0; i < d.format->nb_streams; ++i)
		if (static_cast<int>(i) != d.videoStream && static_cast<int>(i) != d.audioStream)
			d.format->streams[i]->discard = AVDISCARD_ALL;
	if (d.videoStream < 0 || !(d.video = Decoder::openCodec(d.format->streams[d.videoStream])))
	{
		Log::Info("MoviePlayer: no picture in {}", path);
		return false;
	}
	d.scale = sws_getContext(d.video->width, d.video->height, d.video->pix_fmt, d.video->width, d.video->height,
	                         AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (!d.scale)
		return false;
	d.rgba.assign(static_cast<std::size_t>(d.video->width) * d.video->height * 4, 0);
	d.texture = std::make_unique<Texture>(d.rgba.data(), d.video->width, d.video->height, "movie");
	if (d.audioStream >= 0 && (d.audio = Decoder::openCodec(d.format->streams[d.audioStream])))
	{
		d.rate = d.audio->sample_rate;
		const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
		if (swr_alloc_set_opts2(&d.resample, &stereo, AV_SAMPLE_FMT_S16, d.rate, &d.audio->ch_layout, d.audio->sample_fmt,
		                        d.rate, 0, nullptr) < 0 ||
		    swr_init(d.resample) < 0)
			avcodec_free_context(&d.audio);
	}
	if (d.audio)
	{
		alGenSources(1, &d.source);
		alSourcef(d.source, AL_GAIN, gain);
		alSourcei(d.source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(d.source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	}
	else
		d.audioStream = -1;
	d.packet = av_packet_alloc();
	d.frame = av_frame_alloc();
	Log::Info("MoviePlayer: {} {}x{}, soundtrack {} of {} at {} Hz", path, d.video->width, d.video->height,
	          d.audio ? std::find(soundtracks.begin(), soundtracks.end(), d.audioStream) - soundtracks.begin() : -1,
	          soundtracks.size(), d.rate);
	_decoder = std::move(decoder);
	return true;
}

bool MoviePlayer::IsPlaying() const
{
	return _decoder && !_decoder->over;
}

double MoviePlayer::GetTime() const
{
	return _decoder ? _decoder->clock : 0.0;
}

void MoviePlayer::Update(double dt)
{
	if (!IsPlaying())
		return;
	Decoder& d = *_decoder;
	// The sound heard is the clock, while it plays; else the frames' time,
	// from when the sound starts.
	ALint state = AL_STOPPED;
	if (d.audio)
	{
		d.reclaim();
		alGetSourcei(d.source, AL_SOURCE_STATE, &state);
	}
	if (d.audio && d.started && state == AL_PLAYING)
		d.clock = d.heard();
	else if (!d.audio || d.started)
		d.clock += dt;
	// Read on until a picture to come waits and the sound is ahead enough.
	const auto ahead = [&d] {
		return !d.audio || static_cast<double>(d.samplesQueued + d.pcm.size() / 2) / d.rate > d.clock + kSoundAhead;
	};
	while (!d.ended && (d.pictures.empty() || d.pictures.back().second <= d.clock || !ahead()))
		d.readPacket();
	if (d.audio)
	{
		d.queueSound();
		// Started, or started again after running dry.
		if (state != AL_PLAYING && !d.queued.empty())
		{
			alSourcePlay(d.source);
			d.started = true;
		}
	}
	// The last picture whose time has come.
	AVFrame* shown = nullptr;
	while (!d.pictures.empty() && d.pictures.front().second <= d.clock)
	{
		av_frame_free(&shown);
		shown = d.pictures.front().first;
		d.pictures.pop_front();
	}
	if (shown)
	{
		uint8_t* planes[] = {d.rgba.data()};
		const int strides[] = {d.video->width * 4};
		sws_scale(d.scale, shown->data, shown->linesize, 0, d.video->height, planes, strides);
		d.texture->Update(d.rgba.data());
		av_frame_free(&shown);
	}
	d.over = d.ended && d.pictures.empty() && (!d.audio || (d.pcm.empty() && d.queued.empty()));
}

void MoviePlayer::Draw(const Matrix4x4& proj, int width, int height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (!_decoder || !_decoder->texture)
		return;
	const Texture& picture = *_decoder->texture;
	const float scale = std::min(static_cast<float>(width) / picture.GetWidth(), static_cast<float>(height) / picture.GetHeight());
	const Vector2 size(picture.GetWidth() * scale, picture.GetHeight() * scale);
	SpriteBatch batch;
	batch.Draw(_decoder->texture.get(), Vector2((width - size.X) * 0.5f, (height - size.Y) * 0.5f), size, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	batch.Flush(proj);
}

void MoviePlayer::Stop()
{
	_decoder.reset();
}

#else

struct MoviePlayer::Decoder
{
};

MoviePlayer::MoviePlayer() = default;
MoviePlayer::~MoviePlayer() = default;

bool MoviePlayer::IsAvailable()
{
	return false;
}

bool MoviePlayer::Open(const std::string& path, int, float)
{
	Log::Info("MoviePlayer: built without FFmpeg, {} not played", path);
	return false;
}

bool MoviePlayer::IsPlaying() const
{
	return false;
}

double MoviePlayer::GetTime() const
{
	return 0.0;
}

void MoviePlayer::Update(double) {}

void MoviePlayer::Draw(const Matrix4x4&, int, int) {}

void MoviePlayer::Stop() {}

#endif

} // namespace Donut
