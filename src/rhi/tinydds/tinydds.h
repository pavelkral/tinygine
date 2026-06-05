#pragma once

// /////////////////////////////////////////////////////////////////////////////
// 
// tinydds.h
// Header-only mini library for loading DDS textures (including cubemaps)
// and preparing backend-agnostic / DX12 / Vulkan upload metadata.

// - Parse modern DDS (DX10 header) + legacy DDS cubemap flags
// - Support 2D, arrays, cubemaps, 3D volume textures
// - Expose subresource iteration and row/slice pitch helpers
// - Optional helpers for DirectX 12 / Vulkan backends
//
// Notes:
// - This library intentionally focuses on DDS because it is the most natural
//   container for DX-style BC compressed textures and classic cubemap workflows.
// - It does not perform transcoding or decompression. It returns the original
//   byte payload and precise layout metadata for GPU upload.
// - For best results, use DDS files that already contain GPU-ready mipmaps in
//   the exact target format (BC1..BC7, RGBA8, RGBA16F, etc.).
//

// -----------------------------------------------------------------------------
// Suggested usage notes
// -----------------------------------------------------------------------------

/*
Suggested usage in an engine:

1) Load asset:
   auto tex = mt::Texture::from_file("albedo.dds");
   if (!tex.ok()) { ... }

2) Create GPU image/resource from tex.desc().

3) Upload each subresource:
   - DX12: iterate mt::build_d3d12_subresources(tex) or tex.subresources()
   - Vulkan: use mt::build_vk_buffer_image_copies(tex)

4) Create SRV / image view using helpers:
   - DX12: mt::make_d3d12_srv_desc(tex.desc())
   - Vulkan: mt::make_vk_image_view_create_info(image, tex.desc())

Cubemap face order used by DDS payload is preserved as physical 2D array faces:
  0 +X
  1 -X
  2 +Y
  3 -Y
  4 +Z
  5 -Z

The library stores cubemaps as:
  desc.is_cube = true
  desc.array_size = number of cubes
  physical layers = array_size * 6


The library does not perform any transcoding or decompression. It returns the
original byte payload and precise layout metadata for GPU upload.

*/

// Example:
//   #include "tinydds.h"
//   using namespace mt;
//
//   auto tex = Texture::from_file("skybox.dds");
//   if (!tex.ok()) {
//       std::fprintf(stderr, "%s\n", tex.error_message().c_str());
//       return;
//   }
//
//   for (const auto& sr : tex.subresources()) {
//       // sr.data points into tex.owned_bytes()
//       // upload using row_pitch / slice_pitch / width / height / depth
//   }
//
//   // Backend helper indexing:
//   auto idx = calc_subresource_index(/*mip=*/2, /*layer=*/0, /*face=*/4,
//                                     tex.desc().mip_count,
//                                     tex.desc().is_cube);
//
// /////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mt {

// -----------------------------------------------------------------------------
// Public enums and flags
// -----------------------------------------------------------------------------

enum class TextureDimension {
    tex1d,
    tex2d,
    tex3d,
    cube,
};

enum class TextureFormat {
    unknown = 0,

    r8_unorm,
    rg8_unorm,
    rgba8_unorm,
    rgba8_srgb,
    bgra8_unorm,
    bgra8_srgb,

    r16_float,
    rg16_float,
    rgba16_float,
    r32_float,
    rg32_float,
    rgba32_float,

    bc1_unorm,
    bc1_srgb,
    bc2_unorm,
    bc2_srgb,
    bc3_unorm,
    bc3_srgb,
    bc4_unorm,
    bc4_snorm,
    bc5_unorm,
    bc5_snorm,
    bc6h_ufloat,
    bc6h_sfloat,
    bc7_unorm,
    bc7_srgb,

    d32_float,
    d24_unorm_s8_uint,
};

enum class TextureUsage : std::uint32_t {
    none          = 0,
    sampled       = 1u << 0,
    storage       = 1u << 1,
    render_target = 1u << 2,
    depth_stencil = 1u << 3,
    transfer_src  = 1u << 4,
    transfer_dst  = 1u << 5,
};

inline constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
inline constexpr bool has_flag(TextureUsage value, TextureUsage flag) noexcept {
    return (value & flag) != TextureUsage::none;
}

// -----------------------------------------------------------------------------
// Descriptors
// -----------------------------------------------------------------------------

struct TextureDesc {
    TextureDimension dimension = TextureDimension::tex2d;
    TextureFormat format = TextureFormat::unknown;

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;

    std::uint32_t mip_count = 1;
    std::uint32_t array_size = 1;      // For cube arrays: number of cubes, not faces.
    bool is_cube = false;
    bool is_srgb = false;

    TextureUsage intended_usage = TextureUsage::sampled;
};

struct Subresource {
    const std::byte* data = nullptr;
    std::size_t data_size = 0;

    std::uint32_t mip_level = 0;
    std::uint32_t array_layer = 0; // cube index when is_cube=true
    std::uint32_t face = 0;        // 0..5 for cubemaps, else 0

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;

    std::uint32_t row_pitch = 0;
    std::uint32_t slice_pitch = 0;
};

struct Result {
    bool success = false;
    std::string error;

    static Result ok() { return {true, {}}; }
    static Result fail(std::string msg) { return {false, std::move(msg)}; }
};

// -----------------------------------------------------------------------------
// Internal DDS definitions
// -----------------------------------------------------------------------------

namespace detail {

constexpr std::uint32_t make_fourcc(char a, char b, char c, char d) noexcept {
    return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a))      ) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) <<  8) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(d)) << 24);
}

constexpr std::uint32_t DDS_MAGIC = make_fourcc('D', 'D', 'S', ' ');
constexpr std::uint32_t FOURCC_DX10 = make_fourcc('D', 'X', '1', '0');

