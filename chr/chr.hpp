#pragma once

#include <array>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

namespace chr {

struct ColorRGBA {
    std::array<uint8_t, 4> data;

    explicit ColorRGBA(uint32_t value)
    {
        data[0] = value >> 24 & 0xFF;
        data[1] = value >> 16 & 0xFF;
        data[2] = value >> 8  & 0xFF;
        data[3] = value       & 0xFF;
    }

    explicit ColorRGBA(std::span<uint8_t> color)
    {
        if (color.size() == 1) {
            data[0] = data[1] = data[2] = color[0];
            data[3] = 0xFF;
        } else {
            data[0] = color[0];
            data[1] = color[1];
            data[2] = color[2];
            data[3] = color.size() >= 4 ? color[3] : 0xFF;
        }
    }

    ColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        data[0] = r;
        data[1] = g;
        data[2] = b;
        data[3] = a;
    }

    uint8_t red() const   { return data[0]; }
    uint8_t green() const { return data[1]; }
    uint8_t blue() const  { return data[2]; }
    uint8_t alpha() const { return data[3]; }

    uint8_t operator[](std::size_t i) { return data[i]; }
};

inline bool operator==(const ColorRGBA &c1, const ColorRGBA &c2)
{
    return c1.data[0] == c2.data[0]
        && c1.data[1] == c2.data[1]
        && c1.data[2] == c2.data[2]
        && c1.data[3] == c2.data[3];
}

using Callback = std::function<void(std::span<uint8_t, 128>)>;

/* convert: chr to rgba
 * extract: rgba to chr */
void to_rgba(std::span<uint8_t> bytes, Callback draw_row);
void to_rgba(FILE *fp, Callback draw_row);
std::vector<uint8_t> to_chr(std::span<uint8_t> bytes, std::size_t width, std::size_t height, int channels);
long img_height(std::size_t num_bytes);
void use_palette(const std::array<ColorRGBA, 4> &palette);
ColorRGBA get_color(uint8_t color);

} // namespace chr
