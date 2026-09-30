#include <gtest/gtest.h>

#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" {
#include "fact_pack.h"
}

namespace
{

std::vector<fact_t> parse(const std::string &text, int max = FACT_MAX, int *skipped = nullptr)
{
    std::vector<fact_t> facts(max > 0 ? max : 1);
    int n = fact_pack_parse(text.c_str(), facts.data(), max, skipped);
    facts.resize(n);
    return facts;
}

}  // namespace

TEST(FactPack, ALineIsAFactOrTitleFactOrTitleFactQuestion)
{
    auto facts = parse(
        "Honey keeps.\n"
        "Animals|Owls turn their heads a long way.\n"
        "Space|A day on Venus is long.|Which way does it spin?\n");
    ASSERT_EQ(facts.size(), 3u);
    EXPECT_STREQ(facts[0].title, "");
    EXPECT_STREQ(facts[0].text, "Honey keeps.");
    EXPECT_STREQ(facts[0].question, "");
    EXPECT_STREQ(facts[1].title, "Animals");
    EXPECT_STREQ(facts[1].text, "Owls turn their heads a long way.");
    EXPECT_STREQ(facts[2].title, "Space");
    EXPECT_STREQ(facts[2].question, "Which way does it spin?");
}

TEST(FactPack, FurtherBarsBelongToTheQuestion)
{
    auto facts = parse("T|Fact|Is it a | or a bar?\n");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_STREQ(facts[0].question, "Is it a | or a bar?");
}

TEST(FactPack, BlankLinesCommentsAndLineEndingsAreHandled)
{
    int skipped = -1;
    auto facts = parse(
        "# my facts\r\n\r\n   \r\nFirst fact\r\n\t# another comment\r\n  Second|fact  \r\nLast",
        FACT_MAX, &skipped);
    ASSERT_EQ(facts.size(), 3u);
    EXPECT_EQ(skipped, 0);
    EXPECT_STREQ(facts[0].text, "First fact");
    EXPECT_STREQ(facts[1].title, "Second");
    EXPECT_STREQ(facts[1].text, "fact");
    EXPECT_STREQ(facts[2].text, "Last");  // no line end at the end of the file
}

TEST(FactPack, ALineWithoutFactTextIsSkippedAndCounted)
{
    int skipped = 0;
    auto facts = parse("Title only|\n|\nGood\n||Q only\n", FACT_MAX, &skipped);
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_STREQ(facts[0].text, "Good");
    EXPECT_EQ(skipped, 3);
}

TEST(FactPack, ATextEditorsByteOrderMarkIsIgnored)
{
    auto facts = parse(
        "\xEF\xBB\xBF"
        "First\nSecond\n");
    ASSERT_EQ(facts.size(), 2u);
    EXPECT_STREQ(facts[0].text, "First");
}

TEST(FactPack, UmlautsSurvive)
{
    auto facts = parse(
        "K\xC3\xBC"
        "chenchemie|Honig verdirbt fast nie: gr\xC3\xB6\xC3\x9F"
        "te Haltbarkeit.\n");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_STREQ(facts[0].title,
                 "K\xC3\xBC"
                 "chenchemie");
    EXPECT_STREQ(facts[0].text,
                 "Honig verdirbt fast nie: gr\xC3\xB6\xC3\x9F"
                 "te Haltbarkeit.");
}

TEST(FactPack, TooLongFieldsAreCutAtACharacterBoundary)
{
    std::string title(FACT_TITLE_MAX - 2, 'a');
    title += "\xC3\xA4\xC3\xA4\xC3\xA4";  // the cut would fall inside the second umlaut
    std::string text(FACT_TEXT_MAX + 40, 'b');
    std::string question(FACT_QUESTION_MAX + 40, 'c');
    auto facts = parse(title + "|" + text + "|" + question + "\n");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(strlen(facts[0].title), (size_t) FACT_TITLE_MAX - 2);  // no half umlaut at the end
    EXPECT_EQ(facts[0].title[strlen(facts[0].title) - 1], 'a');
    EXPECT_EQ(strlen(facts[0].text), (size_t) FACT_TEXT_MAX - 1);
    EXPECT_EQ(strlen(facts[0].question), (size_t) FACT_QUESTION_MAX - 1);
}

TEST(FactPack, TheLimitStopsTheListAndCountsTheRest)
{
    std::string pack;
    for (int i = 0; i < 10; i++) {
        pack += "Fact " + std::to_string(i) + "\n";
    }
    int skipped = 0;
    auto facts = parse(pack, 4, &skipped);
    ASSERT_EQ(facts.size(), 4u);
    EXPECT_EQ(skipped, 6);
    EXPECT_STREQ(facts[3].text, "Fact 3");
}

