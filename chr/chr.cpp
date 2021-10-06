#include "chr.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>

using u8  = uint8_t;
using u32 = uint32_t;

namespace chr {

const int TILES_PER_ROW = 16;
const int TILE_WIDTH = 8;
const int TILE_HEIGHT = 8;
const int BPP = 2;
const int BYTES_PER_TILE = BPP * 8;
const int ROW_SIZE = TILES_PER_ROW * TILE_WIDTH;
const int MAX_BPP = 8;

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

    constexpr unsigned pow2(unsigned n)
    {
        int r = 1;
        while (n-- > 0) {
            r *= 2;
        }
        return r;
    }

    template <unsigned BPP>
    constexpr std::array<ColorRGBA, pow2(BPP)> make_default_palette()
    {
        constexpr unsigned N = pow2(BPP);
        constexpr uint8_t base = 0xFF / (N-1);
        std::array<ColorRGBA, N> palette;
        for (unsigned i = 0; i < N; i++) {
            uint8_t value = base * i;
            palette[i] = ColorRGBA{value, value, value, 0xFF};
        }
        return palette;
    }

    const auto palette_2bpp = make_default_palette<2>();
    const auto palette_3bpp = make_default_palette<3>();
    const auto palette_4bpp = make_default_palette<4>();
    const auto palette_8bpp = make_default_palette<8>();

    std::span<const ColorRGBA> get_palette(int bpp)
    {
        switch (bpp) {
        case 2:  return palette_2bpp;
        case 3:  return palette_3bpp;
        case 4:  return palette_4bpp;
        case 8:  return palette_8bpp;
        default: return std::span<const ColorRGBA>{};
        }
    }
}



Palette::Palette(int bpp)
    : data(get_palette(bpp))
{ }

int Palette::find_color(ColorRGBA color) const
{
    auto it = std::find(data.begin(), data.end(), color);
    return it != data.end() ? it - data.begin() : -1;
}

void Palette::dump() const
{
    for (auto color : data)
        fprintf(stderr, "%02X %02X %02X\n", color.red(), color.green(), color.blue());
}



/* decoding functions (chr -> image) */

namespace {
    u8 decode_pixel_planar(std::span<u8> tile, int row, int col, int bpp)
    {
        u8 nbit = 7 - col;
        u8 res = 0;
        for (int i = 0; i < bpp; i++) {
            u8 byte = tile[row + i*8];
            u8 bit  = getbit(byte, nbit);
            res     = setbit(res, i, bit);
        }
        return res;
    }

    u8 decode_pixel_interwined(std::span<u8> tile, int row, int col, int bpp)
    {
        u8 nbit = 7 - col;
        u8 res = 0;
        for (int i = 0; i < bpp/2; i++) {
            u8 lowbyte = tile[i*16 + row*2    ];
            u8 hibyte  = tile[i*16 + row*2 + 1];
            u8 lowbit  = getbit(lowbyte, nbit);
            u8 hibit   = getbit(hibyte,  nbit);
            res        = setbit(res, i*2,     lowbit);
            res        = setbit(res, i*2 + 1, hibit);
        }
        if (bpp % 2 != 0) {
            int i = bpp/2;
            u8 byte = tile[i*16 + row];
            u8 bit  = getbit(byte, nbit);
            res     = setbit(res, i*2, bit);
        }
        return res;
    }

    u8 decode_pixel(std::span<u8> tile, int row, int col, int bpp, DataMode mode)
    {
        return mode == DataMode::Planar ? decode_pixel_planar(tile, row, col, bpp)
                                        : decode_pixel_interwined(tile, row, col, bpp);
    }

    // when converting tiles, they are converted row-wise, i.e. first we convert
    // the first row of every single tile, then the second, etc...
    // decode_pixel()'s job is to do the conversion for one single tile
    std::array<u8, ROW_SIZE> decode_row(std::span<u8> tiles, int row, int num_tiles, int bpp, DataMode mode)
    {
        int bpt = bpp*8;
        std::array<u8, ROW_SIZE> res;
        // i = tile number; j = tile column
        for (int i = 0; i < TILES_PER_ROW; i++) {
            std::span<u8> tile = tiles.subspan(i*bpt, bpt);
            for (int j = 0; j < 8; j++) {
                res[i*8 + j] = i < num_tiles ? decode_pixel(tile, row, j, bpp, mode) : 0;
            }
        }
        return res;
    }
}

