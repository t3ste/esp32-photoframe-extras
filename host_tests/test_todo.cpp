#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "fake_http_fetch.h"
#include "todo.h"
}

namespace
{

todo_list_t parse(const char *body)
{
    todo_list_t out;
    todo_parse(body, strlen(body), &out);
    return out;
}

}  // namespace

TEST(TodoParse, PlainTaskNoMetadata)
{
    todo_list_t out = parse("Buy milk\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].priority, 0);
    EXPECT_STREQ(out.items[0].text, "Buy milk");
    EXPECT_STREQ(out.items[0].due_date, "");
}

TEST(TodoParse, PriorityMarker)
{
    todo_list_t out = parse("(A) Call Mom\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].priority, 'A');
    EXPECT_STREQ(out.items[0].text, "Call Mom");
}

TEST(TodoParse, CreationDateSkippedFromText)
{
    todo_list_t out = parse("(A) 2011-03-02 Call Mom\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].priority, 'A');
    EXPECT_STREQ(out.items[0].text, "Call Mom");
}

TEST(TodoParse, ProjectsAndContexts)
{
    todo_list_t out = parse("(A) 2011-03-02 Call Mom +Family @phone due:2011-03-05\n");
    ASSERT_EQ(out.count, 1);
    const todo_item_t &item = out.items[0];
    EXPECT_STREQ(item.text, "Call Mom");
    ASSERT_EQ(item.project_count, 1);
    EXPECT_STREQ(item.projects[0], "Family");
    ASSERT_EQ(item.context_count, 1);
    EXPECT_STREQ(item.contexts[0], "phone");
    EXPECT_STREQ(item.due_date, "2011-03-05");
}

TEST(TodoParse, MultipleProjectsAndContexts)
{
    todo_list_t out = parse("Plan +Trip +Vacation @home @weekend\n");
    ASSERT_EQ(out.count, 1);
    const todo_item_t &item = out.items[0];
    ASSERT_EQ(item.project_count, 2);
    EXPECT_STREQ(item.projects[0], "Trip");
    EXPECT_STREQ(item.projects[1], "Vacation");
    ASSERT_EQ(item.context_count, 2);
    EXPECT_STREQ(item.contexts[0], "home");
    EXPECT_STREQ(item.contexts[1], "weekend");
}

TEST(TodoParse, CompletedTaskExcludedByDefault)
{
    todo_list_t out = parse(
        "x 2011-03-03 2011-03-02 Review report +Project @context\n"
        "Still open task\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.items[0].text, "Still open task");
}

TEST(TodoParse, BlankAndCommentishLinesSkipped)
{
    todo_list_t out = parse("\n   \nReal task\n\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.items[0].text, "Real task");
}

TEST(TodoParse, MalformedPriorityDegradesToPlainText)
{
    // Lowercase letter inside parens isn't a valid priority marker - the
    // whole thing should just become part of the text, not crash or be
    // silently dropped.
    todo_list_t out = parse("(a) not a real priority\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].priority, 0);
    EXPECT_STREQ(out.items[0].text, "(a) not a real priority");
}

TEST(TodoParse, MalformedDateDegradesToPlainText)
{
    todo_list_t out = parse("(A) not-a-date rest of text\n");
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].priority, 'A');
    EXPECT_STREQ(out.items[0].text, "not-a-date rest of text");
}

TEST(TodoParse, EmptyFileYieldsNoItems)
{
    todo_list_t out;
    esp_err_t err = todo_parse("", 0, &out);
    EXPECT_EQ(err, ESP_ERR_INVALID_ARG);
    EXPECT_EQ(out.count, 0);
}

TEST(TodoParse, AllCompletedYieldsNoItemsButNotAnError)
{
    todo_list_t out = parse("x 2011-01-01 done one\nx 2011-01-02 done two\n");
    EXPECT_EQ(out.count, 0);
}

TEST(TodoParse, CrlfLineEndingsTolerated)
{
    todo_list_t out = parse("(A) First\r\nSecond\r\n");
    ASSERT_EQ(out.count, 2);
    EXPECT_STREQ(out.items[0].text, "First");
    EXPECT_STREQ(out.items[1].text, "Second");
}

TEST(TodoParse, NoTrailingNewlineOnLastLine)
{
    todo_list_t out = parse("Only line, no trailing newline");
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.items[0].text, "Only line, no trailing newline");
}

TEST(TodoParse, CapsAtMaxItems)
{
    std::string body;
    for (int i = 0; i < TODO_MAX_ITEMS + 5; i++) {
        body += "Task\n";
    }
    todo_list_t out = parse(body.c_str());
    EXPECT_EQ(out.count, TODO_MAX_ITEMS);
}

TEST(TodoParse, TagCountCappedWithoutOverflow)
{
    std::string body = "Overloaded";
    for (int i = 0; i < TODO_TAG_MAX + 5; i++) {
        body += " +P" + std::to_string(i);
    }
    body += "\n";
    todo_list_t out = parse(body.c_str());
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.items[0].project_count, TODO_TAG_MAX);
}

TEST(TodoParse, NullBodyIsInvalidArg)
{
    todo_list_t out;
    esp_err_t err = todo_parse(nullptr, 0, &out);
    EXPECT_EQ(err, ESP_ERR_INVALID_ARG);
    EXPECT_EQ(out.count, 0);
}

TEST(TodoParse, NullOutIsInvalidArg)
{
    esp_err_t err = todo_parse("Task\n", 5, nullptr);
    EXPECT_EQ(err, ESP_ERR_INVALID_ARG);
}

#if FEATURE_CALDAV_TODO
// A CalDAV task list (FEATURE_CALDAV_TODO): a caldav(s):// ToDo address is answered by a REPORT for
// the VTODO components; http_fetch_report() is the scriptable stub of fake_http_fetch.c.
namespace
{

std::string read_fixture(const char *name)
{
    std::string path = std::string(CALDAV_TEST_DATA_DIR) + "/" + name;
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        return "";
    }
    std::string data;
    char chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        data.append(chunk, n);
    }
    std::fclose(fp);
    return data;
}

class TodoCaldav : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        setenv("TZ", "UTC0", 1);
        tzset();
        fake_report_reset();
    }
};

}  // namespace