#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t four_cc;
    std::uint32_t rgb_bit_count;
    std::uint32_t r_bit_mask;
    std::uint32_t g_bit_mask;
    std::uint32_t b_bit_mask;
    std::uint32_t a_bit_mask;
};

struct DDS_HEADER {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitch_or_linear_size;
    std::uint32_t depth;
    std::uint32_t mip_map_count;
    std::uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    std::uint32_t caps;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};

struct DDS_HEADER_DXT10 {
    std::uint32_t dxgi_format;
    std::uint32_t resource_dimension;
    std::uint32_t misc_flag;
    std::uint32_t array_size;
    std::uint32_t misc_flags2;
};
#pragma pack(pop)

static_assert(sizeof(DDS_PIXELFORMAT) == 32, "DDS_PIXELFORMAT must be 32 bytes");
static_assert(sizeof(DDS_HEADER) == 124, "DDS_HEADER must be 124 bytes");
static_assert(sizeof(DDS_HEADER_DXT10) == 20, "DDS_HEADER_DXT10 must be 20 bytes");

// DDS_PIXELFORMAT flags
constexpr std::uint32_t DDPF_ALPHAPIXELS = 0x00000001u;
constexpr std::uint32_t DDPF_ALPHA       = 0x00000002u;
constexpr std::uint32_t DDPF_FOURCC      = 0x00000004u;
constexpr std::uint32_t DDPF_RGB         = 0x00000040u;
constexpr std::uint32_t DDPF_YUV         = 0x00000200u;
constexpr std::uint32_t DDPF_LUMINANCE   = 0x00020000u;

// DDS_HEADER caps2 cubemap flags
constexpr std::uint32_t DDSCAPS2_CUBEMAP           = 0x00000200u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000u;
constexpr std::uint32_t DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000u;
constexpr std::uint32_t DDSCAPS2_VOLUME            = 0x00200000u;

constexpr std::uint32_t DDS_RESOURCE_MISC_TEXTURECUBE = 0x4u;

// DXGI_FORMAT subset used by this mini library
constexpr std::uint32_t DXGI_FORMAT_UNKNOWN            = 0;
constexpr std::uint32_t DXGI_FORMAT_R8_UNORM           = 61;
constexpr std::uint32_t DXGI_FORMAT_R16_FLOAT          = 54;
constexpr std::uint32_t DXGI_FORMAT_R32_FLOAT          = 41;
constexpr std::uint32_t DXGI_FORMAT_R8G8_UNORM         = 49;
constexpr std::uint32_t DXGI_FORMAT_R16G16_FLOAT       = 34;
constexpr std::uint32_t DXGI_FORMAT_R32G32_FLOAT       = 16;
constexpr std::uint32_t DXGI_FORMAT_R8G8B8A8_UNORM     = 28;
constexpr std::uint32_t DXGI_FORMAT_R8G8B8A8_UNORM_SRGB= 29;
constexpr std::uint32_t DXGI_FORMAT_B8G8R8A8_UNORM     = 87;
constexpr std::uint32_t DXGI_FORMAT_B8G8R8A8_UNORM_SRGB= 91;
constexpr std::uint32_t DXGI_FORMAT_R16G16B16A16_FLOAT = 10;
constexpr std::uint32_t DXGI_FORMAT_R32G32B32A32_FLOAT = 2;
constexpr std::uint32_t DXGI_FORMAT_D32_FLOAT          = 40;
constexpr std::uint32_t DXGI_FORMAT_D24_UNORM_S8_UINT  = 45;
constexpr std::uint32_t DXGI_FORMAT_BC1_UNORM          = 71;
constexpr std::uint32_t DXGI_FORMAT_BC1_UNORM_SRGB     = 72;
constexpr std::uint32_t DXGI_FORMAT_BC2_UNORM          = 74;
constexpr std::uint32_t DXGI_FORMAT_BC2_UNORM_SRGB     = 75;
constexpr std::uint32_t DXGI_FORMAT_BC3_UNORM          = 77;
constexpr std::uint32_t DXGI_FORMAT_BC3_UNORM_SRGB     = 78;
constexpr std::uint32_t DXGI_FORMAT_BC4_UNORM          = 80;
constexpr std::uint32_t DXGI_FORMAT_BC4_SNORM          = 81;
constexpr std::uint32_t DXGI_FORMAT_BC5_UNORM          = 83;
constexpr std::uint32_t DXGI_FORMAT_BC5_SNORM          = 84;
constexpr std::uint32_t DXGI_FORMAT_BC6H_UF16          = 95;
constexpr std::uint32_t DXGI_FORMAT_BC6H_SF16          = 96;
constexpr std::uint32_t DXGI_FORMAT_BC7_UNORM          = 98;
constexpr std::uint32_t DXGI_FORMAT_BC7_UNORM_SRGB     = 99;

constexpr std::uint32_t DDS_DIMENSION_TEXTURE1D = 2;
constexpr std::uint32_t DDS_DIMENSION_TEXTURE2D = 3;
constexpr std::uint32_t DDS_DIMENSION_TEXTURE3D = 4;

inline constexpr bool is_block_compressed(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::bc1_unorm:
        case TextureFormat::bc1_srgb:
        case TextureFormat::bc2_unorm:
        case TextureFormat::bc2_srgb:
        case TextureFormat::bc3_unorm:
        case TextureFormat::bc3_srgb:
        case TextureFormat::bc4_unorm:
        case TextureFormat::bc4_snorm:
        case TextureFormat::bc5_unorm:
        case TextureFormat::bc5_snorm:
        case TextureFormat::bc6h_ufloat:
        case TextureFormat::bc6h_sfloat:
        case TextureFormat::bc7_unorm:
        case TextureFormat::bc7_srgb:
            return true;
        default:
            return false;
    }
}

