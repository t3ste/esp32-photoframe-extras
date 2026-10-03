// The text cleaning of the recipe page (main/recipe_text.c): UTF-8 into the one-byte code of the
// fonts, HTML, step numbers, paragraphs, times and amounts. The texts are invented.

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <limits>
#include <string>

extern "C" {
#include "recipe_text.h"
}

namespace
{

std::string clean(const std::string &text, size_t cap = 4096, bool *cut = nullptr)
{
    std::string out(cap, '\0');
    size_t len = recipe_text_clean(text.c_str(), &out[0], cap, cut);
    out.resize(len);
    return out;
}

std::string steps(const std::string &text)
{
    std::string out(4096, '\0');
    out.resize(recipe_text_merge_steps(text.c_str(), &out[0], out.size()));
    return out;
}

std::string paragraphs(const std::string &text)
{
    std::string out(4096, '\0');
    out.resize(recipe_text_paragraphs(text.c_str(), &out[0], out.size()));
    return out;
}

std::string minutes(int value, bool german)
{
    char out[32];
    recipe_text_format_minutes(value, german, out, sizeof(out));
    return out;
}

std::string amount(double value, bool german)
{
    char out[32];
    recipe_text_format_amount(value, german, out, sizeof(out));
    return out;
}

// One byte of the font's code
std::string byte(unsigned char value)
{
    return std::string(1, static_cast<char>(value));
}

}  // namespace

TEST(RecipeClean, PlainAsciiStaysAsItIs)
{
    EXPECT_EQ(clean("Mix the flour, 2 eggs & milk."), "Mix the flour, 2 eggs & milk.");
}

TEST(RecipeClean, GermanLettersBecomeSingleBytes)
{
    EXPECT_EQ(
        clean("\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F"),
        byte(0xE4) + byte(0xF6) + byte(0xFC) + byte(0xC4) + byte(0xD6) + byte(0xDC) + byte(0xDF));
    EXPECT_EQ(clean("caf\xC3\xA9 cr\xC3\xA8me"), "caf" + byte(0xE9) + " cr" + byte(0xE8) + "me");
}

TEST(RecipeClean, TypographicSignsAreInTheFontsCode)
{
    // a German quotation, a dash, the ellipsis, the euro sign, the degree and a fraction
    EXPECT_EQ(clean("\xE2\x80\x9E"
                    "Gut\xE2\x80\x9C \xE2\x80\x93 \xE2\x80\xA6 \xE2\x82\xAC 180 \xC2\xB0"
                    "C \xC2\xBD"),
              byte(0x84) + "Gut" + byte(0x93) + " " + byte(0x96) + " " + byte(0x85) + " " +
                  byte(0x80) + " 180 " + byte(0xB0) + "C " + byte(0xBD));
    EXPECT_EQ(clean("it\xE2\x80\x99s"), "it" + byte(0x92) + "s");
}

TEST(RecipeClean, SignsWithoutAGlyphGetTheirPlainForm)
{
    EXPECT_EQ(clean("\xE2\x85\x93 Tasse"), "1/3 Tasse");
    EXPECT_EQ(clean("\xE2\x85\x9B und \xE2\x85\x9C"), "1/8 und 3/8");
    EXPECT_EQ(clean("a\xE2\x80\x91"
                    "b"),
              "a-b");  // a non-breaking hyphen
    EXPECT_EQ(clean("\xE2\x88\x92"
                    "5"),
              "-5");  // the minus sign
    EXPECT_EQ(clean("1\xE2\x81\x84"
                    "2"),
              "1/2");  // the fraction slash
    EXPECT_EQ(clean("a\xC2\xA0"
                    "b\xE2\x80\x89"
                    "c"),
              "a b c");  // a no-break and a thin space
}

TEST(RecipeClean, WhatTheFontHasNoGlyphForIsDropped)
{
    EXPECT_EQ(clean("a\xC2\xAD"
                    "b"),
              "ab");  // a soft hyphen
    EXPECT_EQ(clean("a\xE2\x80\x8B"
                    "b\xEF\xBB\xBF"
                    "c"),
              "abc");  // zero width signs, a byte order mark
    EXPECT_EQ(clean("Soup \xF0\x9F\x8D\xB2 hot"), "Soup hot");  // an emoji
    EXPECT_EQ(clean("\xD0\x9F\xD1\x80 text"), "text");          // Cyrillic
    EXPECT_EQ(clean("a\x01"
                    "b\x7F"
                    "c"),
              "abc");  // control characters
}

