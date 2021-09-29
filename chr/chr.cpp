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

    std::array<u8, 128> get_pixel_row(std::span<u8[16], 16> tiles, int row)
    {
        std::array<u8, 128> res;
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 8; j++) {
                u8 pd = get_pixel_data(tiles[i], row, j);
                res[i*8 + j] = pd;
            }
        }
        return res;
    }

    std::pair<bool, bool> convert_one_color(ColorRGBA color)
    {
        auto it = std::find(palette, palette + 4, color);
        if (it == palette + 4) {
            std::fprintf(stderr,
                "warning: color not present in palette: %02X %02X %02X %02X\n",
                color.red(), color.green(), color.blue(), color.alpha()
            );
            return {0, 0};
        }
        int i = it - palette;
        return { getbit(i, 1), getbit(i, 0) };
    }

    // loop over a row of a single tile
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

    // loop over the rows of a single tile
    std::array<u8, 16> extract_one(std::span<u8> tile_rows, int channels, int i)
    {
        std::array<u8, 16> res;
        const int tile_row_size = 8 * channels;
        const int row_size = tile_row_size * 16;

        for (int j = 0; j < 8; j++) {
            int start = row_size*j + tile_row_size*i;
            auto [low, hi] = convert_colors(tile_rows.subspan(start, tile_row_size), channels);
            res[j  ] = hi;
            res[j+8] = low;
        }
        return res;
    }
}

void convert(std::span<uint8_t> bytes, Callback draw_row)
{
    u8 tiles[16][16];

    for (std::size_t index = 0; index < bytes.size(); ) {
        for (int i = 0; i < 16; i++) {
            std::memcpy(tiles[i], &bytes[index], 16);
            index += 16;
        }
        for (int i = 0; i < 8; i++) {
            auto row = get_pixel_row(tiles, i);
            draw_row(row);
        }
    }
}

void convert(FILE *fp, Callback callback)
{
    long size = filesize(fp);
    auto ptr = std::make_unique<u8[]>(size);
    std::fread(ptr.get(), 1, size, fp);
    convert(std::span{ptr.get(), std::size_t(size)}, callback);
}

const int TILES_PER_ROW = 16;
const int COLORS_PER_TILE = 8;

std::vector<uint8_t> extract(std::span<uint8_t> bytes, int channels)
{
    // assert(channels == 3 || channels == 4);
    const std::size_t count = 8 * TILES_PER_ROW * COLORS_PER_TILE * channels;
    std::vector<u8> res;
    auto add = [&](auto &bytes) { for (auto b : bytes) res.push_back(b); };

    for (std::size_t i = 0; i < bytes.size(); i += count ) {
        std::span<u8> tile_rows = bytes.subspan(i, count);
        if (tile_rows.size() < count) {
            std::fprintf(stderr, "warning: bytes not formatted correctly.\n");
            return res;
        }
        for (int i = 0; i < 16; i++) {
            auto tile_bytes = extract_one(tile_rows, channels, i);
            add(tile_bytes);
        }
    }

    return res;
}

std::vector<uint8_t> extract(FILE *fp, int channels)
{
    long size = filesize(fp);
    auto ptr = std::make_unique<u8[]>(size);
    std::fread(ptr.get(), 1, size, fp);
    return extract(std::span{ptr.get(), std::size_t(size)}, channels);
}

long img_height(std::size_t num_bytes)
{
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
