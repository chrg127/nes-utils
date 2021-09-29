#include <cstdio>
#include <cstdint>
#include <cassert>
#include <array>
#include <span>
#include <string>
#include <fmt/core.h>
#include <CImg.h>
#include "chr.hpp"

using namespace std::literals;

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

void process(FILE *f, const char *output_name)
{
    cimg_library::CImg<unsigned char> img(16 * 8, chr::img_height(filesize(f)), 1, 4);
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

    // auto text_backend = [](std::span<uint8_t> row)
    // {
    //     for (int x = 0; x < 128; x++)
    //         fmt::print("{}", row[x]);
    //     fmt::print("\n");
    // };

    img.fill(0);
    chr::convert(f, img_backend);
    img.save_png(output_name);
}

void usage()
{
    fmt::print(stderr, "usage: chrconvert [file...]\n"
                       "valid flags:\n"
                       "    -h: show this help text\n"
                       "    -o FILENAME: output to FILENAME\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }

    char *input = NULL, *output = NULL;
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

    FILE *f = std::fopen(input, "r");
    if (!f) {
        fmt::print(stderr, "error: couldn't open file {}: ", input);
        std::perror("");
        return 1;
    }

    process(f, output ? output : "output.png");
    fclose(f);
}

