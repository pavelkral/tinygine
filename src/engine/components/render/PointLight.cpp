#include "engine/EngineDependencies.h"
#include "engine/components/render/PointLight.h"

PointLight::PointLight() { componentType = "PointLight"; }


json PointLight::Serialize() {
        json j = Component::Serialize();
        j["color"] = { color.x, color.y, color.z };
        j["intensity"] = intensity;
        j["radius"] = radius;
        return j;
    }


void PointLight::Deserialize(const json& j) {
        if (j.contains("color")) color = { j["color"][0], j["color"][1], j["color"][2] };
        intensity = j.value("intensity", 10.0f);
        radius = j.value("radius", 10.0f);
    }


void PointLight::OnGUI() {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "Point Light");
        ImGui::SameLine();
        if (ImGui::Button("Remove##pl")) Destroy();

        ImGui::ColorEdit3("Color", &color.x);
        ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f);
        ImGui::SliderFloat("Radius", &radius, 1.0f, 100.0f);
    }
