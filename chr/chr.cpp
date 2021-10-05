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

template <typename T>
class HeapArray {
    std::unique_ptr<T[]> ptr;
    std::size_t len = 0;

public:
    HeapArray() = default;
    explicit HeapArray(std::size_t s) : ptr(std::make_unique<T[]>(s)), len(s) {}
    explicit HeapArray(int s) : HeapArray(std::size_t(s)) {}

    T & operator[](std::size_t pos) { return ptr[pos]; }
    T *data()                       { return ptr.get(); }
    T *begin() const                { return ptr.get(); }
    T *end() const                  { return ptr.get() + len; }
    std::size_t size() const        { return len; }
};

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
}



/* decoding functions (chr -> image) */

namespace {
    u8 decode_pixel_interwined(std::span<u8> tile, int row, int col, int bpp)
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
            for (int j = 0; j < 8; j++) {
                res[i*8 + j] = i < num_tiles ?
                    decode_pixel(tiles.subspan(i*bpt, bpt), row, j, bpp, mode) : 0;
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
        for (int r = 0; r < 8; r++) {
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
    HeapArray<u8> encode_row(std::span<u8> row, int bpp)
    {
        HeapArray<u8> bytes{bpp};
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

    // loop over the rows of a single tile. si = start index
    HeapArray<u8> encode_tile(std::span<u8> tiles, std::size_t si, std::size_t width, int bpp, DataMode mode)
    {
        int bpt = bpp*8;
        HeapArray<u8> res{bpt};

        for (int y = 0; y < TILE_HEIGHT; y++) {
            std::size_t ri = si + y*width;
            auto bytes = encode_row(tiles.subspan(ri, TILE_WIDTH), bpp);
            if (mode == DataMode::Planar) {
                for (std::size_t i = 0; i < bytes.size(); i++)
                    res[y + i*8] = bytes[i];
            } else {
                for (std::size_t i = 0; i < bytes.size()/2; i++) {
                    res[y*2 + i*16    ] = bytes[i*2  ];
                    res[y*2 + i*16 + 1] = bytes[i*2+1];
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
            write_data(tile);
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