TEST(FactPack, NothingToParse)
{
    fact_t out[2];
    int skipped = 5;
    EXPECT_EQ(fact_pack_parse(nullptr, out, 2, &skipped), 0);
    EXPECT_EQ(skipped, 0);
    EXPECT_EQ(fact_pack_parse("", out, 2, &skipped), 0);
    EXPECT_EQ(fact_pack_parse("Fact\n", out, 0, &skipped), 0);
    EXPECT_EQ(fact_pack_parse("Fact\n", out, 2, nullptr), 1);
}

TEST(FactPack, DayNumbersAgreeWithTheCLibrary)
{
    EXPECT_EQ(fact_day_number(1970, 1, 1), 0);
    EXPECT_EQ(fact_day_number(1970, 1, 2), 1);
    EXPECT_EQ(fact_day_number(1969, 12, 31), -1);
    EXPECT_EQ(fact_day_number(2000, 3, 1), 11017);
    for (int year = 1970; year <= 2100; year++) {
        for (int month = 1; month <= 12; month++) {
            for (int day : {1, 15, 28}) {
                struct tm tm_value;
                memset(&tm_value, 0, sizeof(tm_value));
                tm_value.tm_year = year - 1900;
                tm_value.tm_mon = month - 1;
                tm_value.tm_mday = day;
                tm_value.tm_hour = 12;
                long expected = (long) (timegm(&tm_value) / 86400);
                ASSERT_EQ(fact_day_number(year, month, day), expected)
                    << year << "-" << month << "-" << day;
            }
        }
    }
    // the leap day
    EXPECT_EQ(fact_day_number(2024, 3, 1) - fact_day_number(2024, 2, 28), 2);
    EXPECT_EQ(fact_day_number(2100, 3, 1) - fact_day_number(2100, 2, 28),
              1);  // 2100 is no leap year
}

TEST(FactPack, TheFactOfTheDayIsStableAndMovesOnEachDay)
{
    for (int count : {1, 2, 7, 24, 64}) {
        std::vector<int> seen(count, 0);
        for (long day = 20000; day < 20000 + count; day++) {
            int index = fact_pick_index(day, count);
            ASSERT_GE(index, 0);
            ASSERT_LT(index, count);
            EXPECT_EQ(fact_pick_index(day, count), index);  // the same all day
            seen[index]++;
            EXPECT_EQ(fact_pick_index(day + count, count), index);  // and after a full round
        }
        for (int n : seen) {
            EXPECT_EQ(n, 1) << "count " << count;  // every fact once in a round
        }
    }
    EXPECT_EQ(fact_pick_index(-1, 5), 4);  // days before 1970
    EXPECT_EQ(fact_pick_index(-5, 5), 0);
    EXPECT_EQ(fact_pick_index(10, 0), -1);
    EXPECT_EQ(fact_pick_index(10, -3), -1);
}

TEST(FactPack, TheBuiltInFactsAreCompleteInBothLanguages)
{
    ASSERT_GE(fact_builtin_count(), 20);
    int with_question = 0, german_umlauts = 0;
    for (int i = 0; i < fact_builtin_count(); i++) {
        const fact_t *en = fact_builtin(i, false);
        const fact_t *de = fact_builtin(i, true);
        ASSERT_NE(en, nullptr) << i;
        ASSERT_NE(de, nullptr) << i;
        for (const fact_t *f : {en, de}) {
            EXPECT_GT(strlen(f->text), 20u) << i;
            EXPECT_LT(strlen(f->text), (size_t) FACT_TEXT_MAX) << i;
            EXPECT_GT(strlen(f->title), 0u) << i;
            EXPECT_LT(strlen(f->title), (size_t) FACT_TITLE_MAX) << i;
            EXPECT_LT(strlen(f->question), (size_t) FACT_QUESTION_MAX) << i;
        }
        EXPECT_STRNE(en->text, de->text) << i;
        // a fact has a question in both languages or in neither
        EXPECT_EQ(en->question[0] == '\0', de->question[0] == '\0') << i;
        with_question += en->question[0] != '\0';
        for (const unsigned char *c = (const unsigned char *) de->text; *c; c++) {
            if (*c == 0xC3) {
                german_umlauts++;
                break;
            }
        }
    }
    EXPECT_GE(with_question, 5);
    EXPECT_GE(german_umlauts, 8);  // the German pack uses real umlauts
    EXPECT_EQ(fact_builtin(-1, false), nullptr);
    EXPECT_EQ(fact_builtin(fact_builtin_count(), true), nullptr);
}

TEST(FactPack, BuiltInFactsAreNotRepeatedWithinTheirOwnList)
{
    for (int i = 0; i < fact_builtin_count(); i++) {
        for (int j = i + 1; j < fact_builtin_count(); j++) {
            EXPECT_STRNE(fact_builtin(i, false)->text, fact_builtin(j, false)->text)
                << i << " " << j;
        }
    }
}