TEST(RecipeClean, InvalidUtf8IsSkippedNotCopied)
{
    EXPECT_EQ(clean("a\xC3 b"), "a b");  // a lead byte without its continuation
    EXPECT_EQ(clean("a\xFF"
                    "b"),
              "ab");
    EXPECT_EQ(clean("a\xE2\x82"), "a");  // cut off in the middle of a sign
}

TEST(RecipeClean, HtmlEntitiesAreDecoded)
{
    EXPECT_EQ(clean("Salt &amp; pepper &lt;fine&gt; &quot;hot&quot;"),
              "Salt & pepper <fine> \"hot\"");
    EXPECT_EQ(clean("K&auml;se, Gem&uuml;se, Spa&szlig;"),
              "K" + byte(0xE4) + "se, Gem" + byte(0xFC) + "se, Spa" + byte(0xDF));
    EXPECT_EQ(clean("a&#228;b &#xE4; &#8364;"),
              "a" + byte(0xE4) + "b " + byte(0xE4) + " " + byte(0x80));
    EXPECT_EQ(clean("1&nbsp;kg"), "1 kg");
    EXPECT_EQ(clean("&frac12; EL"), byte(0xBD) + " EL");
}

TEST(RecipeClean, AnAmpersandThatIsNoEntityStays)
{
    EXPECT_EQ(clean("AT&T and R&D"), "AT&T and R&D");
    EXPECT_EQ(clean("a &unknownentity; b"), "a &unknownentity; b");
    EXPECT_EQ(clean("fish & chips; fine"), "fish & chips; fine");
    EXPECT_EQ(clean("&#;"), "&#;");
}

TEST(RecipeClean, TagsAreRemovedAndParagraphsBecomeLines)
{
    EXPECT_EQ(clean("<p>First.</p><p>Second.</p>"), "First.\nSecond.");
    EXPECT_EQ(clean("Line<br>two<br/>three<BR />four"), "Line\ntwo\nthree\nfour");
    EXPECT_EQ(clean("<b>bold</b> and <a href=\"x\">link</a>"), "bold and link");
    EXPECT_EQ(clean("<ul><li>one</li><li>two</li></ul>"), "one\ntwo");
}

TEST(RecipeClean, ALessThanSignThatIsNoTagStays)
{
    EXPECT_EQ(clean("bake < 5 min, 3 < 4, a<b"), "bake < 5 min, 3 < 4, a<b");
    EXPECT_EQ(clean("x <3 y"), "x <3 y");
    EXPECT_EQ(clean("open <tag without end"), "open <tag without end");
}

TEST(RecipeClean, WhiteSpaceIsTidied)
{
    EXPECT_EQ(clean("  a   b\t\tc  "), "a b c");
    EXPECT_EQ(clean("a \n\n\n  b \r\n c"), "a\nb\nc");
    EXPECT_EQ(clean("\n\n  lead and trail  \n\n"), "lead and trail");
    EXPECT_EQ(clean("   "), "");
    EXPECT_EQ(clean(""), "");
}

TEST(RecipeClean, NoSpaceBeforeACommaOrSemicolon)
{
    // sources write "0,5 Bund , ersatzweise ..." and "(n. B. , scharfe)"
    EXPECT_EQ(clean("0,5 Bund , ersatzweise Basilikum"), "0,5 Bund, ersatzweise Basilikum");
    EXPECT_EQ(clean("Salz ; Pfeffer"), "Salz; Pfeffer");
    EXPECT_EQ(clean("a  ,  b"), "a, b");
    EXPECT_EQ(clean("a ,"), "a,");
    EXPECT_EQ(clean(", a"), ", a");  // nothing before it: stays
    EXPECT_EQ(clean("2 - 3 EL"), "2 - 3 EL");
}

TEST(RecipeClean, ATextWithManyOpeningBracketsAndNoClosingOneIsCleanedInOnePass)
{
    // 400 KB of "<a": the search for the '>' of each tag used to run to the end of the text every
    // time (quadratic: hundreds of milliseconds on a PC, seconds on the frame)
    std::string flood;
    while (flood.size() < 400000)
        flood += "<a";
    auto start = std::chrono::steady_clock::now();
    std::string out = clean(flood, flood.size() + 16);
    double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    EXPECT_EQ(out, flood);  // none of it is a tag: it stays
    EXPECT_LT(ms, 100.0);
    // the tags that are closed are still removed, before and after the lone '<'
    EXPECT_EQ(clean("a<b>b <i>c</i> d<e f<g>h"),
              "ab c dh");  // "<e f<g>" is one tag, as it always was
    EXPECT_EQ(clean("x<br>y<p>z"), "x\ny\nz");
    EXPECT_EQ(clean("<a href=\"x\">link</a> and <b"), "link and <b");
}

