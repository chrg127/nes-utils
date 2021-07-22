#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "stb_image.h"
#include "chr.hpp"

void output(std::span<uint8_t> bytes, int n)
{
    std::string filename = std::string("output") + (n == 0 ? "" : std::to_string(n)) + ".chr";
    FILE *fp = fopen(filename.c_str(), "w");
    std::fwrite(bytes.data(), 1, bytes.size(), fp);
    fclose(fp);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fmt::print(stderr, "usage: chrextract [image file...]\n");
        return 1;
    }

    int n = 0;
    while (++argv, --argc > 0) {
        int width, height, channels;
        unsigned char *data = stbi_load(*argv, &width, &height, &channels, 0);
        if (!data) {
            fmt::print(stderr, "error: couldn't load {}\n", *argv);
            continue;
        }
        std::span<uint8_t> dataspan{data, std::size_t(width*height*channels)};
        auto res = chr::extract(dataspan, channels);
        output(res, n);
    }

    return 0;
}