inline constexpr std::uint32_t bytes_per_block(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::bc1_unorm:
        case TextureFormat::bc1_srgb:
        case TextureFormat::bc4_unorm:
        case TextureFormat::bc4_snorm:
            return 8;

        case TextureFormat::bc2_unorm:
        case TextureFormat::bc2_srgb:
        case TextureFormat::bc3_unorm:
        case TextureFormat::bc3_srgb:
        case TextureFormat::bc5_unorm:
        case TextureFormat::bc5_snorm:
        case TextureFormat::bc6h_ufloat:
        case TextureFormat::bc6h_sfloat:
        case TextureFormat::bc7_unorm:
        case TextureFormat::bc7_srgb:
            return 16;
        default:
            return 0;
    }
}

inline constexpr std::uint32_t bytes_per_pixel(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::r8_unorm:           return 1;
        case TextureFormat::rg8_unorm:          return 2;
        case TextureFormat::rgba8_unorm:        return 4;
        case TextureFormat::rgba8_srgb:         return 4;
        case TextureFormat::bgra8_unorm:        return 4;
        case TextureFormat::bgra8_srgb:         return 4;
        case TextureFormat::r16_float:          return 2;
        case TextureFormat::rg16_float:         return 4;
        case TextureFormat::rgba16_float:       return 8;
        case TextureFormat::r32_float:          return 4;
        case TextureFormat::rg32_float:         return 8;
        case TextureFormat::rgba32_float:       return 16;
        case TextureFormat::d32_float:          return 4;
        case TextureFormat::d24_unorm_s8_uint:  return 4;
        default:                                return 0;
    }
}

inline constexpr TextureFormat dxgi_to_texture_format(std::uint32_t dxgi) noexcept {
    switch (dxgi) {
        case DXGI_FORMAT_R8_UNORM:            return TextureFormat::r8_unorm;
        case DXGI_FORMAT_R8G8_UNORM:          return TextureFormat::rg8_unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM:      return TextureFormat::rgba8_unorm;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return TextureFormat::rgba8_srgb;
        case DXGI_FORMAT_B8G8R8A8_UNORM:      return TextureFormat::bgra8_unorm;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return TextureFormat::bgra8_srgb;
        case DXGI_FORMAT_R16_FLOAT:           return TextureFormat::r16_float;
        case DXGI_FORMAT_R16G16_FLOAT:        return TextureFormat::rg16_float;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:  return TextureFormat::rgba16_float;
        case DXGI_FORMAT_R32_FLOAT:           return TextureFormat::r32_float;
        case DXGI_FORMAT_R32G32_FLOAT:        return TextureFormat::rg32_float;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:  return TextureFormat::rgba32_float;
        case DXGI_FORMAT_D32_FLOAT:           return TextureFormat::d32_float;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:   return TextureFormat::d24_unorm_s8_uint;
        case DXGI_FORMAT_BC1_UNORM:           return TextureFormat::bc1_unorm;
        case DXGI_FORMAT_BC1_UNORM_SRGB:      return TextureFormat::bc1_srgb;
        case DXGI_FORMAT_BC2_UNORM:           return TextureFormat::bc2_unorm;
        case DXGI_FORMAT_BC2_UNORM_SRGB:      return TextureFormat::bc2_srgb;
        case DXGI_FORMAT_BC3_UNORM:           return TextureFormat::bc3_unorm;
        case DXGI_FORMAT_BC3_UNORM_SRGB:      return TextureFormat::bc3_srgb;
        case DXGI_FORMAT_BC4_UNORM:           return TextureFormat::bc4_unorm;
        case DXGI_FORMAT_BC4_SNORM:           return TextureFormat::bc4_snorm;
        case DXGI_FORMAT_BC5_UNORM:           return TextureFormat::bc5_unorm;
        case DXGI_FORMAT_BC5_SNORM:           return TextureFormat::bc5_snorm;
        case DXGI_FORMAT_BC6H_UF16:           return TextureFormat::bc6h_ufloat;
        case DXGI_FORMAT_BC6H_SF16:           return TextureFormat::bc6h_sfloat;
        case DXGI_FORMAT_BC7_UNORM:           return TextureFormat::bc7_unorm;
        case DXGI_FORMAT_BC7_UNORM_SRGB:      return TextureFormat::bc7_srgb;
        default:                              return TextureFormat::unknown;
    }
}

inline constexpr bool texture_format_is_srgb(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::rgba8_srgb:
        case TextureFormat::bgra8_srgb:
        case TextureFormat::bc1_srgb:
        case TextureFormat::bc2_srgb:
        case TextureFormat::bc3_srgb:
        case TextureFormat::bc7_srgb:
            return true;
        default:
            return false;
    }
}

inline constexpr TextureFormat legacy_dds_pf_to_texture_format(const DDS_PIXELFORMAT& pf) noexcept {
    if (pf.flags & DDPF_FOURCC) {
        switch (pf.four_cc) {
        case make_fourcc('D', 'X', 'T', '1'): return TextureFormat::bc1_unorm;
        case make_fourcc('D', 'X', 'T', '3'): return TextureFormat::bc2_unorm;
        case make_fourcc('D', 'X', 'T', '5'): return TextureFormat::bc3_unorm;
        case make_fourcc('A', 'T', 'I', '1'):
        case make_fourcc('B', 'C', '4', 'U'): return TextureFormat::bc4_unorm;
        case make_fourcc('B', 'C', '4', 'S'): return TextureFormat::bc4_snorm;
        case make_fourcc('A', 'T', 'I', '2'):
        case make_fourcc('B', 'C', '5', 'U'): return TextureFormat::bc5_unorm;
        case make_fourcc('B', 'C', '5', 'S'): return TextureFormat::bc5_snorm;

            // --- PØIDÁNO: Podpora pro staré IBL / HDR cubemapy! ---
        case 111: return TextureFormat::r16_float;
        case 112: return TextureFormat::rg16_float;
        case 113: return TextureFormat::rgba16_float;
        case 114: return TextureFormat::r32_float;
        case 115: return TextureFormat::rg32_float;
        case 116: return TextureFormat::rgba32_float;
            // ------------------------------------------------------

        default: return TextureFormat::unknown;
        }
    }

    if ((pf.flags & DDPF_RGB) && pf.rgb_bit_count == 32) {
        if (pf.r_bit_mask == 0x000000ffu && pf.g_bit_mask == 0x0000ff00u &&
            pf.b_bit_mask == 0x00ff0000u && pf.a_bit_mask == 0xff000000u) {
            return TextureFormat::rgba8_unorm;
        }
        if (pf.r_bit_mask == 0x00ff0000u && pf.g_bit_mask == 0x0000ff00u &&
            pf.b_bit_mask == 0x000000ffu && pf.a_bit_mask == 0xff000000u) {
            return TextureFormat::bgra8_unorm;
        }
    }

    if ((pf.flags & DDPF_LUMINANCE) && pf.rgb_bit_count == 8 && pf.r_bit_mask == 0x000000ffu) {
        return TextureFormat::r8_unorm;
    }

    return TextureFormat::unknown;
}

