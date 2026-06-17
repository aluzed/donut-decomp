// Copyright 2019-2020 the donut authors. See AUTHORS.md

#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector3.h"

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace Donut
{

class SkinAnimation
{
public:
	template <class T>
	class ValueKey
	{
	public:
		ValueKey(float time, const T& value): _time(time), _value(value) {}

		virtual T Lerp(const T& b, float time) = 0;

		float GetTime() const { return _time; }
		const T& GetValue() const { return _value; }

	protected:
		float _time;
		T _value;
	};

	class TranslationKey: public ValueKey<Vector3>
	{
	public:
		TranslationKey(float time, const Vector3& value): ValueKey(time, value) {}

		virtual Vector3 Lerp(const Vector3& b, float time) override
		{
			return _value + (b - _value) * time;
		}
	};

	class RotationKey: public ValueKey<Quaternion>
	{
	public:
		RotationKey(float time, const Quaternion& value): ValueKey(time, value) {}

		virtual Quaternion Lerp(const Quaternion& b, float time) override
		{
			float cosTheta = _value.X * b.X + _value.Y * b.Y + _value.Z * b.Z + _value.W * b.W;
			float t = time;

			Quaternion other = b;
			if (cosTheta < 0.0f)
			{
				cosTheta = -cosTheta;
				other = Quaternion(-b.X, -b.Y, -b.Z, -b.W);
				t = -t;
			}

			if (cosTheta > 0.9995f)
				return Quaternion(
					_value.X + (other.X - _value.X) * time,
					_value.Y + (other.Y - _value.Y) * time,
					_value.Z + (other.Z - _value.Z) * time,
					_value.W + (other.W - _value.W) * time).Normal();

			float theta = acosf(cosTheta);
			float sinTheta = sinf(theta);
			float wA = sinf((1.0f - time) * theta) / sinTheta;
			float wB = sinf(time * theta) / sinTheta;

			return Quaternion(
				wA * _value.X + wB * other.X,
				wA * _value.Y + wB * other.Y,
				wA * _value.Z + wB * other.Z,
				wA * _value.W + wB * other.W);
		}
	};

	template <class T>
	class ValueKeyCurve
	{
	public:
		void AddTranslationKey(float time, const Vector3& value)
		{
			_keyValues.push_back(std::make_unique<TranslationKey>(time, value));
		}
		void AddRotationKey(float time, const Quaternion& value)
		{
			_keyValues.push_back(std::make_unique<RotationKey>(time, value));
		}

		T Evalulate(float time, T defaultValue)
		{
			auto count = _keyValues.size();
			if (count == 0)
				return defaultValue;

			auto lastIndex = _keyValues.size() - 1;
			time = glm::mod(time, _keyValues[lastIndex]->GetTime());

			auto index = GetKeyValueIndex(time);
			auto nextIndex = (index == lastIndex) ? 0 : index + 1;

			const auto& prevPoint = _keyValues[index];
			const auto& nextPoint = _keyValues[nextIndex];
			auto delta = nextPoint->GetTime() - prevPoint->GetTime();

			if (delta > 0.0f)
			{
				auto fraction = (time - prevPoint->GetTime()) / delta;
				fraction = Math::Clamp(fraction, 0.0f, 1.0f);

				return prevPoint->Lerp(nextPoint->GetValue(), fraction);
			}
			else
			{
				return prevPoint->GetValue();
			}
		}

	private:
		std::vector<std::unique_ptr<ValueKey<T>>> _keyValues;

		size_t GetKeyValueIndex(float time)
		{
			size_t count = _keyValues.size();
			if (count == 0)
				return 0;

			size_t lastIndex = count - 1;

			if (time < _keyValues.at(0)->GetTime())
			{
				return 0;
			}

			if (time >= _keyValues.at(lastIndex)->GetTime())
			{
				return lastIndex;
			}

			size_t minIndex = 0;
			size_t maxIndex = count;

			while (maxIndex - minIndex > 1)
			{
				auto midIndex = (minIndex + maxIndex) / 2;

				if (_keyValues.at(midIndex)->GetTime() <= time)
				{
					minIndex = midIndex;
				}
				else
				{
					maxIndex = midIndex;
				}
			}

			return minIndex;
		}
	};

	class Track
	{
	public:
		Track(std::string name): _name(name) {}

		Matrix4x4 Evaluate(float time);

		void AddTranslationKey(float time, const Vector3& value) { _translationKeys.AddTranslationKey(time, value); }
		void AddRotationKey(float time, const Quaternion& value) { _rotationKeys.AddRotationKey(time, value); }

	private:
		std::string _name;
		ValueKeyCurve<Vector3> _translationKeys;
		ValueKeyCurve<Quaternion> _rotationKeys;
	};

	SkinAnimation(std::string name, float length, int32_t frameCount, float frameRate)
	    : _name(name), _length(length), _frameCount(frameCount), _frameRate(frameRate)
	{
	}

	Matrix4x4 Evaluate(size_t trackIndex, float time);

	void AddTrack(std::unique_ptr<Track>& track) { _tracks.push_back(std::move(track)); }
	size_t GetNumTracks() const { return _tracks.size(); }
	const std::string& GetName() const { return _name; }
	float GetFrameRate() const { return _frameRate; }

private:
	std::string _name;
	float _length;
	int32_t _frameCount;
	float _frameRate;
	std::vector<std::unique_ptr<Track>> _tracks;
};

} // namespace Donut
