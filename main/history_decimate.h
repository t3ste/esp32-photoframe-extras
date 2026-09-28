#ifndef HISTORY_DECIMATE_H
#define HISTORY_DECIMATE_H

// Thins a long history log out to a number of points a chart can show (and the
// device can answer in reasonable time and memory): every stride-th reading plus
// always the newest one. Header-only and free of ESP-IDF, so it is host-tested
// (host_tests/test_history_decimate.cpp).

#include <stdbool.h>
#include <stddef.h>

// Distance between two kept readings so that at most max_points (+1 for the newest
// one) remain of count readings; 1 keeps everything.
static inline size_t history_stride(size_t count, size_t max_points)
{
    if (max_points == 0 || count <= max_points) {
        return 1;
    }
    return (count + max_points - 1) / max_points;
}

// Is reading number index (0 = oldest) of count kept with this stride?
static inline bool history_keep(size_t index, size_t count, size_t stride)
{
    return stride <= 1 || index % stride == 0 || index + 1 == count;
}

#endif  // HISTORY_DECIMATE_H
