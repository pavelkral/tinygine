#pragma once

#include "pch/Pch.h"

struct MipData {
    std::vector<uint32_t> pixels;
    int width;
    int height;
};

float ToLinear(uint8_t v);
uint8_t ToGamma(float f);
std::vector<MipData> GenerateMipChain(unsigned char* srcData, int w, int h);
