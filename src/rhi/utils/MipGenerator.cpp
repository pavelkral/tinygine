#include "rhi/utils/MipGenerator.h"

float ToLinear(uint8_t v) { float f = v / 255.0f; return (f <= 0.04045f) ? f / 12.92f : powf((f + 0.055f) / 1.055f, 2.4f); }
uint8_t ToGamma(float f) { f = (f <= 0.0031308f) ? f * 12.92f : 1.055f * powf(f, 1.0f / 2.4f) - 0.055f; return (uint8_t)(std::clamp(f * 255.0f, 0.0f, 255.0f)); }

std::vector<MipData> GenerateMipChain(unsigned char* srcData, int w, int h) {
    std::vector<MipData> mips; MipData m0; m0.width = w; m0.height = h; m0.pixels.resize(w * h); memcpy(m0.pixels.data(), srcData, w * h * 4); mips.push_back(m0);
    int currW = w, currH = h; std::vector<uint32_t>* prevPixels = &mips[0].pixels;
    while (currW > 1 || currH > 1) {
        int nextW = std::max(1, currW / 2); int nextH = std::max(1, currH / 2); MipData nextMip; nextMip.width = nextW; nextMip.height = nextH; nextMip.pixels.resize(nextW * nextH);
        for (int y = 0; y < nextH; ++y) {
            for (int x = 0; x < nextW; ++x) {
                int x0 = x * 2; int x1 = std::min(x * 2 + 1, currW - 1); int y0 = y * 2; int y1 = std::min(y * 2 + 1, currH - 1);
                uint32_t p[4] = { (*prevPixels)[y0 * currW + x0], (*prevPixels)[y0 * currW + x1], (*prevPixels)[y1 * currW + x0], (*prevPixels)[y1 * currW + x1] };
                float r = 0, g = 0, b = 0, a = 0;
                for (int i = 0; i < 4; i++) { r += ToLinear(p[i] & 0xFF); g += ToLinear((p[i] >> 8) & 0xFF); b += ToLinear((p[i] >> 16) & 0xFF); a += ((p[i] >> 24) & 0xFF) / 255.0f; }
                nextMip.pixels[y * nextW + x] = ToGamma(r / 4.0f) | (ToGamma(g / 4.0f) << 8) | (ToGamma(b / 4.0f) << 16) | ((uint8_t)((a / 4.0f) * 255.0f) << 24);
            }
        }
        mips.push_back(nextMip); currW = nextW; currH = nextH; prevPixels = &mips.back().pixels;
    }
    return mips;
}