TEST(RecipeSteps, ANumberOfFiveDigitsOrMoreIsTextNotAStep)
{
    EXPECT_EQ(steps("1234\nMix."), "1234. Mix.");    // four digits: still a marker
    EXPECT_EQ(steps("12345\nMix."), "12345\nMix.");  // five: text
    // no overflow of the number (it was a signed overflow, undefined)
    EXPECT_EQ(steps("1234567890123456789012\nMix."), "1234567890123456789012\nMix.");
    EXPECT_EQ(steps("Step 99999999999\nMix."), "Step 99999999999\nMix.");
}

TEST(RecipeAmount, NoAmountIsBetterThanAnAbsurdOne)
{
    EXPECT_EQ(amount(100000.0, true), "100000");
    EXPECT_EQ(amount(100000.5, true), "");
    EXPECT_EQ(amount(1e30, true), "");
    EXPECT_EQ(amount(std::numeric_limits<double>::infinity(), true), "");
    EXPECT_EQ(amount(std::numeric_limits<double>::quiet_NaN(), true), "");
    EXPECT_EQ(amount(-3.0, true), "");
    EXPECT_EQ(amount(1500, false), "1500");
}

TEST(RecipeClean, ANullInputGivesAnEmptyText)
{
    char out[8] = "xxxxxxx";
    EXPECT_EQ(recipe_text_clean(nullptr, out, sizeof(out), nullptr), 0u);
    EXPECT_STREQ(out, "");
    EXPECT_EQ(recipe_text_clean("abc", nullptr, 0, nullptr), 0u);
}

TEST(RecipeClean, ASmallBufferCutsAndSaysSo)
{
    bool cut = false;
    EXPECT_EQ(clean("0123456789", 6, &cut), "01234");
    EXPECT_TRUE(cut);
    cut = true;
    EXPECT_EQ(clean("01234", 6, &cut), "01234");
    EXPECT_FALSE(cut);
}

TEST(RecipeClean, TheOutputIsAlwaysTerminatedAndWithinItsBuffer)
{
    for (size_t cap = 1; cap < 40; cap++) {
        std::string out(cap + 8, 'Q');
        bool cut = false;
        size_t len = recipe_text_clean("<p>K&auml;se &amp; Brot</p>  caf\xC3\xA9 \xE2\x82\xAC",
                                       &out[0], cap, &cut);
        ASSERT_LT(len, cap) << cap;
        EXPECT_EQ(out[len], '\0') << cap;
        EXPECT_EQ(out.substr(cap), std::string(8, 'Q'))
            << "written beyond the buffer at cap " << cap;
    }
}

TEST(RecipeSteps, MarkersOnTheirOwnLineJoinTheNextText)
{
    EXPECT_EQ(steps("STEP 1\nMix the flour.\nSTEP 2\nBake it."), "1. Mix the flour.\n2. Bake it.");
    EXPECT_EQ(steps("1\nMix.\n2\nBake."), "1. Mix.\n2. Bake.");
    EXPECT_EQ(steps("1.\nMix.\n2)\nBake.\n3:\nServe."), "1. Mix.\n2. Bake.\n3. Serve.");
    EXPECT_EQ(steps("Step 4:\nServe."), "4. Serve.");
}

TEST(RecipeSteps, ABareBulletTakesTheNextNumber)
{
    EXPECT_EQ(steps("\n" + byte(0x95) + "\nMix.\n" + byte(0x95) + "\nBake."), "1. Mix.\n2. Bake.");
    EXPECT_EQ(steps("-\nMix.\n-\nBake."), "1. Mix.\n2. Bake.");
}

TEST(RecipeSteps, TextThatStartsWithANumberIsText)
{
    EXPECT_EQ(steps("2 eggs and 1 cup of milk.\nStir."), "2 eggs and 1 cup of milk.\nStir.");
    EXPECT_EQ(steps("Step by step: mix.\nBake."), "Step by step: mix.\nBake.");
    EXPECT_EQ(steps("350 degrees\nBake."), "350 degrees\nBake.");
}

TEST(RecipeSteps, TextWithoutMarkersIsUnchanged)
{
    EXPECT_EQ(steps("Mix.\nBake.\nServe."), "Mix.\nBake.\nServe.");
    EXPECT_EQ(steps(""), "");
    EXPECT_EQ(steps("Only one line"), "Only one line");
}

TEST(RecipeSteps, ALastMarkerWithoutTextIsDropped)
{
    EXPECT_EQ(steps("Mix.\nSTEP 2"), "Mix.");
}

