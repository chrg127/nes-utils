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
#undef None
#include "cmdline.hpp"

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

template <typename T = int>
std::optional<T> _conv(const char *start, const char *end, unsigned base = 10)
{
    static_assert(std::is_integral_v<T>, "T must be an integral numeric type");
    T value = 0;
    auto res = std::from_chars(start, end, value, base);
    if (res.ec != std::errc() || res.ptr != end)
        return std::nullopt;
    return value;
}

template <typename T = int, typename TStr = std::string>
std::optional<T> strconv(const TStr &str, unsigned base = 10)
{
    return _conv<T>(str.data(), str.data() + str.size(), base);
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
    auto data = chr::palette_to_indexed(tmp, pal, channels);
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

bool select_mode(std::string_view arg, int &bpp, chr::DataMode &mode)
{
    if (arg == "nes") {
        bpp = 2;
        mode = chr::DataMode::Planar;
        return true;
    }
    if (arg == "snes") {
        bpp = 4;
        mode = chr::DataMode::Interwined;
        return true;
    }
    return false;
}

using cmdline::ParamType;

static const cmdline::ArgumentList arglist = {
    { 'h', "help",      "show this help text"                                         },
    { 'o', "output",    "FILENAME: output to FILENAME",             ParamType::Single },
    { 'r', "reverse",   "convert from image to chr"                                   },
    { 'b', "bpp",       "NUMBER: specify bpp (bits per pixel)",     ParamType::Single },
    { 'd', "data-mode", "(planar | interwined): specify data mode", ParamType::Single },
    { 'm', "mode",      "(nes | snes): specify mode",               ParamType::Single },
};

int main(int argc, char *argv[])
{
    auto usage = []() {
        fmt::print(stderr, "usage: chrconvert [file...]\n");
        cmdline::print_args(arglist, stderr);
    };

    if (argc < 2) {
        usage();
        return 1;
    }

    enum class Mode { TOIMG, TOCHR } mode = Mode::TOIMG;
    const char *input = NULL, *output = NULL;
    int bpp = 2;
    chr::DataMode datamode = chr::DataMode::Planar;

    auto result = cmdline::parse(argc, argv, arglist);
    if (result.has['h']) {
        usage();
        return 0;
    }
    if (result.has['o'])
        output = result.params['o'].data();
    if (result.has['r'])
        mode = Mode::TOCHR;
    if (result.has['b']) {
        auto num = strconv(result.params['b']);
        if (!num)
            fmt::print(stderr, "warning: invalid value {} for -b (default of 2 will be used)\n", argv[0]);
        else if (num.value() == 0 || num.value() > 8)
            fmt::print(stderr, "warning: bpp can only be 1 to 8 (default of 2 will be used)\n");
        else
            bpp = num.value();
    }
    if (result.has['d']) {
        auto param = result.params['d'];
        if (param == "planar")
            ; // default
        else if (param == "interwined")
            datamode = chr::DataMode::Interwined;
        else
            fmt::print(stderr, "warning: invalid argument {} for -d (default \"planar\" will be used)\n", argv[0]);
    }
    if (result.has['m']) {
        if (!select_mode(result.params['m'], bpp, datamode))
            fmt::print(stderr, "warning: invalid mode (defaults will be used)\n");
    }

    if (result.items.size() == 0) {
        fmt::print(stderr, "error: no file specified\n");
        usage();
        return 1;
    } else if (result.items.size() > 1)
        fmt::print(stderr, "error: too many files specified (only first will be used)\n");
    input = result.items[0].data();

    return mode == Mode::TOIMG ? chr_to_image(input, output ? output : "output.png", bpp, datamode)
                               : image_to_chr(input, output ? output : "output.chr", bpp, datamode);
}
