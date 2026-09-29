#include <gtest/gtest.h>

extern "C" {
#include "source_auth.h"
}

#include <string>

namespace
{

struct Split {
    source_auth_result_t result;
    std::string url;
    std::string user;
    std::string pass;
};

Split split(const std::string &in)
{
    char url[SOURCE_AUTH_URL_MAX_LEN] = "unset";
    char user[SOURCE_AUTH_USER_MAX_LEN] = "unset";
    char pass[SOURCE_AUTH_PASS_MAX_LEN] = "unset";
    Split s;
    s.result =
        source_auth_split_url(in.c_str(), url, sizeof(url), user, sizeof(user), pass, sizeof(pass));
    s.url = url;
    s.user = user;
    s.pass = pass;
    return s;
}

}  // namespace

TEST(SourceAuth, PlainUrlHasNoLogin)
{
    Split s = split("https://calendar.example.org/dav/me/cal/?export");
    EXPECT_EQ(s.result, SOURCE_AUTH_NO_LOGIN);
    EXPECT_EQ(s.url, "unset");  // outputs untouched
    EXPECT_EQ(s.user, "unset");
}

TEST(SourceAuth, SplitsUserAndPassword)
{
    Split s = split("https://alice:s3cret@calendar.example.org/dav/alice/cal/?export");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.url, "https://calendar.example.org/dav/alice/cal/?export");
    EXPECT_EQ(s.user, "alice");
    EXPECT_EQ(s.pass, "s3cret");
}

TEST(SourceAuth, PercentDecodesBothParts)
{
    Split s = split("https://al%40ice%3Aexample:p%40ss%3Aw%2Fd%25@host.example.org/x");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.user, "al@ice:example");
    EXPECT_EQ(s.pass, "p@ss:w/d%");
    EXPECT_EQ(s.url, "https://host.example.org/x");
}

TEST(SourceAuth, PasswordMayContainColonsAndPlus)
{
    Split s = split("https://bob:a:b+c@host.example.org/");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.user, "bob");
    EXPECT_EQ(s.pass, "a:b+c");  // '+' is not a space in a login
}

TEST(SourceAuth, UserWithoutPassword)
{
    Split s = split("https://token@host.example.org/feed.ics");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.user, "token");
    EXPECT_EQ(s.pass, "");
    EXPECT_EQ(s.url, "https://host.example.org/feed.ics");
}

TEST(SourceAuth, AtSignInPathOrQueryIsNotALogin)
{
    EXPECT_EQ(split("https://host.example.org/cal?owner=me@example.org").result,
              SOURCE_AUTH_NO_LOGIN);
    EXPECT_EQ(split("https://host.example.org/users/me@example.org/cal.ics").result,
              SOURCE_AUTH_NO_LOGIN);
    EXPECT_EQ(split("https://host.example.org#me@example.org").result, SOURCE_AUTH_NO_LOGIN);
}

TEST(SourceAuth, LoginAndAtSignInQuery)
{
    Split s = split("https://u:p@host.example.org/cal?owner=me@example.org");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.url, "https://host.example.org/cal?owner=me@example.org");
}

TEST(SourceAuth, KeepsPortAndScheme)
{
    Split s = split("http://u:p@192.168.1.10:5232/alice/cal/");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.url, "http://192.168.1.10:5232/alice/cal/");
}

TEST(SourceAuth, EmptyPathAndNoSchemeSeparator)
{
    Split s = split("https://u:p@host.example.org");
    ASSERT_EQ(s.result, SOURCE_AUTH_SPLIT);
    EXPECT_EQ(s.url, "https://host.example.org");

    EXPECT_EQ(split("host.example.org/u:p@x").result, SOURCE_AUTH_NO_LOGIN);
    EXPECT_EQ(split("").result, SOURCE_AUTH_NO_LOGIN);
}

TEST(SourceAuth, RejectsMalformedEscapes)
{
    EXPECT_EQ(split("https://u:p%zz@host/").result, SOURCE_AUTH_INVALID);
    EXPECT_EQ(split("https://u:p%4@host/").result, SOURCE_AUTH_INVALID);
    EXPECT_EQ(split("https://u:p%@host/").result, SOURCE_AUTH_INVALID);
    EXPECT_EQ(split("https://u:p%00@host/").result, SOURCE_AUTH_INVALID);  // no embedded NUL
}

TEST(SourceAuth, RejectsPartsThatDoNotFit)
{
    EXPECT_EQ(split("https://" + std::string(SOURCE_AUTH_USER_MAX_LEN, 'u') + ":p@host/").result,
              SOURCE_AUTH_INVALID);
    EXPECT_EQ(split("https://u:" + std::string(SOURCE_AUTH_PASS_MAX_LEN, 'p') + "@host/").result,
              SOURCE_AUTH_INVALID);
    // exactly one byte less than the buffer is fine
    EXPECT_EQ(
        split("https://" + std::string(SOURCE_AUTH_USER_MAX_LEN - 1, 'u') + ":p@host/").result,
        SOURCE_AUTH_SPLIT);
    EXPECT_EQ(split("https://u:p@host/" + std::string(SOURCE_AUTH_URL_MAX_LEN, 'x')).result,
              SOURCE_AUTH_INVALID);
}

TEST(SourceAuth, NullUrlIsNoLogin)
{
    char url[16], user[16], pass[16];
    EXPECT_EQ(
        source_auth_split_url(nullptr, url, sizeof(url), user, sizeof(user), pass, sizeof(pass)),
        SOURCE_AUTH_NO_LOGIN);
}

TEST(SourceAuth, HttpsDetectionIsCaseInsensitive)
{
    EXPECT_TRUE(source_auth_is_https("https://host/"));
    EXPECT_TRUE(source_auth_is_https("HTTPS://host/"));
    EXPECT_FALSE(source_auth_is_https("http://host/"));
    EXPECT_FALSE(source_auth_is_https("ftp://host/"));
    EXPECT_FALSE(source_auth_is_https(""));
    EXPECT_FALSE(source_auth_is_https(nullptr));
}