TEST(RecipeParagraphs, ALineOfTwoSentencesIsOneParagraph)
{
    EXPECT_EQ(paragraphs("Cut the onion. Fry it in oil."), "Cut the onion. Fry it in oil.");
    EXPECT_EQ(paragraphs("One sentence only."), "One sentence only.");
}

TEST(RecipeParagraphs, ALongLineIsCutAfterEverySecondSentence)
{
    EXPECT_EQ(paragraphs("A one. B two. C three. D four. E five."),
              "A one. B two.\nC three. D four.\nE five.");
}

TEST(RecipeParagraphs, TheLinesOfTheTextStaySeparate)
{
    EXPECT_EQ(paragraphs("First line.\nSecond line. Still second.\nThird."),
              "First line.\nSecond line. Still second.\nThird.");
}

TEST(RecipeParagraphs, AbbreviationsAndNumbersDoNotEndASentence)
{
    // "ca." "z. B." "Min." and the ordinal "2." are no sentence ends, so these are three sentences
    EXPECT_EQ(paragraphs("Mit ca. 2 EL Oel braten. Danach z. B. Zwiebeln dazugeben. Salzen."),
              "Mit ca. 2 EL Oel braten. Danach z. B. Zwiebeln dazugeben.\nSalzen.");
    EXPECT_EQ(paragraphs("Die 2. Haelfte der Masse formen. Dann backen. 20 Min. Backzeit. Fertig."),
              "Die 2. Haelfte der Masse formen. Dann backen.\n20 Min. Backzeit. Fertig.");
}

TEST(RecipeParagraphs, ExclamationAndQuestionMarksEndSentences)
{
    EXPECT_EQ(paragraphs("Fertig! Wirklich? Ja. Gut."), "Fertig! Wirklich?\nJa. Gut.");
}

TEST(RecipeParagraphs, ADotBeforeALowerCaseWordIsNoEnd)
{
    EXPECT_EQ(paragraphs("Mix well. then add. Next. Bake. Cool."),
              "Mix well. then add. Next.\nBake. Cool.");
}

TEST(RecipeParagraphs, GermanCapitalsStartASentence)
{
    std::string text =
        "Kochen. " + byte(0xC4) + "pfel schneiden. " + byte(0xD6) + "l erhitzen. Fertig.";
    EXPECT_EQ(paragraphs(text),
              "Kochen. " + byte(0xC4) + "pfel schneiden.\n" + byte(0xD6) + "l erhitzen. Fertig.");
}

TEST(RecipeParagraphs, EmptyAndSingleWords)
{
    EXPECT_EQ(paragraphs(""), "");
    EXPECT_EQ(paragraphs("Word"), "Word");
}

TEST(RecipeParagraphs, NothingIsLostOrInvented)
{
    // joining the paragraphs with a space gives the text back (apart from the line breaks)
    std::string text =
        "Dies ist ein Satz. Hier ist noch einer. Und ein dritter! Ein vierter? Der letzte.";
    std::string out = paragraphs(text);
    for (char &c : out) {
        if (c == '\n') {
            c = ' ';
        }
    }
    EXPECT_EQ(out, text);
}

TEST(RecipeMinutes, GermanAndEnglish)
{
    EXPECT_EQ(minutes(35, true), "35 Min.");
    EXPECT_EQ(minutes(80, true), "1 Std. 20 Min.");
    EXPECT_EQ(minutes(120, true), "2 Std.");
    EXPECT_EQ(minutes(35, false), "35 min");
    EXPECT_EQ(minutes(80, false), "1 h 20 min");
    EXPECT_EQ(minutes(120, false), "2 h");
    EXPECT_EQ(minutes(60, true), "1 Std.");
    EXPECT_EQ(minutes(1, true), "1 Min.");
}

TEST(RecipeMinutes, NoTimeGivesNothing)
{
    EXPECT_EQ(minutes(0, true), "");
    EXPECT_EQ(minutes(-5, false), "");
}

TEST(RecipeAmount, WholeNumbersAndDecimals)
{
    EXPECT_EQ(amount(400, true), "400");
    EXPECT_EQ(amount(2.0, false), "2");
    EXPECT_EQ(amount(0.5, true), "0,5");
    EXPECT_EQ(amount(0.5, false), "0.5");
    EXPECT_EQ(amount(1.25, true), "1,25");
    EXPECT_EQ(amount(0.333333, true), "0,33");
    EXPECT_EQ(amount(1.5, true), "1,5");
    EXPECT_EQ(amount(12.0, true), "12");
}

TEST(RecipeAmount, NothingForZeroOrLess)
{
    EXPECT_EQ(amount(0, true), "");
    EXPECT_EQ(amount(-1, true), "");
    EXPECT_EQ(amount(0.001, true), "");  // rounds to nothing
}
