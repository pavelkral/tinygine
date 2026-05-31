#include "engine/EngineDependencies.h"
#include "engine/components/audio/AudioSource.h"

AudioSource::AudioSource(ma_engine* engine, const std::string& path) : audioEngine(engine), filePath(path) {
	componentType = "AudioSource";
}


json AudioSource::Serialize() {
	json j = Component::Serialize();
	j["filePath"] = filePath;
	j["playOnAwake"] = playOnAwake;
	j["isLooping"] = isLooping;
	j["isSpatial3D"] = isSpatial3D;
	j["volume"] = volume;
	j["minDistance"] = minDistance;
	j["maxDistance"] = maxDistance;
	return j;
}


void AudioSource::Deserialize(const json& j) {
	filePath = j.value("filePath", "");
	playOnAwake = j.value("playOnAwake", true);
	isLooping = j.value("isLooping", false);
	isSpatial3D = j.value("isSpatial3D", true);
	volume = j.value("volume", 1.0f);
	minDistance = j.value("minDistance", 2.0f);
	maxDistance = j.value("maxDistance", 50.0f);
}


void AudioSource::Start() {
	LoadSound();
}


void AudioSource::LoadSound() {
	if (!audioEngine || filePath.empty()) return;

	if (isLoaded) ma_sound_uninit(&sound);

	ma_result result = ma_sound_init_from_file(audioEngine, filePath.c_str(), 0, NULL, NULL, &sound);
	if (result == MA_SUCCESS) {
		isLoaded = true;
		ApplySettings();
		if (playOnAwake) Play();
	}
}


void AudioSource::ApplySettings() {
	if (!isLoaded) return;
	ma_sound_set_volume(&sound, volume);
	ma_sound_set_looping(&sound, isLooping ? MA_TRUE : MA_FALSE);

	ma_sound_set_spatialization_enabled(&sound, isSpatial3D ? MA_TRUE : MA_FALSE);
	ma_sound_set_min_distance(&sound, minDistance);
	ma_sound_set_max_distance(&sound, maxDistance);
	ma_sound_set_attenuation_model(&sound, ma_attenuation_model_linear);
}


void AudioSource::Play() { if (isLoaded) ma_sound_start(&sound); }


void AudioSource::Stop() { if (isLoaded) ma_sound_stop(&sound); }


void AudioSource::Update(float dt) {
	if (!isLoaded || !isSpatial3D) return;

	ma_sound_set_position(&sound,
		static_cast<float>(gameObject->transform.position.x),
		static_cast<float>(gameObject->transform.position.y),
		static_cast<float>(gameObject->transform.position.z)
	);
}


void AudioSource::OnGUI() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Audio Source");
	ImGui::SameLine();
	if (ImGui::Button("Remove##as")) Destroy();

	ImGui::Text("File: %s", filePath.empty() ? "None" : filePath.c_str());

	bool settingsChanged = false;

	if (ImGui::SliderFloat("Volume", &volume, 0.0f, 2.0f)) settingsChanged = true;
	if (ImGui::Checkbox("Looping", &isLooping)) settingsChanged = true;
	if (ImGui::Checkbox("3D Spatial Sound", &isSpatial3D)) settingsChanged = true;

	if (isSpatial3D) {
		if (ImGui::SliderFloat("Min Distance", &minDistance, 0.1f, 20.0f)) settingsChanged = true;
		if (ImGui::SliderFloat("Max Distance", &maxDistance, 5.0f, 200.0f)) settingsChanged = true;
	}

	if (settingsChanged) ApplySettings();

	if (ImGui::Button("Play##as", ImVec2(80, 0))) Play();
	ImGui::SameLine();
	if (ImGui::Button("Stop##as", ImVec2(80, 0))) Stop();
}


AudioSource::~AudioSource() {
	if (isLoaded) {
		ma_sound_stop(&sound);
		ma_sound_uninit(&sound);
	}
}
