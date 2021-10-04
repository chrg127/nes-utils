#include <cstdio>
#include <cstdint>
#include <cassert>
#include <array>
#include <span>
#include <string>
#include <fmt/core.h>
#include <CImg.h>
#include "stb_image.h"
#include "chr.hpp"

struct ColorRGBA {
    std::array<uint8_t, 4> data;

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

    ColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : data({r, g, b, a}) { }

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

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

static const ColorRGBA palette[] = {
    ColorRGBA{ 0, 0, 0, 0xFF },
    ColorRGBA{ 0x60, 0x60, 0x60, 0xFF },
    ColorRGBA{ 0xB0, 0xB0, 0xB0, 0xFF },
    ColorRGBA{ 0xFF, 0xFF, 0xFF, 0xFF },
};

ColorRGBA get_color(int color)
{
    return palette[color];
}

std::vector<uint8_t> convert_to_indexed(std::span<unsigned char> data, int channels)
{
    std::vector<uint8_t> output;
    output.reserve(data.size() / channels);
    for (std::size_t i = 0; i < data.size(); i += channels) {
        ColorRGBA color{data.subspan(i, channels)};
        auto it = std::find(std::begin(palette), std::end(palette), color);
        if (it == std::end(palette)) {
            fmt::print(stderr, "warning: color not present in palette\n");
            continue;
        }
        int index = it - std::begin(palette);
        output.push_back(index);
    }
    return output;
}

int image_to_chr(const char *input, const char *output)
{
    int width, height, channels;
    unsigned char *data = stbi_load(input, &width, &height, &channels, 0);
    if (!data) {
        fmt::print(stderr, "error: couldn't load image {}\n", input);
        return 1;
    }

    FILE *out = fopen(output, "w");
    if (!out) {
        fmt::print(stderr, "error: couldn't write to {}\n", output);
        std::perror("");
        return 1;
    }

    auto tmp = std::span(data, width*height*channels);
    auto data_transformed = convert_to_indexed(tmp, channels);
    chr::to_chr(data_transformed, width, height, [&](std::span<uint8_t> tile) {
        fwrite(tile.data(), 1, tile.size(), out);
    });
    // std::span<uint8_t> dataspan{data, std::size_t(width*height*channels)};
    // auto res = chr::to_chr(dataspan, width, height, channels);

    fclose(out);
    return 0;
}

int chr_to_image(const char *input, const char *output)
{
    FILE *f = fopen(input, "r");
    if (!f) {
        fmt::print(stderr, "error: couldn't open file {}: ", input);
        std::perror("");
        return 1;
    }

    size_t height = chr::img_height(filesize(f));
    cimg_library::CImg<unsigned char> img(16 * 8, height, 1, 4);
    int y = 0;

    img.fill(0);
    chr::to_indexed(f, [&](std::span<uint8_t> row)
    {
        for (int x = 0; x < 128; x++) {
            auto color = get_color(row[x]);
            img(x, y, 0) = color.red();
            img(x, y, 1) = color.green();
            img(x, y, 2) = color.blue();
            img(x, y, 3) = 0xFF;
        }
        y++;
    });

    img.save_png(output);
    fclose(f);

    return 0;
}

void usage()
{
    fmt::print(stderr, "usage: chrconvert [file...]\n"
                       "valid flags:\n"
                       "    -h: show this help text\n"
                       "    -o FILENAME: output to FILENAME\n"
                       "    -r: reverse: convert from image to chr\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    enum class Mode { TOIMG, TOCHR } mode = Mode::TOIMG;
    const char *input = NULL, *output = NULL;

    while (++argv, --argc > 0) {
        if (argv[0][0] == '-') {
            switch (argv[0][1]) {
            case 'h':
                usage();
                return 0;
            case 'o':
                ++argv;
                --argc;
                if (argc == 0)
                    fmt::print(stderr, "warning: no argument provided for -o\n");
                else
                    output = argv[0];
                break;
            case 'r':
                mode = Mode::TOCHR;
                break;
            default:
                fmt::print(stderr, "warning: -{}: unknown flag\n", argv[0][1]);
            }
        } else {
            if (!input)
                input = argv[0];
            else
                fmt::print(stderr, "warning: too many files specified\n");
        }
    }

    if (!input) {
        fmt::print(stderr, "error: no file specified\n");
        return 1;
    }

    return mode == Mode::TOIMG ? chr_to_image(input, output ? output : "output.png")
                               : image_to_chr(input, output ? output : "output.chr");
}