inline constexpr std::uint32_t mip_extent(std::uint32_t v, std::uint32_t mip) noexcept {
    const std::uint32_t shr = (mip >= 31u) ? 31u : mip;
    const std::uint32_t r = (v >> shr);
    return r ? r : 1u;
}

inline constexpr std::uint64_t checked_mul_u64(std::uint64_t a, std::uint64_t b) {
    if (a == 0 || b == 0) return 0;
    if (a > (std::numeric_limits<std::uint64_t>::max() / b)) {
        throw std::overflow_error("integer overflow while computing texture layout");
    }
    return a * b;
}

inline constexpr std::pair<std::uint32_t, std::uint32_t> compute_row_and_slice_pitch(
    TextureFormat format,
    std::uint32_t width,
    std::uint32_t height) {

    if (is_block_compressed(format)) {
        const std::uint32_t block_w = (width + 3u) / 4u;
        const std::uint32_t block_h = (height + 3u) / 4u;
        const std::uint32_t bpb = bytes_per_block(format);
        return {block_w * bpb, block_w * block_h * bpb};
    }

    const std::uint32_t bpp = bytes_per_pixel(format);
    return {width * bpp, width * height * bpp};
}

inline constexpr const char* format_name(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::unknown: return "unknown";
        case TextureFormat::r8_unorm: return "r8_unorm";
        case TextureFormat::rg8_unorm: return "rg8_unorm";
        case TextureFormat::rgba8_unorm: return "rgba8_unorm";
        case TextureFormat::rgba8_srgb: return "rgba8_srgb";
        case TextureFormat::bgra8_unorm: return "bgra8_unorm";
        case TextureFormat::bgra8_srgb: return "bgra8_srgb";
        case TextureFormat::r16_float: return "r16_float";
        case TextureFormat::rg16_float: return "rg16_float";
        case TextureFormat::rgba16_float: return "rgba16_float";
        case TextureFormat::r32_float: return "r32_float";
        case TextureFormat::rg32_float: return "rg32_float";
        case TextureFormat::rgba32_float: return "rgba32_float";
        case TextureFormat::bc1_unorm: return "bc1_unorm";
        case TextureFormat::bc1_srgb: return "bc1_srgb";
        case TextureFormat::bc2_unorm: return "bc2_unorm";
        case TextureFormat::bc2_srgb: return "bc2_srgb";
        case TextureFormat::bc3_unorm: return "bc3_unorm";
        case TextureFormat::bc3_srgb: return "bc3_srgb";
        case TextureFormat::bc4_unorm: return "bc4_unorm";
        case TextureFormat::bc4_snorm: return "bc4_snorm";
        case TextureFormat::bc5_unorm: return "bc5_unorm";
        case TextureFormat::bc5_snorm: return "bc5_snorm";
        case TextureFormat::bc6h_ufloat: return "bc6h_ufloat";
        case TextureFormat::bc6h_sfloat: return "bc6h_sfloat";
        case TextureFormat::bc7_unorm: return "bc7_unorm";
        case TextureFormat::bc7_srgb: return "bc7_srgb";
        case TextureFormat::d32_float: return "d32_float";
        case TextureFormat::d24_unorm_s8_uint: return "d24_unorm_s8_uint";
        default: return "unsupported";
    }
}

} // namespace detail

// -----------------------------------------------------------------------------
// Public helpers
// -----------------------------------------------------------------------------

inline constexpr bool is_block_compressed(TextureFormat format) noexcept {
    return detail::is_block_compressed(format);
}

inline constexpr std::uint32_t bytes_per_pixel(TextureFormat format) noexcept {
    return detail::bytes_per_pixel(format);
}

inline constexpr std::uint32_t bytes_per_block(TextureFormat format) noexcept {
    return detail::bytes_per_block(format);
}

inline constexpr const char* to_string(TextureFormat format) noexcept {
    return detail::format_name(format);
}

inline constexpr std::uint32_t calc_physical_layer(std::uint32_t array_layer, std::uint32_t face, bool is_cube) noexcept {
    return is_cube ? (array_layer * 6u + face) : array_layer;
}

inline constexpr std::uint32_t calc_subresource_index(
    std::uint32_t mip,
    std::uint32_t array_layer,
    std::uint32_t face,
    std::uint32_t mip_count,
    bool is_cube) noexcept {
    return calc_physical_layer(array_layer, face, is_cube) * mip_count + mip;
}

inline constexpr std::uint32_t physical_layer_count(const TextureDesc& d) noexcept {
    return d.is_cube ? d.array_size * 6u : d.array_size;
}

