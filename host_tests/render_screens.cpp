// Host render harness of the info screens: draws every screen for the panel sizes of all boards and
// writes PNG files, so the layouts can be looked at without a frame.
//
//   render_screens <output directory> [screen] [size WxH]
//
// The screens are drawn with fixed sample data (a date, a household, a forecast, ...).

#include <png.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "info_screens_core.h"
#include "screen_canvas.h"
#include "screen_chore_wheel.h"
#include "screen_fact.h"
#include "screen_finance.h"
#include "screen_fuel.h"
#include "screen_markets.h"
#include "screen_weather.h"
}

#include "render_screens_extra.h"

namespace
{

struct Size {
    const char *board;
    int w, h;
};

const Size kSizes[] = {
    {"waveshare-800x480", 800, 480},     {"reterminal-e1004-1200x1600", 1200, 1600},
    {"xiao-ee03-1872x1404", 1872, 1404}, {"m5paper-960x540", 960, 540},
    {"portrait-480x800", 480, 800},
};

bool write_png(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h)
{
    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        return false;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < h; y++) {
        png_write_row(png, const_cast<uint8_t *>(rgb.data()) + (size_t) y * w * 3);
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return true;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <output directory> [screen] [WxH]\n", argv[0]);
        return 2;
    }
    std::string dir = argv[1];
    std::string only = argc > 2 ? argv[2] : "";
    std::string only_size = argc > 3 ? argv[3] : "";
    mkdir(dir.c_str(), 0755);

    int written = 0;
    for (const Size &size : kSizes) {
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", size.w, size.h);
        if (!only_size.empty() && only_size != label) {
            continue;
        }
        for (const RenderCase &render_case : render_cases()) {
            if (!only.empty() && only != render_case.name) {
                continue;
            }
            std::vector<uint8_t> rgb((size_t) size.w * size.h * 3, 255);
            canvas_t canvas = {rgb.data(), size.w, size.h};
            render_case.draw(&canvas);
            std::string path = dir + "/" + render_case.name + "_" + size.board + ".png";
            if (!write_png(path, rgb, size.w, size.h)) {
                fprintf(stderr, "cannot write %s\n", path.c_str());
                return 1;
            }
            written++;
        }
    }
    printf("wrote %d pictures to %s\n", written, dir.c_str());
    return 0;
}