TEST_F(TodoCaldav, RadicaleTaskListBecomesTheColumn)
{
    std::string xml = read_fixture("radicale-todo-open.xml");
    ASSERT_FALSE(xml.empty());
    fake_report.response = xml.c_str();
    todo_list_t list;
    ASSERT_EQ(todo_fetch("caldavs://u:p@cal.example.org/tester/tasks/", 0, nullptr, nullptr,
                         nullptr, 0, &list),
              ESP_OK);
    // the finished ones (a cancelled one, unlike a completed one, comes back from the server) are
    // dropped, the rest is ordered by due date, then the undated by priority
    ASSERT_EQ(list.count, 6);
    EXPECT_STREQ(list.items[0].text, "With an alarm");
    EXPECT_STREQ(list.items[0].due_date, "2026-10-01");
    EXPECT_EQ(list.items[0].priority, 0);
    EXPECT_STREQ(list.items[1].text, "Water the plants");  // repeats: listed once, as it stands
    EXPECT_EQ(list.items[1].priority, 'D');
    EXPECT_STREQ(list.items[2].text, "Buy bread");
    EXPECT_EQ(list.items[2].priority, 'A');
    EXPECT_STREQ(list.items[2].due_date, "2026-10-03");
    EXPECT_STREQ(list.items[3].text, "Tax return");
    EXPECT_STREQ(list.items[3].due_date, "2026-10-15");  // 12:00 UTC
    EXPECT_EQ(list.items[3].priority, 'C');
    EXPECT_STREQ(list.items[4].text,
                 "Broetchen fuer Kaese und eine ziemlich lange Beschreibung, die ueber zwei Zeilen "
                 "geht");
    EXPECT_EQ(list.items[4].priority, 'B');
    EXPECT_STREQ(list.items[4].due_date, "");
    EXPECT_STREQ(list.items[5].text, "Someday");
}

TEST_F(TodoCaldav, AnswerWithTheFinishedOnesIsFilteredHere)
{
    std::string xml = read_fixture("radicale-todo-all.xml");
    ASSERT_FALSE(xml.empty());
    fake_report.response = xml.c_str();
    fake_report.status[0] = 400;  // a server that does not take the open-only filter
    todo_list_t list;
    ASSERT_EQ(todo_fetch("caldavs://u:p@cal.example.org/tester/tasks/", 0, nullptr, nullptr,
                         nullptr, 0, &list),
              ESP_OK);
    ASSERT_EQ(fake_report.calls, 2);
    EXPECT_NE(std::string(fake_report.body[0]).find("is-not-defined"), std::string::npos);
    EXPECT_EQ(std::string(fake_report.body[1]).find("prop-filter"), std::string::npos);
    EXPECT_NE(std::string(fake_report.body[1]).find("name=\"VTODO\""), std::string::npos);
    EXPECT_EQ(list.count, 6);  // "Already done" (and the cancelled one) are not in it
    for (int i = 0; i < list.count; i++) {
        EXPECT_STRNE(list.items[i].text, "Already done");
        EXPECT_STRNE(list.items[i].text, "Cancelled thing");
    }
}

TEST_F(TodoCaldav, AskedOnceWithTheLoginInTheAddress)
{
    fake_report.response = "<d:multistatus xmlns:d=\"DAV:\"></d:multistatus>";
    todo_list_t list;
    ASSERT_EQ(todo_fetch("caldav://user:pw@192.168.1.5:5232/user/tasks/", 0, nullptr, nullptr,
                         nullptr, 0, &list),
              ESP_OK);
    EXPECT_EQ(fake_report.calls, 1);
    EXPECT_STREQ(fake_report.url[0], "http://user:pw@192.168.1.5:5232/user/tasks/");
    EXPECT_EQ(list.count, 0);  // an empty list is a valid answer
}

TEST_F(TodoCaldav, RefusedLoginAndOtherErrorsAreNotRetried)
{
    for (int status : {401, 403, 404}) {
        fake_report_reset();
        fake_report.response = "";
        fake_report.status[0] = status;
        todo_list_t list;
        EXPECT_NE(
            todo_fetch("caldavs://u:p@cal.example.org/t/", 0, nullptr, nullptr, nullptr, 0, &list),
            ESP_OK)
            << status;
        EXPECT_EQ(fake_report.calls, 1) << status;
        EXPECT_EQ(list.count, 0);
    }
}

TEST_F(TodoCaldav, PlainAddressesStillFetchAFile)
{
    // an https:// todo.txt address is not a CalDAV one: it does not go through the REPORT path
    todo_list_t list;
    todo_fetch("https://example.org/todo.txt", 0, nullptr, nullptr, nullptr, 0, &list);
    EXPECT_EQ(fake_report.calls, 0);
}
#endif
