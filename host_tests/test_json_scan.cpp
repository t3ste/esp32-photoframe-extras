#include <gtest/gtest.h>

#include <cstring>
#include <string>

extern "C" {
#include "json_scan.h"
}

TEST(JsonScan, SkipBlanks)
{
    const char *text = " \t\r\n  x ";
    EXPECT_EQ(json_skip_blanks(text), text + 6);
    EXPECT_STREQ(json_skip_blanks("x"), "x");
    EXPECT_STREQ(json_skip_blanks("   "), "");
    EXPECT_STREQ(json_skip_blanks(""), "");
}

TEST(JsonScan, ObjectEndOfAFlatObject)
{
    const char *text = "{\"a\":1}, {\"b\":2}";
    const char *end = json_object_end(text);
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(*end, '}');
    EXPECT_EQ(end - text, 6);
}

TEST(JsonScan, ObjectEndOfANestedObject)
{
    const char *text = "{\"a\":{\"b\":{}},\"c\":[{}]} tail";
    const char *end = json_object_end(text);
    ASSERT_NE(end, nullptr);
    EXPECT_STREQ(end, "} tail");
}

TEST(JsonScan, BracesInsideStringsDoNotCount)
{
    const char *text = "{\"a\":\"}{\",\"b\":\"{{{\"}x";
    const char *end = json_object_end(text);
    ASSERT_NE(end, nullptr);
    EXPECT_STREQ(end, "}x");
}

TEST(JsonScan, EscapedQuotesDoNotEndAString)
{
    // the string is: a"} (a quote and a brace inside)
    const char *text = "{\"a\":\"a\\\"}\"}!";
    const char *end = json_object_end(text);
    ASSERT_NE(end, nullptr);
    EXPECT_STREQ(end, "}!");
    // an escaped backslash before the closing quote does end it
    const char *text2 = "{\"a\":\"x\\\\\"}?";
    const char *end2 = json_object_end(text2);
    ASSERT_NE(end2, nullptr);
    EXPECT_STREQ(end2, "}?");
}

TEST(JsonScan, CutOffTextHasNoEnd)
{
    EXPECT_EQ(json_object_end("{\"a\":1"), nullptr);
    EXPECT_EQ(json_object_end("{\"a\":{\"b\":1}"), nullptr);
    EXPECT_EQ(json_object_end("{\"a\":\"unterminated"), nullptr);
    EXPECT_EQ(json_object_end("{\"a\":\"ends with a backslash\\"), nullptr);
    EXPECT_EQ(json_object_end(""), nullptr);
    EXPECT_EQ(json_object_end("no object at all"), nullptr);
}

TEST(JsonScan, AnObjectDoesNotRunPastItsOwnEnd)
{
    std::string text = "{}{}{}";
    const char *end = json_object_end(text.c_str());
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end - text.c_str(), 1);
}
