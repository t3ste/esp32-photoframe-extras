#include <gtest/gtest.h>

extern "C" {
#include "history_decimate.h"
}

namespace
{

// How many of `count` readings survive with the stride chosen for `max_points`.
size_t kept(size_t count, size_t max_points)
{
    size_t stride = history_stride(count, max_points);
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        if (history_keep(i, count, stride)) {
            n++;
        }
    }
    return n;
}

}  // namespace

TEST(HistoryDecimate, ShortLogIsKeptWhole)
{
    EXPECT_EQ(history_stride(0, 1000), 1u);
    EXPECT_EQ(history_stride(1, 1000), 1u);
    EXPECT_EQ(history_stride(1000, 1000), 1u);
    EXPECT_EQ(kept(1000, 1000), 1000u);
}

TEST(HistoryDecimate, LongLogIsThinnedToTheLimit)
{
    // 180 days at one reading per 5 minutes - the log that made the device unresponsive.
    const size_t count = 180 * 24 * 12;
    EXPECT_EQ(count, 51840u);
    EXPECT_GT(history_stride(count, 1000), 1u);
    EXPECT_LE(kept(count, 1000), 1001u);  // the limit, plus the newest reading
    EXPECT_GT(kept(count, 1000), 500u);   // ...and not thinned out much further than needed
}

TEST(HistoryDecimate, LimitHoldsForEveryLength)
{
    for (size_t count = 0; count <= 5000; count++) {
        for (size_t max_points : {1u, 2u, 7u, 100u, 1000u}) {
            EXPECT_LE(kept(count, max_points), max_points + 1) << count << "/" << max_points;
            if (count > 0) {
                EXPECT_GE(kept(count, max_points), 1u);
            }
        }
    }
}

TEST(HistoryDecimate, OldestAndNewestReadingAreKept)
{
    for (size_t count : {2u, 3u, 999u, 1001u, 1999u, 2001u, 51840u, 51841u}) {
        size_t stride = history_stride(count, 1000);
        EXPECT_TRUE(history_keep(0, count, stride)) << count;
        EXPECT_TRUE(history_keep(count - 1, count, stride)) << count;
    }
}

TEST(HistoryDecimate, KeptReadingsAreEvenlySpaced)
{
    const size_t count = 10000;
    size_t stride = history_stride(count, 1000);
    EXPECT_EQ(stride, 10u);
    size_t previous = 0;
    for (size_t i = 1; i < count; i++) {
        if (history_keep(i, count, stride)) {
            EXPECT_LE(i - previous, stride);
            previous = i;
        }
    }
}

TEST(HistoryDecimate, NoLimitKeepsEverything)
{
    EXPECT_EQ(history_stride(50000, 0), 1u);
    EXPECT_EQ(kept(50000, 0), 50000u);
}
