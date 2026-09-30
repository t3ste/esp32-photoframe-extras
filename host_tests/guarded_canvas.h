#ifndef GUARDED_CANVAS_H
#define GUARDED_CANVAS_H

// A canvas for the screen tests with guard bytes before and after its pixel buffer, so a stroke
// that leaves the canvas is caught.

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

extern "C" {
#include "screen_canvas.h"
}

struct GuardedCanvas {
    static constexpr size_t kGuard = 4096;
    std::vector<uint8_t> memory;
    canvas_t canvas;

    GuardedCanvas(int w, int h) : memory((size_t) w * h * 3 + 2 * kGuard, 0xA5)
    {
        // white paper in the canvas itself, 0xA5 around it
        std::fill(memory.begin() + kGuard, memory.end() - kGuard, 255);
        canvas.rgb = memory.data() + kGuard;
        canvas.width = w;
        canvas.height = h;
    }

    bool guards_intact() const
    {
        for (size_t i = 0; i < kGuard; i++) {
            if (memory[i] != 0xA5 || memory[memory.size() - 1 - i] != 0xA5) {
                return false;
            }
        }
        return true;
    }

    // Pixels that are not white.
    size_t painted() const
    {
        size_t n = 0;
        for (size_t i = 0; i < (size_t) canvas.width * canvas.height; i++) {
            const uint8_t *p = canvas.rgb + i * 3;
            n += (p[0] != 255 || p[1] != 255 || p[2] != 255) ? 1 : 0;
        }
        return n;
    }

    // The colours used, as 0xRRGGBB.
    std::set<uint32_t> colours() const
    {
        std::set<uint32_t> seen;
        for (size_t i = 0; i < (size_t) canvas.width * canvas.height; i++) {
            const uint8_t *p = canvas.rgb + i * 3;
            seen.insert((uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2]);
        }
        return seen;
    }

    // Whether the pixel is white paper.
    bool blank(int x, int y) const
    {
        const uint8_t *p = canvas.rgb + ((size_t) y * canvas.width + x) * 3;
        return p[0] == 255 && p[1] == 255 && p[2] == 255;
    }
};

#endif
