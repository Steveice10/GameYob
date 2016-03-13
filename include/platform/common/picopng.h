#pragma once

#include <vector>

#include "types.h"

int decodePNG(std::vector<u8>& out_image, u32& image_width, u32& image_height, const u8* in_png, size_t in_size, bool convert_to_rgba32 = true);