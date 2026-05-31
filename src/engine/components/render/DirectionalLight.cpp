#include "engine/EngineDependencies.h"
#include "engine/components/render/DirectionalLight.h"

DirectionalLight::DirectionalLight() { componentType = "DirectionalLight"; }


json DirectionalLight::Serialize() {
        json j = Component::Serialize();
        j["color"] = { color.x, color.y, color.z };
        j["intensity"] = intensity;
        j["castShadows"] = castShadows;
        return j;
    }


void DirectionalLight::Deserialize(const json& j) {
        if (j.contains("color")) color = { j["color"][0], j["color"][1], j["color"][2] };
        intensity = j.value("intensity", 5.0f);
        castShadows = j.value("castShadows", true);
    }


void DirectionalLight::OnGUI() {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "Directional Light");
        ImGui::SameLine();
        if (ImGui::Button("Remove##dl")) Destroy();

        ImGui::ColorEdit3("Color", &color.x);
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 20.0f);
        ImGui::Checkbox("Cast Shadows", &castShadows);
    }
