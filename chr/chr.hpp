#pragma once

#include <array>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <memory>

namespace chr {

using Callback  = std::function<void(std::span<uint8_t>)>;

enum class DataMode {
    Planar,
    Interwined,
};

class ColorRGBA {
    std::array<uint8_t, 4> data;

public:
    constexpr ColorRGBA() = default;
    constexpr ColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : data({r, g, b, a}) { }

    explicit ColorRGBA(std::span<uint8_t> color)
    {
        if (color.size() == 1) {
            data[0] = data[1] = data[2] = color[0];
            data[3] = 0xFF;
        } else {
            data[0] = color[0];
            data[1] = color[1];
            data[2] = color[2];
            data[3] = color.size() >= 4 ? color[3] : 0xFF;
        }
    }

    constexpr uint8_t red() const   { return data[0]; }
    constexpr uint8_t green() const { return data[1]; }
    constexpr uint8_t blue() const  { return data[2]; }
    constexpr uint8_t alpha() const { return data[3]; }
    constexpr uint8_t operator[](std::size_t i) const { return data[i]; }
    friend bool operator==(const ColorRGBA &c1, const ColorRGBA &c2);
};

inline bool operator==(const ColorRGBA &c1, const ColorRGBA &c2) { return c1.data == c2.data; }

class Palette {
    std::span<const ColorRGBA> data;

public:
    explicit Palette(int bpp);
    explicit Palette(std::span<ColorRGBA> p) : data(p) {}

    const ColorRGBA & operator[](std::size_t pos) const { return data[pos]; }
    int find_color(ColorRGBA color) const;
    void dump() const;
};

template <typename T>
class HeapArray {
    std::unique_ptr<T[]> ptr;
    std::size_t len = 0;
public:
    HeapArray() = default;
    explicit HeapArray(std::size_t s) : ptr(std::make_unique<T[]>(s)), len(s) {}
    T *begin() const                { return ptr.get(); }
    T *end()   const                { return ptr.get() + len; }
    T *data()  const                { return ptr.get(); }
    std::size_t size() const        { return len; }
    T & operator[](std::size_t pos) { return ptr[pos]; }
};

void to_indexed(std::span<uint8_t> bytes, int bpp, DataMode mode, Callback draw_row);
void to_indexed(FILE *fp, int bpp, DataMode mode, Callback draw_row);
void to_chr(std::span<uint8_t> bytes, std::size_t width, std::size_t height, int bpp, DataMode mode, Callback write_data);
long img_height(std::size_t num_bytes, int bpp);
HeapArray<uint8_t> palette_to_indexed(std::span<uint8_t> data, const Palette &palette, int channels);
HeapArray<ColorRGBA> indexed_to_palette(std::span<uint8_t> data, const Palette &palette);

} // namespace chr
