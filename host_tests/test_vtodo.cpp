#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" {
#include "vtodo.h"
}

namespace
{

class Vtodo : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        setenv("TZ", "UTC0", 1);
        tzset();
    }

    todo_list_t parse(const std::string &ics)
    {
        std::string buf = ics;  // std::string keeps a NUL after size()
        todo_list_t list;
        EXPECT_EQ(vtodo_parse(&buf[0], buf.size(), &list), ESP_OK);
        return list;
    }

    static std::string todo(const std::string &lines)
    {
        return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VTODO\r\n" + lines +
               "END:VTODO\r\nEND:VCALENDAR\r\n";
    }
};

}  // namespace

TEST_F(Vtodo, TextPriorityAndDueDate)
{
    todo_list_t l =
        parse(todo("UID:1\r\nSUMMARY:Buy bread\r\nPRIORITY:1\r\nDUE;VALUE=DATE:20261005\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "Buy bread");
    EXPECT_EQ(l.items[0].priority, 'A');
    EXPECT_STREQ(l.items[0].due_date, "2026-10-05");
    EXPECT_EQ(l.items[0].project_count, 0);
    EXPECT_EQ(l.items[0].context_count, 0);
}

TEST_F(Vtodo, PriorityMapping)
{
    const struct {
        int ical;
        char letter;
    } table[] = {{0, 0},   {1, 'A'}, {2, 'A'}, {3, 'B'}, {4, 'B'}, {5, 'C'},
                 {6, 'D'}, {7, 'D'}, {8, 'D'}, {9, 'D'}, {10, 0}};
    for (const auto &row : table) {
        todo_list_t l = parse(todo("SUMMARY:x\r\nPRIORITY:" + std::to_string(row.ical) + "\r\n"));
        ASSERT_EQ(l.count, 1) << row.ical;
        EXPECT_EQ(l.items[0].priority, row.letter) << row.ical;
    }
    todo_list_t none = parse(todo("SUMMARY:x\r\n"));
    EXPECT_EQ(none.items[0].priority, 0);
}

TEST_F(Vtodo, DueInTheThreeShapes)
{
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE;VALUE=DATE:20261231\r\n")).items[0].due_date,
                 "2026-12-31");
    EXPECT_STREQ(
        parse(todo("SUMMARY:a\r\nDUE;TZID=Europe/Berlin:20261005T090000\r\n")).items[0].due_date,
        "2026-10-05");  // written date, no conversion
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261005T090000\r\n")).items[0].due_date,
                 "2026-10-05");
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261005T090000Z\r\n")).items[0].due_date,
                 "2026-10-05");
}

TEST_F(Vtodo, UtcTimeBecomesTheLocalDate)
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // UTC+2 in October
    tzset();
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261005T230000Z\r\n")).items[0].due_date,
                 "2026-10-06");
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261005T120000Z\r\n")).items[0].due_date,
                 "2026-10-05");
    // winter time (UTC+1): 23:30 UTC is the next day as well
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261231T233000Z\r\n")).items[0].due_date,
                 "2027-01-01");
    setenv("TZ", "UTC0", 1);
    tzset();
}

TEST_F(Vtodo, BadDueIsNoDue)
{
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:garbage\r\n")).items[0].due_date, "");
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:20261305\r\n")).items[0].due_date, "");  // month 13
    EXPECT_STREQ(parse(todo("SUMMARY:a\r\nDUE:2026\r\n")).items[0].due_date, "");
}

TEST_F(Vtodo, FinishedItemsAreLeftOut)
{
    std::string ics = todo("SUMMARY:open\r\nSTATUS:NEEDS-ACTION\r\n") +
                      todo("SUMMARY:done by status\r\nSTATUS:COMPLETED\r\n") +
                      todo("SUMMARY:done by time\r\nCOMPLETED:20261001T100000Z\r\n") +
                      todo("SUMMARY:done by percent\r\nPERCENT-COMPLETE:100\r\n") +
                      todo("SUMMARY:cancelled\r\nSTATUS:CANCELLED\r\n") +
                      todo("SUMMARY:half\r\nPERCENT-COMPLETE:50\r\nSTATUS:IN-PROCESS\r\n");
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, 2);
    std::vector<std::string> texts = {l.items[0].text, l.items[1].text};
    EXPECT_NE(std::find(texts.begin(), texts.end(), "open"), texts.end());
    EXPECT_NE(std::find(texts.begin(), texts.end(), "half"), texts.end());
}

TEST_F(Vtodo, SortedByDueThenPriorityThenArrival)
{
    std::string ics = todo("SUMMARY:no due, prio A\r\nPRIORITY:1\r\n") +
                      todo("SUMMARY:due late\r\nDUE;VALUE=DATE:20261101\r\n") +
                      todo("SUMMARY:due soon, low\r\nDUE;VALUE=DATE:20261003\r\nPRIORITY:9\r\n") +
                      todo("SUMMARY:due soon, high\r\nDUE;VALUE=DATE:20261003\r\nPRIORITY:1\r\n") +
                      todo("SUMMARY:due soon, none first\r\nDUE;VALUE=DATE:20261003\r\n") +
                      todo("SUMMARY:due soon, none second\r\nDUE;VALUE=DATE:20261003\r\n") +
                      todo("SUMMARY:no due, none\r\n");
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, 7);
    const char *expected[] = {"due soon, high",        "due soon, low", "due soon, none first",
                              "due soon, none second", "due late",      "no due, prio A",
                              "no due, none"};
    // priorities sort A < D < none; equal ones keep their arrival order
    EXPECT_STREQ(l.items[0].text, expected[0]);
    EXPECT_STREQ(l.items[1].text, expected[1]);
    EXPECT_STREQ(l.items[2].text, expected[2]);
    EXPECT_STREQ(l.items[3].text, expected[3]);
    EXPECT_STREQ(l.items[4].text, expected[4]);
    EXPECT_STREQ(l.items[5].text, expected[5]);
    EXPECT_STREQ(l.items[6].text, expected[6]);
}

