#include "chr.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>

namespace chr {

using u8  = uint8_t;
using u32 = uint32_t;

static const ColorRGBA default_pal[] = {
    ColorRGBA{ 0, 0, 0, 0xFF },
    ColorRGBA{ 0x60, 0x60, 0x60, 0xFF },
    ColorRGBA{ 0xB0, 0xB0, 0xB0, 0xFF },
    ColorRGBA{ 0xFF, 0xFF, 0xFF, 0xFF },
};
static const ColorRGBA *palette = default_pal;

const int TILES_PER_ROW = 16;
const int TILE_WIDTH = 8;
const int TILE_HEIGHT = 8;

namespace {
    long filesize(FILE *f)
    {
        long pos = ftell(f);
        fseek(f, 0, SEEK_END);
        long res = ftell(f);
        fseek(f, pos, SEEK_SET);
        return res;
    }

    constexpr inline uint64_t bitmask(uint8_t nbits)
    {
        return (1UL << nbits) - 1UL;
    }

    constexpr inline uint64_t setbits(uint64_t num, uint8_t bitno, uint8_t nbits, uint64_t data)
    {
        const uint64_t mask = bitmask(nbits);
        return (num & ~(mask << bitno)) | (data & mask) << bitno;
    }

    constexpr inline uint64_t setbit(uint64_t num, uint8_t bitno, bool data)
    {
        return setbits(num, bitno, 1, data);
    }

    constexpr inline uint64_t getbits(uint64_t num, uint8_t bitno, uint8_t nbits)
    {
        return num >> bitno & bitmask(nbits);
    }

    constexpr inline uint64_t getbit(uint64_t num, uint8_t bitno)
    {
        return getbits(num, bitno, 1);
    }

    u8 get_pixel_data(u8 tile[16], int row, int col)
    {
        u8 bit = 7 - col;
        u8 lowbyte = tile[row];
        u8 hibyte  = tile[row+8];
        u8 lowbit  = getbit(lowbyte, bit);
        u8 hibit   = getbit(hibyte, bit);
        return hibit << 1 | lowbit;
    }

    std::array<u8, 128> get_pixel_row(std::span<u8[16], 16> tiles, int row, int num_tiles)
    {
        std::array<u8, 128> res;
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 8; j++) {
                res[i*8 + j] = i < num_tiles ? get_pixel_data(tiles[i], row, j)
                                              : 0;
            }
        }
        return res;
    }

    std::pair<bool, bool> convert_one_color(ColorRGBA color)
    {
        auto it = std::find(palette, palette + 4, color);
        if (it == palette + 4) {
            std::fprintf(stderr, "warning: color not present in palette: %02X %02X %02X %02X\n", color[0], color[1], color[2], color[3]);
            return {0, 0};
        }
        int i = it - palette;
        return { getbit(i, 1), getbit(i, 0) };
    }

    // loop over a row of a single tile, return low byte and high byte
    auto convert_colors(std::span<u8> tilerow, int channels)
    {
        u8 low = 0, hi = 0;
        assert(tilerow.size() == std::size_t(8 * channels));
        for (int i = 0, j = 0; j < 8; i += channels, j++) {
            auto [lowbit, hibit] = convert_one_color(ColorRGBA{tilerow.subspan(i, channels)});
            low = setbit(low, 7-j, lowbit);
            hi  = setbit(hi,  7-j, hibit );
        }
        return std::pair{low, hi};
    }

    // loop over the rows of a single tile. si = start index
    std::array<u8, 16> extract_one(std::span<u8> tile_rows, int channels, std::size_t si, std::size_t width)
    {
        std::array<u8, 16> res;
        const int tile_row_size = 8 * channels;

        for (int y = 0; y < 8; y++) {
            std::size_t ri = si + y*width*channels;
            auto [low, hi] = convert_colors(tile_rows.subspan(ri, tile_row_size), channels);
            res[y  ] = hi;
            res[y+8] = low;
        }
        return res;
    }
}

void to_rgba(std::span<uint8_t> bytes, Callback draw_row)
{
    u8 tiles[16][16];

    for (std::size_t index = 0; index < bytes.size(); ) {
        int num_tiles = 0;
        for (num_tiles = 0; num_tiles < 16 && index < bytes.size(); num_tiles++, index += 16)
            std::memcpy(tiles[num_tiles], &bytes[index], 16);
        for (int r = 0; r < 8; r++) {
            auto row = get_pixel_row(tiles, r, num_tiles);
            draw_row(row);
        }
    }
}

void to_rgba(FILE *fp, Callback callback)
{
    long size = filesize(fp);
    auto ptr = std::make_unique<u8[]>(size);
    std::fread(ptr.get(), 1, size, fp);
    to_rgba(std::span{ptr.get(), std::size_t(size)}, callback);
}

std::vector<u8> to_chr(std::span<u8> bytes, std::size_t width, std::size_t height, int channels)
{
    std::vector<u8> res;
    auto add = [&](auto &bytes) { for (auto b : bytes) res.push_back(b); };

    if (width % 8 != 0 || height % 8 != 0) {
        std::fprintf(stderr, "error: width and height must be a power of 8");
        return res;
    }

    for (std::size_t j = 0; j < bytes.size(); j += width*channels*8) {
        for (std::size_t i = 0; i < width*channels; i += 8*channels) {
            auto tile_bytes = extract_one(bytes, channels, j+i, width);
            add(tile_bytes);
        }
    }

    return res;
}

long img_height(std::size_t num_bytes)
{
    // we put 16 tiles on every row. this corresponds to exactly 256 bytes for
    // every row and means we must make sure to have a multiple of 256.
    num_bytes = num_bytes % 256 == 0 ? num_bytes : (num_bytes/256 + 1) * 256;
    return num_bytes / 16 / 16 * 8;
}

void use_palette(const std::array<ColorRGBA, 4> &pal)
{
    palette = pal.data();
}

ColorRGBA get_color(u8 color)
{
    assert(color <= 3);
    return palette[color];
}

} // namespace chr
