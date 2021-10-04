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

    u8 get_pixel_data_interwined(std::span<u8> tile, int row, int col, int bpp)
    {
        u8 nbit = 7 - col;
        u8 res = 0;
        for (int i = 0; i < bpp/2; i++) {
            u8 lowbyte = tile[row*2 + i*16];
            u8 lowbit = getbit(lowbyte, nbit);
            res = setbit(res, i*2, lowbit);
            u8 hibyte = tile[row*2+1 + i*16];
            u8 hibit  = getbit(hibyte,  nbit);
            res = setbit(res, i*2 + 1, hibit);
        }
        return res;
    }

    u8 get_pixel_data_planar(std::span<u8> tile, int row, int col, int bpp)
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

    u8 get_pixel_data(std::span<u8> tile, int row, int col, int bpp)
    {
        // return get_pixel_data_interwined(tile, row, col, 4);
        return get_pixel_data_planar(tile, row, col, bpp);
    }

    // when converting tiles, they are converted row-wise, i.e. first we convert
    // the first row of every single tile, then the second, etc...
    // get_pixel_data()'s job is to do the conversion for one single tile
    std::array<u8, ROW_SIZE> get_single_row(std::span<u8> tiles, int row, int num_tiles, int bpp)
    {
        int bpt = bpp*8;
        std::array<u8, ROW_SIZE> res;
        // i = tile number; j = tile column
        for (int i = 0; i < TILES_PER_ROW; i++) {
            for (int j = 0; j < 8; j++) {
                res[i*8 + j] = i < num_tiles ?
                    get_pixel_data(tiles.subspan(i*bpt, bpt), row, j, bpp) : 0;
            }
        }
        return res;
    }

    // loop over a row of a single tile, return low byte and high byte
    auto encode_row(std::span<u8> row)
    {
        u8 low = 0, hi = 0;
        for (int i = 0; i < 8; i++) {
            u8 bits = row[i];
            low = setbit(low, 7-i, getbit(bits, 1));
            hi  = setbit(hi,  7-i, getbit(bits, 0));
        }
        return std::pair{low, hi};
    }

    // loop over the rows of a single tile. si = start index
    std::array<u8, BYTES_PER_TILE> extract_tile(std::span<u8> bytes, std::size_t si, std::size_t width)
    {
        std::array<u8, BYTES_PER_TILE> res;
        for (int y = 0; y < TILE_HEIGHT; y++) {
            std::size_t ri = si + y*width;
            auto [low, hi] = encode_row(bytes.subspan(ri, TILE_WIDTH));
            res[y  ] = hi;
            res[y+8] = low;
        }
        return res;
    }
}

void to_indexed(std::span<uint8_t> bytes, int bpp, Callback draw_row)
{
    // this loop inspect 16 tiles each iteration
    // the inner loop gets one single row of pixels, with size equal to the
    // width of the resulting image
    for (std::size_t index = 0; index < bytes.size(); index += BYTES_PER_TILE * TILES_PER_ROW) {
        std::size_t bytes_remaining = bytes.size() - index;
        std::size_t count = std::min(bytes_remaining, (std::size_t) BYTES_PER_TILE * TILES_PER_ROW);
        int num_tiles = count / BYTES_PER_TILE;
        std::span<u8> tiles = bytes.subspan(index, count);
        for (int r = 0; r < 8; r++) {
            auto row = get_single_row(tiles, r, num_tiles, bpp);
            draw_row(row);
        }
    }
}

void to_indexed(FILE *fp, int bpp, Callback callback)
{
    long size = filesize(fp);
    auto ptr = std::make_unique<u8[]>(size);
    std::fread(ptr.get(), 1, size, fp);
    to_indexed(std::span{ptr.get(), std::size_t(size)}, bpp, callback);
}

void to_chr(std::span<u8> bytes, std::size_t width, std::size_t height, int bpp, Callback2 callback)
{
    if (width % 8 != 0 || height % 8 != 0) {
        std::fprintf(stderr, "error: width and height must be a power of 8");
        return;
    }

    for (std::size_t j = 0; j < bytes.size(); j += width*TILE_WIDTH) {
        for (std::size_t i = 0; i < width; i += TILE_WIDTH) {
            auto tile = extract_tile(bytes, j+i, width);
            callback(tile);
        }
    }
}

long img_height(std::size_t num_bytes)
{
    // we put 16 tiles on every row. this corresponds to exactly 256 bytes for
    // every row and means we must make sure to have a multiple of 256.
    num_bytes = num_bytes % 256 == 0 ? num_bytes : (num_bytes/256 + 1) * 256;
    return num_bytes / 16 / 16 * 8;
}

} // namespace chr

// int main()
// {
//     std::array<u8, 32> data = { 0x00, 0xff, 0x60, 0xff, 0x70, 0xff, 0x38, 0xff, 0x1c, 0xff, 0x0e, 0xff, 0x07, 0xff, 0x03, 0xff,
//                                 0x00, 0x00, 0x60, 0x00, 0x70, 0x00, 0x38, 0x00, 0x1c, 0x00, 0x0e, 0x00, 0x07, 0x00, 0x03, 0x00, };
//     for (int i = 0; i < 8; i++) {
//         for (int j = 0; j < 8; j++) {
//             u8 byte = chr::get_pixel_data_interwined(data, j, i, 4);
//             printf("%d", byte);
//         }
//         printf("\n");
//     }
// }

