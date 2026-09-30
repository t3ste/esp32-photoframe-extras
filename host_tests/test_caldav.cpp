#include <gtest/gtest.h>

extern "C" {
#include "caldav.h"
}

#include <cstring>
#include <string>

namespace
{

// Runs the in-place extraction on a copy and returns what is left.
std::string extract(const std::string &xml)
{
    std::string buf = xml;  // has a NUL at buf[size()]
    size_t n = caldav_extract_calendar_data(&buf[0], buf.size());
    return std::string(buf.c_str(), n);
}

}  // namespace

TEST(Caldav, RecognisesItsSchemes)
{
    EXPECT_TRUE(caldav_is_url("caldavs://cal.example.org/dav/me/cal/"));
    EXPECT_TRUE(caldav_is_url("caldav://192.168.1.5:5232/me/cal/"));
    EXPECT_TRUE(caldav_is_url("CalDAVs://cal.example.org/"));
    EXPECT_FALSE(caldav_is_url("https://cal.example.org/dav/"));
    EXPECT_FALSE(caldav_is_url("webcal://cal.example.org/a.ics"));
    EXPECT_FALSE(caldav_is_url("caldav.example.org/x"));
    EXPECT_FALSE(caldav_is_url(""));
    EXPECT_FALSE(caldav_is_url(nullptr));
}

TEST(Caldav, ResolvesTheSchemes)
{
    char buf[128];
    EXPECT_STREQ(caldav_resolve_url("caldavs://u:p@cal.example.org/dav/", buf, sizeof(buf)),
                 "https://u:p@cal.example.org/dav/");
    EXPECT_STREQ(caldav_resolve_url("caldav://192.168.1.5:5232/me/", buf, sizeof(buf)),
                 "http://192.168.1.5:5232/me/");
    EXPECT_STREQ(caldav_resolve_url("CALDAVS://Cal.Example.org/A", buf, sizeof(buf)),
                 "https://Cal.Example.org/A");
    const char *plain = "https://cal.example.org/a.ics";
    EXPECT_EQ(caldav_resolve_url(plain, buf, sizeof(buf)), plain);

    char tiny[10];
    EXPECT_EQ(caldav_resolve_url("caldavs://cal.example.org/dav/", tiny, sizeof(tiny)), nullptr);
}

TEST(Caldav, ReportBodyCarriesTheRange)
{
    char body[CALDAV_REPORT_BODY_MAX];
    int n = caldav_build_report_body(body, sizeof(body), 1767225600, 1767225600 + 3 * 86400, true);
    ASSERT_GT(n, 0);
    EXPECT_EQ(static_cast<size_t>(n), strlen(body));
    std::string s = body;
    EXPECT_NE(s.find("<c:time-range start=\"20260101T000000Z\" end=\"20260104T000000Z\"/>"),
              std::string::npos);
    EXPECT_NE(s.find("<c:expand start=\"20260101T000000Z\" end=\"20260104T000000Z\"/>"),
              std::string::npos);
    EXPECT_NE(s.find("name=\"VEVENT\""), std::string::npos);
    EXPECT_NE(s.find("urn:ietf:params:xml:ns:caldav"), std::string::npos);
}

TEST(Caldav, ReportBodyWithoutExpand)
{
    char body[CALDAV_REPORT_BODY_MAX];
    ASSERT_GT(caldav_build_report_body(body, sizeof(body), 0, 86400, false), 0);
    std::string s = body;
    EXPECT_EQ(s.find("expand"), std::string::npos);
    EXPECT_NE(s.find("<c:time-range start=\"19700101T000000Z\" end=\"19700102T000000Z\"/>"),
              std::string::npos);
}

TEST(Caldav, ReportBodyBufferTooSmall)
{
    char tiny[64];
    EXPECT_EQ(caldav_build_report_body(tiny, sizeof(tiny), 0, 86400, true), -1);
}

TEST(Caldav, ExtractsEveryCalendarDataElement)
{
    std::string xml =
        "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" "
        "xmlns:cal=\"urn:ietf:params:xml:ns:caldav\">"
        "<d:response><d:href>/a.ics</d:href><d:propstat><d:prop>"
        "<d:getetag>\"1\"</d:getetag>"
        "<cal:calendar-data>BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:One\r\nEND:VEVENT\r\n"
        "END:VCALENDAR\r\n</cal:calendar-data></d:prop></d:propstat></d:response>"
        "<d:response><d:href>/b.ics</d:href><d:propstat><d:prop>"
        "<cal:calendar-data>BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:Two\r\nEND:VEVENT\r\n"
        "END:VCALENDAR\r\n</cal:calendar-data></d:prop></d:propstat></d:response>"
        "</d:multistatus>";
    std::string out = extract(xml);
    EXPECT_NE(out.find("SUMMARY:One"), std::string::npos);
    EXPECT_NE(out.find("SUMMARY:Two"), std::string::npos);
    EXPECT_EQ(out.find("href"), std::string::npos);
    EXPECT_EQ(out.find("getetag"), std::string::npos);
    EXPECT_EQ(out.find('<'), std::string::npos);
    // two separate calendar objects, one after the other
    size_t first_end = out.find("END:VCALENDAR");
    ASSERT_NE(first_end, std::string::npos);
    EXPECT_NE(out.find("BEGIN:VCALENDAR", first_end), std::string::npos);
}

