#pragma once

// this is the chr C interface.

#ifdef __cpluplus
extern "C" {
#endif

#define CHR_ROW_SIZE 128

typedef void (*chr_CallbackPtr)(uint8_t *);
typedef uint8_t chr_ColorRGBA[4];

void chr_convert(uint8_t *bytes, size_t size, chr_CallbackPtr draw_row);
void chr_convert(FILE *fp, chr_CallbackPtr draw_row);
uint8_t *chr_extract(uint8_t *bytes, size_t size, int channels);
long chr_img_height(size_t num_bytes);
void chr_use_palette(const chr_ColorRGBA palette[4]);

#ifdef __cpluplus
}
#endif