inline constexpr std::pair<std::uint32_t, std::uint32_t> compute_row_and_slice_pitch(
    TextureFormat format,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    return detail::compute_row_and_slice_pitch(format, width, height);
}

// -----------------------------------------------------------------------------
// Texture object
// -----------------------------------------------------------------------------

class Texture {
public:
    Texture() = default;

    [[nodiscard]] bool ok() const noexcept { return result_.success; }
    [[nodiscard]] const Result& result() const noexcept { return result_; }
    [[nodiscard]] const std::string& error_message() const noexcept { return result_.error; }
    [[nodiscard]] const TextureDesc& desc() const noexcept { return desc_; }
    [[nodiscard]] std::span<const std::byte> owned_bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::span<const Subresource> subresources() const noexcept { return subresources_; }

    [[nodiscard]] const Subresource* find_subresource(std::uint32_t mip, std::uint32_t array_layer = 0, std::uint32_t face = 0) const noexcept {
        const auto wanted = calc_subresource_index(mip, array_layer, face, desc_.mip_count, desc_.is_cube);
        if (wanted >= subresources_.size()) return nullptr;
        return &subresources_[wanted];
    }

    static Texture from_memory(const void* data, std::size_t size) {
        Texture t;
        t.parse_dds(data, size);
        return t;
    }

    static Texture from_bytes(std::vector<std::byte> bytes) {
        Texture t;
        t.storage_ = std::move(bytes);
        t.bytes_ = std::span<const std::byte>(t.storage_.data(), t.storage_.size());
        t.parse_dds(t.storage_.data(), t.storage_.size());
        return t;
    }

    static Texture from_file(const std::string& path) {
        Texture t;

        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) {
            t.result_ = Result::fail("failed to open file: " + path);
            return t;
        }

        const auto end = f.tellg();
        if (end < 0) {
            t.result_ = Result::fail("failed to determine file size: " + path);
            return t;
        }

        t.storage_.resize(static_cast<std::size_t>(end));
        f.seekg(0, std::ios::beg);
        if (!t.storage_.empty()) {
            f.read(reinterpret_cast<char*>(t.storage_.data()), static_cast<std::streamsize>(t.storage_.size()));
            if (!f) {
                t.result_ = Result::fail("failed to read file: " + path);
                return t;
            }
        }

        t.bytes_ = std::span<const std::byte>(t.storage_.data(), t.storage_.size());
        t.parse_dds(t.storage_.data(), t.storage_.size());
        return t;
    }

private:
    void parse_dds(const void* raw, std::size_t size) {
        try {
            if (!raw || size < sizeof(std::uint32_t) + sizeof(detail::DDS_HEADER)) {
                result_ = Result::fail("buffer too small for DDS file");
                return;
            }

            const auto* bytes = static_cast<const std::byte*>(raw);
            const auto* magic = reinterpret_cast<const std::uint32_t*>(bytes);
            if (*magic != detail::DDS_MAGIC) {
                result_ = Result::fail("not a DDS file");
                return;
            }

            const auto* header = reinterpret_cast<const detail::DDS_HEADER*>(bytes + sizeof(std::uint32_t));
            if (header->size != sizeof(detail::DDS_HEADER) || header->ddspf.size != sizeof(detail::DDS_PIXELFORMAT)) {
                result_ = Result::fail("invalid DDS header size");
                return;
            }

            const detail::DDS_HEADER_DXT10* dx10 = nullptr;
            std::size_t data_offset = sizeof(std::uint32_t) + sizeof(detail::DDS_HEADER);
            if ((header->ddspf.flags & detail::DDPF_FOURCC) && header->ddspf.four_cc == detail::FOURCC_DX10) {
                if (size < data_offset + sizeof(detail::DDS_HEADER_DXT10)) {
                    result_ = Result::fail("DDS file truncated before DX10 header");
                    return;
                }
                dx10 = reinterpret_cast<const detail::DDS_HEADER_DXT10*>(bytes + data_offset);
                data_offset += sizeof(detail::DDS_HEADER_DXT10);
            }

            TextureDesc td{};
            td.width = header->width;
            td.height = header->height;
            td.depth = (header->caps2 & detail::DDSCAPS2_VOLUME) ? (header->depth ? header->depth : 1u) : 1u;
            td.mip_count = header->mip_map_count ? header->mip_map_count : 1u;
            td.array_size = 1u;
            td.dimension = TextureDimension::tex2d;
            td.is_cube = false;

            if (dx10) {
                td.format = detail::dxgi_to_texture_format(dx10->dxgi_format);
                if (td.format == TextureFormat::unknown) {
                    result_ = Result::fail("unsupported DXGI format in DDS DX10 header");
                    return;
                }

                td.is_srgb = detail::texture_format_is_srgb(td.format);
                td.array_size = dx10->array_size ? dx10->array_size : 1u;
                td.is_cube = (dx10->misc_flag & detail::DDS_RESOURCE_MISC_TEXTURECUBE) != 0;

                switch (dx10->resource_dimension) {
                    case detail::DDS_DIMENSION_TEXTURE1D:
                        td.dimension = TextureDimension::tex1d;
                        td.height = 1;
                        td.depth = 1;
                        break;
                    case detail::DDS_DIMENSION_TEXTURE2D:
                        td.dimension = td.is_cube ? TextureDimension::cube : TextureDimension::tex2d;
                        td.depth = 1;
                        break;
                    case detail::DDS_DIMENSION_TEXTURE3D:
                        td.dimension = TextureDimension::tex3d;
                        td.depth = header->depth ? header->depth : 1u;
                        td.array_size = 1;
                        td.is_cube = false;
                        break;
                    default:
                        result_ = Result::fail("unsupported DDS resource dimension");
                        return;
                }
            } else {
                td.format = detail::legacy_dds_pf_to_texture_format(header->ddspf);
                if (td.format == TextureFormat::unknown) {
                    result_ = Result::fail("unsupported legacy DDS pixel format; prefer DDS DX10 for broad format support");
                    return;
                }

                td.is_srgb = false;
                if (header->caps2 & detail::DDSCAPS2_VOLUME) {
                    td.dimension = TextureDimension::tex3d;
                    td.depth = header->depth ? header->depth : 1u;
                } else if (header->caps2 & detail::DDSCAPS2_CUBEMAP) {
                    td.dimension = TextureDimension::cube;
                    td.is_cube = true;
                    td.array_size = 1;
                    td.depth = 1;
                } else {
                    td.dimension = TextureDimension::tex2d;
                    td.depth = 1;
                }
            }

            if (td.width == 0 || td.height == 0 || td.mip_count == 0 || td.array_size == 0 || td.depth == 0) {
                result_ = Result::fail("invalid zero dimension in DDS");
                return;
            }

            const std::uint32_t physical_layers = td.is_cube ? (td.array_size * 6u) : td.array_size;
            const auto payload_bytes = size - data_offset;
            const auto* payload = bytes + data_offset;

            std::vector<Subresource> subs;
            subs.reserve(static_cast<std::size_t>(physical_layers) * td.mip_count);

            std::size_t offset = 0;
            for (std::uint32_t layer_or_face = 0; layer_or_face < physical_layers; ++layer_or_face) {
                for (std::uint32_t mip = 0; mip < td.mip_count; ++mip) {
                    const std::uint32_t w = detail::mip_extent(td.width, mip);
                    const std::uint32_t h = detail::mip_extent(td.height, mip);
                    const std::uint32_t d = (td.dimension == TextureDimension::tex3d) ? detail::mip_extent(td.depth, mip) : 1u;

                    const auto [row_pitch, slice_pitch] = detail::compute_row_and_slice_pitch(td.format, w, h);
                    const std::uint64_t total64 = detail::checked_mul_u64(slice_pitch, d);
                    if (total64 > std::numeric_limits<std::size_t>::max()) {
                        result_ = Result::fail("DDS subresource too large for current platform");
                        return;
                    }
                    const std::size_t total = static_cast<std::size_t>(total64);

                    if (offset + total > payload_bytes) {
                        result_ = Result::fail("DDS payload truncated while reading subresources");
                        return;
                    }

                    Subresource sr{};
                    sr.data = payload + offset;
                    sr.data_size = total;
                    sr.mip_level = mip;
                    sr.width = w;
                    sr.height = h;
                    sr.depth = d;
                    sr.row_pitch = row_pitch;
                    sr.slice_pitch = slice_pitch;

                    if (td.is_cube) {
                        sr.array_layer = layer_or_face / 6u;
                        sr.face = layer_or_face % 6u;
                    } else {
                        sr.array_layer = layer_or_face;
                        sr.face = 0;
                    }

                    subs.push_back(sr);
                    offset += total;
                }
            }

            if (offset > payload_bytes) {
                result_ = Result::fail("internal offset overflow while reading DDS payload");
                return;
            }

            desc_ = td;
            subresources_ = std::move(subs);
            if (storage_.empty()) {
                bytes_ = std::span<const std::byte>(static_cast<const std::byte*>(raw), size);
            }
            result_ = Result::ok();
        } catch (const std::exception& e) {
            result_ = Result::fail(std::string("DDS parse failed: ") + e.what());
        }
    }