TEST(Caldav, DecodesEntitiesAndCdata)
{
    EXPECT_EQ(
        extract("<c:calendar-data>A &amp; B &lt;x&gt; &quot;q&quot; it&apos;s</c:calendar-data>"),
        "A & B <x> \"q\" it's");
    // Sabre writes the CR of a CRLF as a numeric entity
    EXPECT_EQ(extract("<calendar-data>L1&#13;\nL2&#xD;\n</calendar-data>"), "L1\r\nL2\r\n");
    EXPECT_EQ(extract("<C:calendar-data>Caf&#233; &#x1F600;</C:calendar-data>"),
              "Caf\xC3\xA9 \xF0\x9F\x98\x80");
    EXPECT_EQ(extract("<c:calendar-data><![CDATA[SUMMARY:a < b & c]]></c:calendar-data>"),
              "SUMMARY:a < b & c");
    EXPECT_EQ(extract("<c:calendar-data>x<![CDATA[<y>]]>&amp;z</c:calendar-data>"), "x<y>&z");
}

TEST(Caldav, KeepsWhatIsNotAnEntity)
{
    EXPECT_EQ(extract("<c:calendar-data>R&D &nbsp; &#; &#xZZ; &amp</c:calendar-data>"),
              "R&D &nbsp; &#; &#xZZ; &amp");
    EXPECT_EQ(extract("<c:calendar-data>a&#0;b</c:calendar-data>"), "a&#0;b");  // no NUL
}

TEST(Caldav, MatchesTheLocalNameOnly)
{
    EXPECT_EQ(extract("<x:calendar-data>in</x:calendar-data>"), "in");
    EXPECT_EQ(extract("<calendar-data xmlns=\"urn:ietf:params:xml:ns:caldav\">in</calendar-data>"),
              "in");
    EXPECT_EQ(extract("<c:calendar-data-x>no</c:calendar-data-x>"), "");
    EXPECT_EQ(extract("<c:my-calendar-data>no</c:my-calendar-data>"), "");
    EXPECT_EQ(extract("<c:calendar-data/>"), "");  // self-closing: nothing in it
    EXPECT_EQ(extract("<c:calendar-data></c:calendar-data>"), "");
}

TEST(Caldav, NothingToCollect)
{
    EXPECT_EQ(extract(""), "");
    EXPECT_EQ(extract("<d:multistatus xmlns:d=\"DAV:\"></d:multistatus>"), "");
    EXPECT_EQ(extract("plain text, no xml"), "");
    EXPECT_EQ(caldav_extract_calendar_data(nullptr, 5), 0u);
}

TEST(Caldav, TruncatedResponseKeepsWhatWasComplete)
{
    // cut in the middle of the second element: its text so far is kept, nothing crashes
    std::string out = extract("<c:calendar-data>ONE</c:calendar-data><c:calendar-data>TW");
    EXPECT_EQ(out, "ONE\nTW");
    EXPECT_EQ(extract("<c:calendar-data>ONE</c:calendar-data><c:calendar-da"), "ONE");
    EXPECT_EQ(extract("<c:calendar-data>A<![CDATA[B"), "AB");
}

TEST(Caldav, HugeResponseIsCompactedInPlace)
{
    std::string item = "<c:calendar-data>" + std::string(1000, 'x') + "</c:calendar-data>";
    std::string xml;
    for (int i = 0; i < 200; i++) {
        xml += "<d:response>" + item + "</d:response>";
    }
    std::string out = extract(xml);
    EXPECT_EQ(out.size(), 200u * 1000u + 199u);  // 200 texts plus the separators
}

TEST(Caldav, TodoBodyAsksForOpenTodos)
{
    char body[CALDAV_REPORT_BODY_MAX];
    int n = caldav_build_todo_report_body(body, sizeof(body), true);
    ASSERT_GT(n, 0);
    EXPECT_EQ(static_cast<size_t>(n), strlen(body));
    std::string s = body;
    EXPECT_NE(s.find("<c:comp-filter name=\"VTODO\">"), std::string::npos);
    EXPECT_NE(s.find("<c:prop-filter name=\"COMPLETED\"><c:is-not-defined/></c:prop-filter>"),
              std::string::npos);
    EXPECT_EQ(s.find("time-range"), std::string::npos);  // to-dos are not asked for by time
    EXPECT_EQ(s.find("expand"), std::string::npos);
}

TEST(Caldav, TodoBodyForEverything)
{
    char body[CALDAV_REPORT_BODY_MAX];
    ASSERT_GT(caldav_build_todo_report_body(body, sizeof(body), false), 0);
    std::string s = body;
    EXPECT_NE(s.find("name=\"VTODO\""), std::string::npos);
    EXPECT_EQ(s.find("prop-filter"), std::string::npos);
}

TEST(Caldav, TodoBodyBufferTooSmall)
{
    char tiny[64];
    EXPECT_EQ(caldav_build_todo_report_body(tiny, sizeof(tiny), true), -1);
}