void to_indexed(std::span<uint8_t> bytes, int bpp, DataMode mode, Callback draw_row)
{
    // this loop inspect 16 tiles each iteration
    // the inner loop gets one single row of pixels, with size equal to the
    // width of the resulting image
    int bpt = bpp*8;
    for (std::size_t index = 0; index < bytes.size(); index += bpt * TILES_PER_ROW) {
        std::size_t bytes_remaining = bytes.size() - index;
        std::size_t count = std::min(bytes_remaining, (std::size_t) bpt * TILES_PER_ROW);
        int num_tiles = count / bpt;
        std::span<u8> tiles = bytes.subspan(index, count);
        for (int r = 0; r < TILE_HEIGHT; r++) {
            auto row = decode_row(tiles, r, num_tiles, bpp, mode);
            draw_row(row);
        }
    }
}

void to_indexed(FILE *fp, int bpp, DataMode mode, Callback callback)
{
    long size = filesize(fp);
    auto ptr = std::make_unique<u8[]>(size);
    std::fread(ptr.get(), 1, size, fp);
    to_indexed(std::span{ptr.get(), std::size_t(size)}, bpp, mode, callback);
}



/* encoding functions (image -> chr) */

namespace {
    // encode single row of tile, returns a byte for each plane
    std::array<u8, MAX_BPP> encode_row(std::span<u8> row, int bpp)
    {
        std::array<u8, MAX_BPP> bytes;
        for (int i = 0; i < bpp; i++) {
            u8 byte = 0;
            for (int c = 0; c < 8; c++) {
                u8 bits = row[c];
                byte = setbit(byte, 7-c, getbit(bits, i));
            }
            bytes[i] = byte;
        }
        return bytes;
    }

    // loop over the rows of a single tile, returns bytes of encoded tile. si = start index
    std::array<u8, MAX_BPP*8> encode_tile(std::span<u8> tiles, std::size_t si, std::size_t width, int bpp, DataMode mode)
    {
        std::array<u8, MAX_BPP*8> res;
        for (int y = 0; y < TILE_HEIGHT; y++) {
            std::size_t ri = si + y*width;
            auto bytes = encode_row(tiles.subspan(ri, TILE_WIDTH), bpp);
            if (mode == DataMode::Planar) {
                for (int i = 0; i < bpp; i++)
                    res[y + i*8] = bytes[i];
            } else {
                for (int i = 0; i < bpp/2; i++) {
                    res[i*16 + y*2    ] = bytes[i*2  ];
                    res[i*16 + y*2 + 1] = bytes[i*2+1];
                }
                if (bpp % 2 != 0) {
                    int i = bpp/2;
                    res[i*16 + y] = bytes[i*2];
                }
            }
        }
        return res;
    }
}

void to_chr(std::span<u8> bytes, std::size_t width, std::size_t height, int bpp, DataMode mode, Callback write_data)
{
    if (width % 8 != 0 || height % 8 != 0) {
        std::fprintf(stderr, "error: width and height must be a power of 8");
        return;
    }

    for (std::size_t j = 0; j < bytes.size(); j += width*TILE_WIDTH) {
        for (std::size_t i = 0; i < width; i += TILE_WIDTH) {
            auto tile = encode_tile(bytes, j+i, width, bpp, mode);
            std::span<u8> tilespan{tile.begin(), tile.begin() + bpp*8};
            write_data(tilespan);
        }
    }
}



long img_height(std::size_t num_bytes, int bpp)
{
    // We put 16 tiles on every row. If we have, for example, bpp = 2,
    // this corresponds to exactly 256 bytes for every row and means
    // we must make sure to have a multiple of 256.
    std::size_t bpt = bpp*8;
    std::size_t base = bpt * TILES_PER_ROW;
    num_bytes = num_bytes % base == 0 ? num_bytes : (num_bytes/base + 1) * base;
    return num_bytes / bpt / TILES_PER_ROW * 8;
}

HeapArray<uint8_t> palette_to_indexed(std::span<uint8_t> data, const Palette &palette, int channels)
{
    HeapArray<u8> output{data.size() / channels};
    auto it = output.begin();

    for (std::size_t i = 0; i < data.size(); i += channels) {
        ColorRGBA color{data.subspan(i, channels)};
        int index = palette.find_color(color);
        if (index == -1) {
            fprintf(stderr, "warning: color not present in palette\n");
            *it++ = 0;
        }
        *it++ = index;
    }
    return output;
}

HeapArray<ColorRGBA> indexed_to_palette(std::span<uint8_t> data, const Palette &palette)
{
    HeapArray<ColorRGBA> output{data.size()};
    for (std::size_t i = 0; i < data.size(); i++)
        output[i] = palette[i];
    return output;
}

} // namespace chr
