#pragma once

#include <assimp/scene.h>
#include <string>

void DebugPrintAssimpScene(const aiScene* scene, const std::string& filepath);
