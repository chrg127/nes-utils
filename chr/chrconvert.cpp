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

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

int image_to_chr(const char *input, const char *output)
{
    int width, height, channels;
    unsigned char *data = stbi_load(input, &width, &height, &channels, 0);
    if (!data) {
        fmt::print(stderr, "error: couldn't load image {}\n", input);
        return 1;
    }

    std::span<uint8_t> dataspan{data, std::size_t(width*height*channels)};
    auto res = chr::to_chr(dataspan, width, height, channels);

    FILE *out = fopen(output, "w");
    if (!out) {
        fmt::print(stderr, "error: couldn't write to {}\n", output);
        std::perror("");
        return 1;
    }

    std::fwrite(res.data(), 1, res.size(), out);
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

    auto img_backend = [&](std::span<uint8_t> row)
    {
        for (int x = 0; x < 128; x++) {
            auto color = chr::get_color(row[x]);
            img(x, y, 0) = color.red();
            img(x, y, 1) = color.green();
            img(x, y, 2) = color.blue();
            img(x, y, 3) = 0xFF;
        }
        y++;
    };

    img.fill(0);
    chr::to_rgba(f, img_backend);
    fclose(f);
    img.save_png(output);

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

    bool mode = false; // chr to image
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
                mode = true; // image to chr
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

    output = output ? output : "output.png";
    if (!mode)
        return chr_to_image(input, output);
    else
        return image_to_chr(input, output);
}
