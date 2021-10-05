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

struct ColorRGBA {
    std::array<uint8_t, 4> data;

    constexpr ColorRGBA() = default;

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

    constexpr ColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : data({r, g, b, a}) { }

    constexpr uint8_t red() const   { return data[0]; }
    constexpr uint8_t green() const { return data[1]; }
    constexpr uint8_t blue() const  { return data[2]; }
    constexpr uint8_t alpha() const { return data[3]; }

    constexpr uint8_t operator[](std::size_t i) { return data[i]; }
};

inline bool operator==(const ColorRGBA &c1, const ColorRGBA &c2)
{
    return c1.data[0] == c2.data[0]
        && c1.data[1] == c2.data[1]
        && c1.data[2] == c2.data[2]
        && c1.data[3] == c2.data[3];
}

template <unsigned BPP>
constexpr std::array<ColorRGBA, BPP*BPP> make_default_palette()
{
    std::array<ColorRGBA, BPP*BPP> palette;
    constexpr uint8_t base = 0xFF / (BPP*BPP-1);
    for (int i = 0; i < BPP*BPP; i++) {
        uint8_t value = base * i;
        palette[i] = ColorRGBA{value, value, value, 0xFF};
    }
    return palette;
}

static const auto palette_2bpp = make_default_palette<2>();
static const auto palette_4bpp = make_default_palette<4>();
static const auto palette_8bpp = make_default_palette<8>();

ColorRGBA get_color(int color, int bpp)
{
    if (bpp == 2) return palette_2bpp[color];
    if (bpp == 4) return palette_4bpp[color];
    if (bpp == 8) return palette_8bpp[color];
    return ColorRGBA{};
}

template <std::size_t Size>
int array_find_color(const std::array<ColorRGBA, Size> &palette, ColorRGBA color)
{
    auto it = std::find(palette.begin(), palette.end(), color);
    return it != palette.end() ? it - palette.begin() : -1;
}

int find_color(ColorRGBA color, int bpp)
{
    if (bpp == 2) return array_find_color(palette_2bpp, color);
    if (bpp == 4) return array_find_color(palette_4bpp, color);
    if (bpp == 8) return array_find_color(palette_8bpp, color);
    return -1;
}

std::vector<uint8_t> convert_to_indexed(std::span<unsigned char> data, int channels, int bpp)
{
    std::vector<uint8_t> output;
    output.reserve(data.size() / channels);

    for (std::size_t i = 0; i < data.size(); i += channels) {
        ColorRGBA color{data.subspan(i, channels)};
        int index = find_color(color, bpp);
        if (index == -1) {
            fmt::print(stderr, "warning: color not present in palette\n");
            continue;
        }
        output.push_back(index);
    }
    return output;
}

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

    auto tmp = std::span(img_data, width*height*channels);
    auto data = convert_to_indexed(tmp, channels, bpp);
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

    size_t height = chr::img_height(filesize(f));
    cimg_library::CImg<unsigned char> img(16 * 8, height, 1, 4);
    int y = 0;

    img.fill(0);
    chr::to_indexed(f, bpp, mode, [&](std::span<uint8_t> row)
    {
        for (int x = 0; x < 128; x++) {
            auto color = get_color(row[x], bpp);
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

void test_indexed()
{
    std::array<uint8_t, 32> data = { 0x00, 0xff, 0x60, 0xff, 0x70, 0xff, 0x38, 0xff, 0x1c, 0xff, 0x0e, 0xff, 0x07, 0xff, 0x03, 0xff,
                                     0x00, 0x00, 0x60, 0x00, 0x70, 0x00, 0x38, 0x00, 0x1c, 0x00, 0x0e, 0x00, 0x07, 0x00, 0x03, 0x00, };
    chr::to_indexed(data, 4, chr::DataMode::Interwined, [](std::span<uint8_t> idata) {
        for (auto b : idata)
            fmt::print("{}", b);
        fmt::print("\n");
    });
}

void test_chr()
{
    std::array<uint8_t, 64> data = { 0, 1, 0, 0, 0, 0, 0, 3,
                                     1, 1, 0, 0, 0, 0, 3, 0,
                                     0, 1, 0, 0, 0, 3, 0, 0,
                                     0, 1, 0, 0, 3, 0, 0, 0,
                                     0, 0, 0, 3, 0, 2, 2, 0,
                                     0, 0, 3, 0, 0, 0, 0, 2,
                                     0, 3, 0, 0, 0, 0, 2, 0,
                                     3, 0, 0, 0, 0, 2, 2, 2 };
    chr::to_chr(data, 8, 8, 2, chr::DataMode::Planar, [](std::span<uint8_t> chrdata) {
        for (auto b : chrdata)
            fmt::print("{:08b}\n", b);
    });
}

int main(int argc, char *argv[])
{
    // test_chr();
    // return 0;

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
                case 2: case 4: case 8: bpp = value.value(); break;
                default: fmt::print(stderr, "warning: bpp can only be 2, 4 or 8 (default of 2 will be used)\n");
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
