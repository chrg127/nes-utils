#pragma once

#include <array>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

namespace chr {

using Callback  = std::function<void(std::span<uint8_t, 128>)>;
using Callback2 = std::function<void(std::span<uint8_t>)>;

void to_indexed(std::span<uint8_t> bytes, int bpp, Callback draw_row);
void to_indexed(FILE *fp, int bpp, Callback draw_row);
void to_chr(std::span<uint8_t> bytes, std::size_t width, std::size_t height, int bpp, Callback2 callback);
long img_height(std::size_t num_bytes);

} // namespace chr