private:
    Result result_{};
    TextureDesc desc_{};
    std::vector<Subresource> subresources_{};
    std::vector<std::byte> storage_{};
    std::span<const std::byte> bytes_{};
};

// -----------------------------------------------------------------------------
// Backend-agnostic upload structs
// -----------------------------------------------------------------------------

struct UploadCopy {
    std::uint32_t subresource_index = 0;
    std::uint64_t src_offset = 0;
    std::uint32_t row_pitch = 0;
    std::uint32_t slice_pitch = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 1;
    std::uint32_t mip_level = 0;
    std::uint32_t physical_layer = 0;
};

inline std::vector<UploadCopy> build_upload_table(const Texture& tex, std::uint64_t base_offset = 0) {
    std::vector<UploadCopy> out;
    out.reserve(tex.subresources().size());

    const auto all = tex.subresources();
    for (std::size_t i = 0; i < all.size(); ++i) {
        const auto& sr = all[i];
        UploadCopy c{};
        c.subresource_index = static_cast<std::uint32_t>(i);
        c.src_offset = base_offset + static_cast<std::uint64_t>(sr.data - tex.owned_bytes().data());
        c.row_pitch = sr.row_pitch;
        c.slice_pitch = sr.slice_pitch;
        c.width = sr.width;
        c.height = sr.height;
        c.depth = sr.depth;
        c.mip_level = sr.mip_level;
        c.physical_layer = calc_physical_layer(sr.array_layer, sr.face, tex.desc().is_cube);
        out.push_back(c);
    }

    return out;
}

// -----------------------------------------------------------------------------
// Optional DX12 helpers
// Define MT_WITH_DX12 before including this header if you want DXGI mappings.
// You still need to include <d3d12.h> and <dxgiformat.h> before this file.
// -----------------------------------------------------------------------------

#ifdef MT_WITH_DX12