TEST_F(Vtodo, KeepsTheBestOnesWhenThereAreMoreThanFit)
{
    std::string ics;
    for (int i = 0; i < 60; i++) {
        char day[16];
        snprintf(day, sizeof(day), "202612%02d", 1 + (i * 7) % 28);
        ics += todo("SUMMARY:task " + std::to_string(i) + "\r\nDUE;VALUE=DATE:" + day + "\r\n");
    }
    ics += todo("SUMMARY:the very first\r\nDUE;VALUE=DATE:20261001\r\n");
    ics += todo("SUMMARY:undated\r\n");
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, TODO_MAX_ITEMS);
    EXPECT_STREQ(l.items[0].text, "the very first");
    for (int i = 1; i < l.count; i++) {
        EXPECT_LE(strcmp(l.items[i - 1].due_date, l.items[i].due_date), 0);
        EXPECT_STRNE(l.items[i].text, "undated");  // undated ones do not push dated ones out
    }
}

TEST_F(Vtodo, FoldedLinesAndTextEscapes)
{
    todo_list_t l =
        parse(todo("SUMMARY:Call the plumber\\, about the\r\n  kitchen\\; sink\\nagain\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "Call the plumber, about the kitchen; sink again");
}

TEST_F(Vtodo, LineEndingsCrlfLfAndFoldWithTab)
{
    std::string lf = "BEGIN:VCALENDAR\nBEGIN:VTODO\nSUMMARY:one\n\ttwo\nEND:VTODO\nEND:VCALENDAR\n";
    todo_list_t l = parse(lf);
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "onetwo");
}

TEST_F(Vtodo, UmlautsAreTransliterated)
{
    todo_list_t l = parse(todo("SUMMARY:Brötchen für Käse\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "Broetchen fuer Kaese");  // the bitmap font is ASCII-only
}

TEST_F(Vtodo, TheAlarmInsideDoesNotOverrideTheItem)
{
    std::string ics = todo(
        "SUMMARY:Real title\r\nPRIORITY:1\r\nBEGIN:VALARM\r\nACTION:DISPLAY\r\nSUMMARY:alarm "
        "text\r\n"
        "DESCRIPTION:alarm text\r\nTRIGGER:-PT15M\r\nEND:VALARM\r\nDUE;VALUE=DATE:20261010\r\n");
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "Real title");
    EXPECT_EQ(l.items[0].priority, 'A');
    EXPECT_STREQ(l.items[0].due_date, "2026-10-10");
}

TEST_F(Vtodo, RepeatingTodoIsListedOnce)
{
    todo_list_t l =
        parse(todo("SUMMARY:Water the plants\r\nRRULE:FREQ=WEEKLY\r\nDUE;VALUE=DATE:20261005\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "Water the plants");
}

TEST_F(Vtodo, EventsAndOtherComponentsAreIgnored)
{
    std::string ics =
        "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:An "
        "event\r\nDTSTART:20261005T090000Z\r\nEND:VEVENT\r\n"
        "BEGIN:VJOURNAL\r\nSUMMARY:A note\r\nEND:VJOURNAL\r\nEND:VCALENDAR\r\n" +
        todo("SUMMARY:A task\r\n");
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "A task");
}

TEST_F(Vtodo, SeveralCalendarObjectsFollowEachOther)
{
    todo_list_t l = parse(todo("SUMMARY:first\r\nDUE;VALUE=DATE:20261002\r\n") + "\n" +
                          todo("SUMMARY:second\r\nDUE;VALUE=DATE:20261001\r\n"));
    ASSERT_EQ(l.count, 2);
    EXPECT_STREQ(l.items[0].text, "second");
    EXPECT_STREQ(l.items[1].text, "first");
}

TEST_F(Vtodo, EmptySummaryIsSkippedAndNothingIsNotAnError)
{
    EXPECT_EQ(parse(todo("PRIORITY:1\r\n")).count, 0);
    EXPECT_EQ(parse("").count, 0);
    EXPECT_EQ(parse("BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n").count, 0);
}

TEST_F(Vtodo, PropertiesInAnyCaseAndOrder)
{
    todo_list_t l =
        parse(todo("due;value=date:20261007\r\npriority:5\r\nsummary:lower case names\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "lower case names");
    EXPECT_EQ(l.items[0].priority, 'C');
    EXPECT_STREQ(l.items[0].due_date, "2026-10-07");
}

TEST_F(Vtodo, VeryLongSummaryIsCut)
{
    todo_list_t l = parse(todo("SUMMARY:" + std::string(3000, 'x') + "\r\n"));
    ASSERT_EQ(l.count, 1);
    EXPECT_EQ(strlen(l.items[0].text), (size_t) TODO_LINE_MAX_LEN - 1);
}

TEST_F(Vtodo, UnterminatedTodoAtTheEndIsDropped)
{
    // a response cut off in the middle of a to-do: what is complete is kept
    std::string ics =
        todo("SUMMARY:whole\r\n") + "BEGIN:VCALENDAR\r\nBEGIN:VTODO\r\nSUMMARY:cut off\r\n";
    todo_list_t l = parse(ics);
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.items[0].text, "whole");
}

TEST_F(Vtodo, NullArguments)
{
    todo_list_t l;
    char buf[1] = {0};
    EXPECT_EQ(vtodo_parse(buf, 0, nullptr), ESP_ERR_INVALID_ARG);
    EXPECT_EQ(vtodo_parse(nullptr, 0, &l), ESP_ERR_INVALID_ARG);
}
