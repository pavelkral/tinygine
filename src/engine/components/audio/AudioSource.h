#pragma once

#include "pch/Pch.h"

class AudioSource : public Component {
public:
	ma_engine* audioEngine = nullptr;
	ma_sound sound;
	std::string filePath;

	bool isLoaded = false;
	bool playOnAwake = true;
	bool isLooping = false;
	bool isSpatial3D = true;
	float volume = 1.0f;
	float minDistance = 2.0f;
	float maxDistance = 50.0f;
	AudioSource(ma_engine* engine = nullptr, const std::string& path = "");
	json Serialize() override;
	void Deserialize(const json& j) override;
	void Start() override;
	void LoadSound();
	void ApplySettings();
	void Play();
	void Stop();
	void Update(float dt) override;
	void OnGUI() override;
	~AudioSource() override;
};
