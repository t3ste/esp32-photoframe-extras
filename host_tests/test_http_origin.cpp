// The cross-site request check of the HTTP API (main/http_origin.h, build option fixes).

#include <gtest/gtest.h>

extern "C" {
#include "http_origin.h"
}

TEST(HttpOrigin, ARequestWithoutAnOriginPasses)
{
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50", nullptr));
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50", ""));
    EXPECT_TRUE(http_origin_matches_host(nullptr, nullptr));  // curl, Home Assistant: no Origin
}

TEST(HttpOrigin, TheWebUiOfTheFrameItselfPasses)
{
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50", "http://192.168.1.50"));
    EXPECT_TRUE(http_origin_matches_host("photoframe.local", "http://photoframe.local"));
    EXPECT_TRUE(http_origin_matches_host("photoframe.fritz.box", "http://photoframe.fritz.box"));
    EXPECT_TRUE(http_origin_matches_host("[fe80::1]", "http://[fe80::1]"));
    EXPECT_TRUE(http_origin_matches_host("192.168.4.1", "http://192.168.4.1"));  // set-up hotspot
}

TEST(HttpOrigin, TheHttpsServerOfTheFramePasses)
{
    EXPECT_TRUE(http_origin_matches_host("photoframe.local", "https://photoframe.local"));
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50:8443", "https://192.168.1.50:8443"));
}

TEST(HttpOrigin, TheCaseOfTheNameDoesNotMatter)
{
    EXPECT_TRUE(http_origin_matches_host("PhotoFrame.local", "HTTP://photoframe.LOCAL"));
}

TEST(HttpOrigin, ADefaultPortMayBeWrittenOrLeftOut)
{
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50:80", "http://192.168.1.50"));
    EXPECT_TRUE(http_origin_matches_host("192.168.1.50", "http://192.168.1.50:80"));
    EXPECT_TRUE(http_origin_matches_host("photoframe.local:443", "https://photoframe.local"));
    // ... but only the default of the scheme in the Origin
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50:443", "http://192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50:80", "https://192.168.1.50"));
}

TEST(HttpOrigin, AnotherSiteIsRefused)
{
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://evil.example"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "https://evil.example"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://192.168.1.51"));
    EXPECT_FALSE(http_origin_matches_host("photoframe.local", "http://localhost:5173"));
}

TEST(HttpOrigin, ANameThatMerelyStartsOrEndsLikeTheHostIsRefused)
{
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://192.168.1.50.evil.example"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://evil.192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.5", "http://192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://192.168.1.5"));
    EXPECT_FALSE(
        http_origin_matches_host("photoframe.local", "http://photoframe.local.evil.example"));
}

TEST(HttpOrigin, APortThatDiffersIsRefused)
{
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://192.168.1.50:8080"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50:8080", "http://192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50:8080", "http://192.168.1.50:8081"));
}

TEST(HttpOrigin, NullAndOtherSchemesAreRefused)
{
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "null"));  // sandboxed frame, file://
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "file://192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "ftp://192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "chrome-extension://abc"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "192.168.1.50"));
}

TEST(HttpOrigin, MalformedInputIsRefused)
{
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "https://"));
    EXPECT_FALSE(
        http_origin_matches_host("192.168.1.50", "http://192.168.1.50/"));  // an Origin has no path
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://192.168.1.50/x"));
    EXPECT_FALSE(http_origin_matches_host("192.168.1.50", "http://user@192.168.1.50"));
    EXPECT_FALSE(http_origin_matches_host("", "http://192.168.1.50"));  // no Host to compare with
    EXPECT_FALSE(http_origin_matches_host(nullptr, "http://192.168.1.50"));
}
