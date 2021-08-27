#include <cstdio>
#include <cstdint>
#include <cassert>
#include <array>
#include <span>
#include <string>
#include <fmt/core.h>
#include <CImg.h>
#include "chr.hpp"

long filesize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long res = ftell(f);
    fseek(f, 0, SEEK_SET);
    return res;
}

void process(FILE *f, int num_image)
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
    auto str = std::string("output") + (num_image == 0 ? "" : std::to_string(num_image)) + ".png";
    img.save_png(str.c_str());
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fmt::print(stderr, "usage: chrconvert [file...]\n");
        return 1;
    }

    int n = 0;
    while (++argv, --argc > 0) {
        FILE *f = std::fopen(*argv, "r");
        if (!f) {
            fmt::print(stderr, "error: couldn't open file {}:", *argv);
            std::perror("");
        }
        process(f, n);
        n++;
        fclose(f);
    }
}