inline DXGI_FORMAT to_dxgi_format(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::r8_unorm: return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::rg8_unorm: return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::rgba8_srgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::bgra8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::bgra8_srgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case TextureFormat::r16_float: return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::rg16_float: return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::rgba16_float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::r32_float: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::rg32_float: return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::rgba32_float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::d32_float: return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::d24_unorm_s8_uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::bc1_unorm: return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::bc1_srgb: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case TextureFormat::bc2_unorm: return DXGI_FORMAT_BC2_UNORM;
        case TextureFormat::bc2_srgb: return DXGI_FORMAT_BC2_UNORM_SRGB;
        case TextureFormat::bc3_unorm: return DXGI_FORMAT_BC3_UNORM;
        case TextureFormat::bc3_srgb: return DXGI_FORMAT_BC3_UNORM_SRGB;
        case TextureFormat::bc4_unorm: return DXGI_FORMAT_BC4_UNORM;
        case TextureFormat::bc4_snorm: return DXGI_FORMAT_BC4_SNORM;
        case TextureFormat::bc5_unorm: return DXGI_FORMAT_BC5_UNORM;
        case TextureFormat::bc5_snorm: return DXGI_FORMAT_BC5_SNORM;
        case TextureFormat::bc6h_ufloat: return DXGI_FORMAT_BC6H_UF16;
        case TextureFormat::bc6h_sfloat: return DXGI_FORMAT_BC6H_SF16;
        case TextureFormat::bc7_unorm: return DXGI_FORMAT_BC7_UNORM;
        case TextureFormat::bc7_srgb: return DXGI_FORMAT_BC7_UNORM_SRGB;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

inline D3D12_RESOURCE_DIMENSION to_d3d12_dimension(const TextureDesc& d) noexcept {
    switch (d.dimension) {
        case TextureDimension::tex1d: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        case TextureDimension::tex2d:
        case TextureDimension::cube:  return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        case TextureDimension::tex3d: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        default: return D3D12_RESOURCE_DIMENSION_UNKNOWN;
    }
}

inline D3D12_RESOURCE_DESC make_d3d12_resource_desc(const TextureDesc& d, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) noexcept {
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = to_d3d12_dimension(d);
    rd.Alignment = 0;
    rd.Width = d.width;
    rd.Height = d.height;
    rd.DepthOrArraySize = static_cast<UINT16>(d.dimension == TextureDimension::tex3d ? d.depth : physical_layer_count(d));
    rd.MipLevels = static_cast<UINT16>(d.mip_count);
    rd.Format = to_dxgi_format(d.format);
    rd.SampleDesc.Count = 1;
    rd.SampleDesc.Quality = 0;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rd.Flags = flags;
    return rd;
}

inline D3D12_SRV_DIMENSION choose_d3d12_srv_dimension(const TextureDesc& d) noexcept {
    if (d.is_cube) {
        return d.array_size > 1 ? D3D12_SRV_DIMENSION_TEXTURECUBEARRAY : D3D12_SRV_DIMENSION_TEXTURECUBE;
    }
    switch (d.dimension) {
        case TextureDimension::tex1d: return d.array_size > 1 ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY : D3D12_SRV_DIMENSION_TEXTURE1D;
        case TextureDimension::tex2d: return d.array_size > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D;
        case TextureDimension::tex3d: return D3D12_SRV_DIMENSION_TEXTURE3D;
        case TextureDimension::cube:  return d.array_size > 1 ? D3D12_SRV_DIMENSION_TEXTURECUBEARRAY : D3D12_SRV_DIMENSION_TEXTURECUBE;
        default: return D3D12_SRV_DIMENSION_UNKNOWN;
    }
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC make_d3d12_srv_desc(const TextureDesc& d) noexcept {
    D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = to_dxgi_format(d.format);
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sd.ViewDimension = choose_d3d12_srv_dimension(d);

    switch (sd.ViewDimension) {
        case D3D12_SRV_DIMENSION_TEXTURE1D:
            sd.Texture1D.MostDetailedMip = 0;
            sd.Texture1D.MipLevels = d.mip_count;
            sd.Texture1D.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
            sd.Texture1DArray.MostDetailedMip = 0;
            sd.Texture1DArray.MipLevels = d.mip_count;
            sd.Texture1DArray.FirstArraySlice = 0;
            sd.Texture1DArray.ArraySize = d.array_size;
            sd.Texture1DArray.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE2D:
            sd.Texture2D.MostDetailedMip = 0;
            sd.Texture2D.MipLevels = d.mip_count;
            sd.Texture2D.PlaneSlice = 0;
            sd.Texture2D.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
            sd.Texture2DArray.MostDetailedMip = 0;
            sd.Texture2DArray.MipLevels = d.mip_count;
            sd.Texture2DArray.FirstArraySlice = 0;
            sd.Texture2DArray.ArraySize = d.array_size;
            sd.Texture2DArray.PlaneSlice = 0;
            sd.Texture2DArray.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURE3D:
            sd.Texture3D.MostDetailedMip = 0;
            sd.Texture3D.MipLevels = d.mip_count;
            sd.Texture3D.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURECUBE:
            sd.TextureCube.MostDetailedMip = 0;
            sd.TextureCube.MipLevels = d.mip_count;
            sd.TextureCube.ResourceMinLODClamp = 0.0f;
            break;
        case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
            sd.TextureCubeArray.MostDetailedMip = 0;
            sd.TextureCubeArray.MipLevels = d.mip_count;
            sd.TextureCubeArray.First2DArrayFace = 0;
            sd.TextureCubeArray.NumCubes = d.array_size;
            sd.TextureCubeArray.ResourceMinLODClamp = 0.0f;
            break;
        default:
            break;
    }
    return sd;
}

struct D3D12SubresourceDataLite {
    const void* pData = nullptr;
    LONG_PTR RowPitch = 0;
    LONG_PTR SlicePitch = 0;
};

inline std::vector<D3D12SubresourceDataLite> build_d3d12_subresources(const Texture& tex) {
    std::vector<D3D12SubresourceDataLite> out;
    out.reserve(tex.subresources().size());
    for (const auto& sr : tex.subresources()) {
        out.push_back({sr.data, static_cast<LONG_PTR>(sr.row_pitch), static_cast<LONG_PTR>(sr.slice_pitch)});
    }
    return out;
}

#endif // MT_WITH_DX12

// -----------------------------------------------------------------------------
// Optional Vulkan helpers
// Define MT_WITH_VULKAN before including this header if you want VkFormat /
// VkImageCreateInfo / VkImageViewCreateInfo mappings.
// You still need to include <vulkan/vulkan.h> before this file.
// -----------------------------------------------------------------------------

#ifdef MT_WITH_VULKAN

inline VkFormat to_vk_format(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::r8_unorm: return VK_FORMAT_R8_UNORM;
        case TextureFormat::rg8_unorm: return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::rgba8_unorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::rgba8_srgb: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::bgra8_unorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::bgra8_srgb: return VK_FORMAT_B8G8R8A8_SRGB;
        case TextureFormat::r16_float: return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::rg16_float: return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::rgba16_float: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::r32_float: return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::rg32_float: return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::rgba32_float: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::d32_float: return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::d24_unorm_s8_uint: return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::bc1_unorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::bc1_srgb: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case TextureFormat::bc2_unorm: return VK_FORMAT_BC2_UNORM_BLOCK;
        case TextureFormat::bc2_srgb: return VK_FORMAT_BC2_SRGB_BLOCK;
        case TextureFormat::bc3_unorm: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::bc3_srgb: return VK_FORMAT_BC3_SRGB_BLOCK;
        case TextureFormat::bc4_unorm: return VK_FORMAT_BC4_UNORM_BLOCK;
        case TextureFormat::bc4_snorm: return VK_FORMAT_BC4_SNORM_BLOCK;
        case TextureFormat::bc5_unorm: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TextureFormat::bc5_snorm: return VK_FORMAT_BC5_SNORM_BLOCK;
        case TextureFormat::bc6h_ufloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case TextureFormat::bc6h_sfloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case TextureFormat::bc7_unorm: return VK_FORMAT_BC7_UNORM_BLOCK;
        case TextureFormat::bc7_srgb: return VK_FORMAT_BC7_SRGB_BLOCK;
        default: return VK_FORMAT_UNDEFINED;
    }
}

inline VkImageType to_vk_image_type(const TextureDesc& d) noexcept {
    switch (d.dimension) {
        case TextureDimension::tex1d: return VK_IMAGE_TYPE_1D;
        case TextureDimension::tex2d:
        case TextureDimension::cube:  return VK_IMAGE_TYPE_2D;
        case TextureDimension::tex3d: return VK_IMAGE_TYPE_3D;
        default: return VK_IMAGE_TYPE_2D;
    }
}

inline VkImageViewType to_vk_view_type(const TextureDesc& d) noexcept {
    if (d.is_cube) {
        return d.array_size > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
    }
    switch (d.dimension) {
        case TextureDimension::tex1d: return d.array_size > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case TextureDimension::tex2d: return d.array_size > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case TextureDimension::tex3d: return VK_IMAGE_VIEW_TYPE_3D;
        case TextureDimension::cube:  return d.array_size > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        default: return VK_IMAGE_VIEW_TYPE_2D;
    }
}

inline VkImageUsageFlags to_vk_image_usage(TextureUsage u) noexcept {
    VkImageUsageFlags out = 0;
    if (has_flag(u, TextureUsage::sampled))       out |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (has_flag(u, TextureUsage::storage))       out |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (has_flag(u, TextureUsage::render_target)) out |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (has_flag(u, TextureUsage::depth_stencil)) out |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (has_flag(u, TextureUsage::transfer_src))  out |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (has_flag(u, TextureUsage::transfer_dst))  out |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return out;
}

inline VkImageCreateInfo make_vk_image_create_info(const TextureDesc& d) noexcept {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.flags = d.is_cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    ci.imageType = to_vk_image_type(d);
    ci.format = to_vk_format(d.format);
    ci.extent = VkExtent3D{d.width, d.height, d.dimension == TextureDimension::tex3d ? d.depth : 1u};
    ci.mipLevels = d.mip_count;
    ci.arrayLayers = d.dimension == TextureDimension::tex3d ? 1u : physical_layer_count(d);
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = to_vk_image_usage(d.intended_usage | TextureUsage::transfer_dst);
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return ci;
}

inline VkImageAspectFlags choose_vk_aspect_flags(TextureFormat f) noexcept {
    switch (f) {
        case TextureFormat::d32_float: return VK_IMAGE_ASPECT_DEPTH_BIT;
        case TextureFormat::d24_unorm_s8_uint: return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default: return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

inline VkImageViewCreateInfo make_vk_image_view_create_info(VkImage image, const TextureDesc& d) noexcept {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = to_vk_view_type(d);
    ci.format = to_vk_format(d.format);
    ci.subresourceRange.aspectMask = choose_vk_aspect_flags(d.format);
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = d.mip_count;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = d.dimension == TextureDimension::tex3d ? 1u : physical_layer_count(d);
    return ci;
}

inline std::vector<VkBufferImageCopy> build_vk_buffer_image_copies(const Texture& tex, VkDeviceSize staging_base_offset = 0) {
    std::vector<VkBufferImageCopy> out;
    out.reserve(tex.subresources().size());

    const auto base = tex.owned_bytes().data();
    for (const auto& sr : tex.subresources()) {
        VkBufferImageCopy c{};
        c.bufferOffset = staging_base_offset + static_cast<VkDeviceSize>(sr.data - base);
        c.bufferRowLength = 0;   // tightly packed
        c.bufferImageHeight = 0; // tightly packed
        c.imageSubresource.aspectMask = choose_vk_aspect_flags(tex.desc().format);
        c.imageSubresource.mipLevel = sr.mip_level;
        c.imageSubresource.baseArrayLayer = calc_physical_layer(sr.array_layer, sr.face, tex.desc().is_cube);
        c.imageSubresource.layerCount = 1;
        c.imageOffset = {0, 0, 0};
        c.imageExtent = {sr.width, sr.height, sr.depth};
        out.push_back(c);
    }
    return out;
}

#endif // MT_WITH_VULKAN


} // namespace mt
