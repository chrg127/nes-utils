#include <cstdio>
#include <cstdint>
#include <cassert>
#include <array>
#include <span>
#include <string>
#include <optional>
#include <charconv>
#include <string_view>
#include <fmt/core.h>
#include <CImg.h>
#include "stb_image.h"
#include "chr.hpp"

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

template <std::integral T = int>
std::optional<T> strconv(const char *str)
{
    T value = 0;
    const char *start = str;
    const char *end = str + strlen(str);
    auto res = std::from_chars(start, end, value, 10);
    if (res.ec != std::errc() || res.ptr != end)
        return std::nullopt;
    return value;
}

int image_to_chr(const char *input, const char *output, int bpp, chr::DataMode mode)
{
    int width, height, channels;
    unsigned char *img_data = stbi_load(input, &width, &height, &channels, 0);
    if (!img_data) {
        fmt::print(stderr, "error: couldn't load image {}\n", input);
        return 1;
    }

    FILE *out = fopen(output, "w");
    if (!out) {
        fmt::print(stderr, "error: couldn't write to {}\n", output);
        std::perror("");
        return 1;
    }

    chr::Palette pal{bpp};
    auto tmp = std::span(img_data, width*height*channels);
    auto data = chr::palette_to_indexed(tmp, pal, channels, bpp);
    chr::to_chr(data, width, height, bpp, mode, [&](std::span<uint8_t> tile) {
        fwrite(tile.data(), 1, tile.size(), out);
    });

    fclose(out);
    return 0;
}

int chr_to_image(const char *input, const char *output, int bpp, chr::DataMode mode)
{
    FILE *f = fopen(input, "r");
    if (!f) {
        fmt::print(stderr, "error: couldn't open file {}: ", input);
        std::perror("");
        return 1;
    }

    size_t height = chr::img_height(filesize(f), bpp);
    cimg_library::CImg<unsigned char> img(16 * 8, height, 1, 4);
    int y = 0;

    img.fill(0);
    chr::Palette palette{bpp};
    chr::to_indexed(f, bpp, mode, [&](std::span<uint8_t> row)
    {
        for (int x = 0; x < 128; x++) {
            const auto color = palette[row[x]];
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
    int bpp = 2;
    chr::DataMode datamode = chr::DataMode::Planar;

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
            case 'b': {
                ++argv;
                --argc;
                if (argc == 0) {
                    fmt::print(stderr, "warning: no argument provided for -b\n");
                    continue;
                }
                auto value = strconv(argv[0]);
                if (!value) {
                    fmt::print(stderr, "warning: invalid value {} for -b (default of 2 will be used)\n", argv[0]);
                    continue;
                }
                switch (value.value()) {
                case 2: case 3: case 4: case 8: bpp = value.value(); break;
                default: fmt::print(stderr, "warning: bpp can only be 2, 3, 4 or 8 (default of 2 will be used)\n");
                }
                break;
            }
            case 'd': {
                ++argv;
                --argc;
                if (argc == 0) {
                    fmt::print(stderr, "warning: no argument provided for -d\n");
                    continue;
                }
                std::string_view arg = argv[0];
                if (arg == "planar")
                    ; // default
                else if (arg == "interwined")
                    datamode = chr::DataMode::Interwined;
                else
                    fmt::print(stderr, "warning: invalid argument {} for -d (default \"planar\" will be used)\n", argv[0]);
                break;
            }
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

    return mode == Mode::TOIMG ? chr_to_image(input, output ? output : "output.png", bpp, datamode)
                               : image_to_chr(input, output ? output : "output.chr", bpp, datamode);
}
