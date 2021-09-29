#include <cstdint>
#include <string>
#include <fmt/core.h>
#include "stb_image.h"
#include "chr.hpp"

void usage()
{
    fmt::print(stderr, "usage: chrextract [file...]\n"
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

    char *input = NULL;
    const char *output = NULL;
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

    int width, height, channels;
    unsigned char *data = stbi_load(input, &width, &height, &channels, 0);
    if (!data) {
        fmt::print(stderr, "error: couldn't load {}\n", input);
        return 1;
    }
    std::span<uint8_t> dataspan{data, std::size_t(width*height*channels)};
    auto res = chr::extract(dataspan, channels);
    output = output ? output : "output.png";

    FILE *fp = fopen(output, "w");
    std::fwrite(res.data(), 1, res.size(), fp);
    fclose(fp);

    return 0;
}
