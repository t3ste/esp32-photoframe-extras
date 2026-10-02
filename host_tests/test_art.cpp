// The artworks mode (build option `artworks`): the choice of kind and source, the caption text, the
// readers of the three services' answers (real answers in data/art/, see its README) and the album
// rule that keeps free space. Pure code on a temporary directory - no network, no frame.

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "art_caption.h"
#include "art_select.h"
#include "art_sources.h"
#include "art_store.h"
}

#ifndef ART_TEST_DATA_DIR
#error "ART_TEST_DATA_DIR must point at host_tests/data/art"
#endif

namespace
{

std::string fixture(const std::string &name)
{
    std::ifstream file(std::string(ART_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    EXPECT_TRUE(file.good()) << name;
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// The text with every `from` replaced; fails the test when there is none (a replacement that does
// nothing would make a test pass for the wrong reason)
std::string replaced(std::string text, const std::string &from, const std::string &to)
{
    size_t at = text.find(from);
    EXPECT_NE(at, std::string::npos) << "not in the fixture: " << from;
    while (at != std::string::npos) {
        text.replace(at, from.size(), to);
        at = text.find(from, at + to.size());
    }
    return text;
}

std::string fold(const std::string &text)
{
    char out[256];
    art_caption_fold(text.c_str(), out, sizeof(out));
    return out;
}

std::string compose(const char *artist, const char *title, const char *year, int max_chars)
{
    char out[256];
    art_caption_compose(artist, title, year, max_chars, out, sizeof(out));
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Choice

TEST(ArtSelect, TheTypeIsPickedAmongTheEnabledOnesWithEqualChance)
{
    unsigned mask = (1u << ART_TYPE_PAINTING) | (1u << ART_TYPE_PRINT);
    EXPECT_EQ(art_select_type(mask, 0), ART_TYPE_PAINTING);
    EXPECT_EQ(art_select_type(mask, 1), ART_TYPE_PRINT);
    EXPECT_EQ(art_select_type(mask, 2), ART_TYPE_PAINTING);
    EXPECT_EQ(art_select_type(ART_TYPES_ALL, 4), ART_TYPE_DRAWING);
    EXPECT_EQ(art_select_type(1u << ART_TYPE_DRAWING, 12345), ART_TYPE_DRAWING);
    int counts[ART_TYPE_COUNT] = {0, 0, 0};
    for (uint32_t rnd = 0; rnd < 300; rnd++) {
        counts[art_select_type(ART_TYPES_ALL, rnd)]++;
    }
    EXPECT_EQ(counts[0], 100);
    EXPECT_EQ(counts[1], 100);
    EXPECT_EQ(counts[2], 100);
}

TEST(ArtSelect, NoTypeEnabledGivesNone)
{
    EXPECT_EQ(art_select_type(0, 5), -1);
    EXPECT_EQ(art_select_type(0xF8u, 5), -1);  // bits that are no kind
}

TEST(ArtSelect, SourcesComeInAFixedOrderAndLeaveOutWhatHasNoWorksOfTheKind)
{
    art_source_t out[ART_SOURCE_COUNT];
    EXPECT_EQ(art_select_sources(ART_SOURCES_ALL, ART_TYPE_PAINTING, out), 3);
    EXPECT_EQ(out[0], ART_SOURCE_RIJKS);
    EXPECT_EQ(out[1], ART_SOURCE_SMK);
    EXPECT_EQ(out[2], ART_SOURCE_SMITHSONIAN);
    EXPECT_EQ(art_select_sources(ART_SOURCES_ALL, ART_TYPE_PRINT, out), 2);  // no Smithsonian
    EXPECT_EQ(out[1], ART_SOURCE_SMK);
    EXPECT_EQ(art_select_sources(1u << ART_SOURCE_SMK, ART_TYPE_DRAWING, out), 1);
    EXPECT_EQ(out[0], ART_SOURCE_SMK);
    EXPECT_EQ(art_select_sources(1u << ART_SOURCE_SMITHSONIAN, ART_TYPE_PRINT, out), 0);
    EXPECT_EQ(art_select_sources(0, ART_TYPE_PAINTING, out), 0);
}

TEST(ArtSelect, TheYearStaysInRange)
{
    for (uint32_t rnd = 0; rnd < 2000; rnd += 7) {
        int year = art_select_year(rnd);
        EXPECT_GE(year, ART_YEAR_MIN);
        EXPECT_LE(year, ART_YEAR_MAX);
    }
    EXPECT_EQ(art_select_year(0), ART_YEAR_MIN);
    EXPECT_EQ(art_select_year(ART_YEAR_MAX - ART_YEAR_MIN), ART_YEAR_MAX);
}

TEST(ArtSelect, Names)
{
    EXPECT_STREQ(art_type_name(ART_TYPE_DRAWING), "drawing");
    EXPECT_STREQ(art_source_name(ART_SOURCE_SMK), "SMK");
    EXPECT_STREQ(art_source_tag(ART_SOURCE_SMITHSONIAN), "si");
    EXPECT_STREQ(art_source_tag(ART_SOURCE_RIJKS), "rijks");
}

// ---------------------------------------------------------------------------------------------
// Caption

TEST(ArtCaption, AccentedLettersBecomePlainLetters)
{
    EXPECT_EQ(fold("Selvportræt en face"), "Selvportraet en face");
    EXPECT_EQ(fold("Eugène Delacroix"), "Eugene Delacroix");
    EXPECT_EQ(fold("Søren Ørsted, Łódź, Dvořák, Çelik, Ñandú"),
              "Soren Orsted, Lodz, Dvorak, Celik, Nandu");
    EXPECT_EQ(fold("Œuvre ŝ ĳ ß"), "OEuvre s ij ß");
}

TEST(ArtCaption, TheLettersTheFontHasStayAsTheyAre)
{
    EXPECT_EQ(fold("Käse Größe Über"), "Käse Größe Über");
    EXPECT_EQ(fold("ä ö ü Ä Ö Ü ß ° €"), "ä ö ü Ä Ö Ü ß ° €");
}

TEST(ArtCaption, PunctuationBecomesAscii)
{
    EXPECT_EQ(fold("a – b — c"), "a - b - c");
    EXPECT_EQ(fold("‘x’ “y” «z»"), "'x' \"y\" \"z\"");
    EXPECT_EQ(fold("wait…"), "wait...");
    EXPECT_EQ(fold("a b"), "a b");  // no-break space
}

TEST(ArtCaption, OtherThingsAreDroppedAndBlanksAreTidy)
{
    EXPECT_EQ(fold("  a   b \t\n c  "), "a b c");
    EXPECT_EQ(fold("x\x01y\x7f"), "xy");
    EXPECT_EQ(fold("smile 😀 done"), "smile done");
    EXPECT_EQ(fold("汉字"), "");
    EXPECT_EQ(fold("a\xC3(b"), "a(b");  // a cut-off UTF-8 sequence
    EXPECT_EQ(fold(""), "");
    char out[8];
    EXPECT_EQ(art_caption_fold(nullptr, out, sizeof(out)), 0u);
    EXPECT_STREQ(out, "");
}

TEST(ArtCaption, EveryLatinExtendedLetterFoldsToAscii)
{
    for (unsigned cp = 0x100; cp <= 0x17F; cp++) {
        char utf8[4] = {(char) (0xC0 | (cp >> 6)), (char) (0x80 | (cp & 0x3F)), 0, 0};
        std::string folded = fold(utf8);
        EXPECT_FALSE(folded.empty()) << std::hex << cp;
        for (unsigned char c : folded) {
            EXPECT_LT(c, 0x80) << std::hex << cp;
        }
    }
    for (unsigned cp = 0xC0; cp <= 0xFF; cp++) {
        char utf8[4] = {(char) (0xC0 | (cp >> 6)), (char) (0x80 | (cp & 0x3F)), 0, 0};
        std::string folded = fold(utf8);
        bool font_glyph = cp == 0xC4 || cp == 0xD6 || cp == 0xDC || cp == 0xDF || cp == 0xE4 ||
                          cp == 0xF6 || cp == 0xFC;
        if (font_glyph) {
            EXPECT_EQ(folded, std::string(utf8)) << std::hex << cp;
        } else if (cp != 0xF7) {  // the division sign has no letter
            EXPECT_FALSE(folded.empty()) << std::hex << cp;
            for (unsigned char c : folded) {
                EXPECT_LT(c, 0x80) << std::hex << cp;
            }
        }
    }
}

TEST(ArtCaption, FoldNeverWritesPastTheBuffer)
{
    char out[6];
    EXPECT_LE(art_caption_fold("Selvportræt", out, sizeof(out)), 5u);
    EXPECT_STREQ(out, "Selvp");
    char one[1];
    EXPECT_EQ(art_caption_fold("abc", one, sizeof(one)), 0u);
    EXPECT_STREQ(one, "");
}

TEST(ArtCaption, CharactersAreCountedNotBytes)
{
    EXPECT_EQ(art_caption_char_count("abc"), 3);
    EXPECT_EQ(art_caption_char_count("Käse"), 4);
    EXPECT_EQ(art_caption_char_count(""), 0);
    EXPECT_EQ(art_caption_char_count(nullptr), 0);
}

TEST(ArtCaption, ComposeJoinsArtistTitleAndYear)
{
    EXPECT_EQ(compose("Jan Toorop", "Misty Sea", "1899", 47), "Jan Toorop - Misty Sea (1899)");
    EXPECT_EQ(compose("", "Misty Sea", "1899", 47), "Misty Sea (1899)");
    EXPECT_EQ(compose("Jan Toorop", "", "1899", 47), "Jan Toorop (1899)");
    EXPECT_EQ(compose("Jan Toorop", "Misty Sea", "", 47), "Jan Toorop - Misty Sea");
    EXPECT_EQ(compose("", "", "1899", 47), "1899");
    EXPECT_EQ(compose("", "", "", 47), "");
    EXPECT_EQ(compose(nullptr, nullptr, nullptr, 47), "");
}

TEST(ArtCaption, ComposeFoldsEveryPart)
{
    EXPECT_EQ(compose("Eugène Delacroix", "La Liberté guidant le peuple", "1830", 60),
              "Eugene Delacroix - La Liberte guidant le peuple (1830)");
}

TEST(ArtCaption, ALongTitleLosesItsEndAndKeepsTheYear)
{
    std::string text = compose("Rembrandt van Rijn",
                               "The Night Watch, Militia Company of District II", "1642", 47);
    EXPECT_EQ(art_caption_char_count(text.c_str()), 47);
    // 18 + 3 for " - " + 19 (18 letters and the "~") + 7 for " (1642)" = 47
    EXPECT_EQ(text, "Rembrandt van Rijn - The Night Watch, M~ (1642)");
}

TEST(ArtCaption, WhenEvenTheShortenedTitleDoesNotFitTheWholeTextIsCut)
{
    std::string text =
        compose("A very long artist name that takes everything", "Title", "1642", 30);
    EXPECT_EQ(art_caption_char_count(text.c_str()), 30);
    EXPECT_EQ(text.back(), '~');
    EXPECT_EQ(text.substr(0, 10), "A very lon");
}

TEST(ArtCaption, ADateTextWithoutADigitIsLeftOut)
{
    EXPECT_EQ(compose("Artist", "Title", "n.d.", 47), "Artist - Title");
    EXPECT_EQ(compose("Artist", "Title", "undated", 47), "Artist - Title");
    EXPECT_EQ(compose("Artist", "Title", "ca. 1642", 47), "Artist - Title (ca. 1642)");
    EXPECT_EQ(compose("Artist", "Title", "1677 - 1755", 47), "Artist - Title (1677 - 1755)");
}

TEST(ArtCaption, ALongDateTextIsLeftOut)
{
    EXPECT_EQ(compose("A", "B", "between 1642 and 1650, perhaps", 47), "A - B");
}

TEST(ArtCaption, TheLimitCountsCharactersNotBytes)
{
    std::string text = compose("Käthe Kollwitz", "Die Überfahrt über den Rhein", "1920", 30);
    EXPECT_LE(art_caption_char_count(text.c_str()), 30);
    EXPECT_NE(text.find("Käthe"), std::string::npos);  // the umlaut is kept
}

TEST(ArtCaption, ComposeNeverWritesPastTheBuffer)
{
    char out[12];
    art_caption_compose("Rembrandt van Rijn", "The Night Watch", "1642", 47, out, sizeof(out));
    EXPECT_LT(strlen(out), sizeof(out));
    art_caption_compose("Rembrandt van Rijn", "The Night Watch", "1642", 10, out, sizeof(out));
    EXPECT_LT(strlen(out), sizeof(out));
    char one[1] = {'x'};
    art_caption_compose("a", "b", "c", 47, one, sizeof(one));
    EXPECT_STREQ(one, "");
}

// ---------------------------------------------------------------------------------------------
// Rijksmuseum

TEST(ArtRijks, TheSearchAddress)
{
    char url[256];
    ASSERT_TRUE(art_rijks_search_url(ART_TYPE_PAINTING, 1642, url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://data.rijksmuseum.nl/search/collection?type=painting&imageAvailable=true"
                 "&creationDate=1642");
    ASSERT_TRUE(art_rijks_search_url(ART_TYPE_PRINT, 1900, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("type=print&"), std::string::npos);
    char small[20];
    EXPECT_FALSE(art_rijks_search_url(ART_TYPE_PAINTING, 1642, small, sizeof(small)));
    EXPECT_FALSE(art_rijks_search_url(ART_TYPE_PAINTING, -1, url, sizeof(url)));
}

TEST(ArtRijks, OneHitOfTheSearchIsPicked)
{
    std::string json = fixture("rijks-search-painting.json");
    char url[160];
    EXPECT_EQ(art_rijks_pick_object(json.c_str(), json.size(), 0, url, sizeof(url)), 100);
    EXPECT_STREQ(url, "https://data.rijksmuseum.nl/200100988?_profile=la-framed");
    EXPECT_EQ(art_rijks_pick_object(json.c_str(), json.size(), 99, url, sizeof(url)), 100);
    EXPECT_STREQ(url, "https://data.rijksmuseum.nl/200107864?_profile=la-framed");
    EXPECT_EQ(art_rijks_pick_object(json.c_str(), json.size(), 100, url, sizeof(url)), 100);
    EXPECT_STREQ(url, "https://data.rijksmuseum.nl/200100988?_profile=la-framed");  // wraps round
}

TEST(ArtRijks, ASearchWithoutHitsOrWithAForeignRecordGivesNone)
{
    char url[160];
    std::string empty = "{\"orderedItems\":[]}";
    EXPECT_EQ(art_rijks_pick_object(empty.c_str(), empty.size(), 3, url, sizeof(url)), 0);
    EXPECT_STREQ(url, "");
    std::string none = "{\"detail\":\"Unsupported query parameter: ps\"}";
    EXPECT_EQ(art_rijks_pick_object(none.c_str(), none.size(), 3, url, sizeof(url)), 0);
    std::string json = fixture("rijks-search-painting.json");
    std::string foreign = replaced(json, "https://id.rijksmuseum.nl/", "https://evil.example/");
    EXPECT_EQ(art_rijks_pick_object(foreign.c_str(), foreign.size(), 0, url, sizeof(url)), 0);
    EXPECT_STREQ(url, "");
    EXPECT_EQ(art_rijks_pick_object("not json", 8, 0, url, sizeof(url)), 0);
    EXPECT_EQ(art_rijks_pick_object(nullptr, 0, 0, url, sizeof(url)), 0);
}

TEST(ArtRijks, TheRecordGivesTitleArtistYearAndTheVisualItem)
{
    std::string json = fixture("rijks-object.json");
    art_work_t work;
    char visual[160];
    ASSERT_TRUE(art_rijks_parse_object(json.c_str(), json.size(), &work, visual, sizeof(visual)));
    EXPECT_EQ(work.source, ART_SOURCE_RIJKS);
    EXPECT_STREQ(work.id, "200100988");
    EXPECT_STREQ(work.title, "Misty Sea");
    EXPECT_STREQ(work.artist, "Jan Toorop");
    EXPECT_STREQ(work.year, "1899");
    EXPECT_STREQ(visual, "https://data.rijksmuseum.nl/202100988?_profile=la-framed");
}

TEST(ArtRijks, ARecordWithoutAPictureIsRefused)
{
    std::string json = replaced(fixture("rijks-object.json"), "\"shows\"", "\"shows_not\"");
    art_work_t work;
    char visual[160];
    EXPECT_FALSE(art_rijks_parse_object(json.c_str(), json.size(), &work, visual, sizeof(visual)));
    EXPECT_STREQ(visual, "");
}

TEST(ArtRijks, ThePictureMustBePublicDomainOrCc0)
{
    std::string json = fixture("rijks-visual-item.json");
    art_work_t work;
    memset(&work, 0, sizeof(work));
    char digital[160];
    ASSERT_TRUE(
        art_rijks_parse_visual_item(json.c_str(), json.size(), &work, digital, sizeof(digital)));
    EXPECT_STREQ(work.rights, "PDM");
    EXPECT_STREQ(digital, "https://data.rijksmuseum.nl/5008910398567010810098?_profile=la-framed");

    std::string zero = replaced(json, "publicdomain/mark/1.0", "publicdomain/zero/1.0");
    ASSERT_TRUE(
        art_rijks_parse_visual_item(zero.c_str(), zero.size(), &work, digital, sizeof(digital)));
    EXPECT_STREQ(work.rights, "CC0");

    std::string restricted = replaced(json, "publicdomain/mark/1.0", "licenses/by-nc/4.0");
    EXPECT_FALSE(art_rijks_parse_visual_item(restricted.c_str(), restricted.size(), &work, digital,
                                             sizeof(digital)));
    EXPECT_STREQ(digital, "");
    std::string no_rights = replaced(json, "\"subject_to\"", "\"subject_to_not\"");
    EXPECT_FALSE(art_rijks_parse_visual_item(no_rights.c_str(), no_rights.size(), &work, digital,
                                             sizeof(digital)));
}

TEST(ArtRijks, TheDigitalObjectGivesTheIiifAddress)
{
    std::string json = fixture("rijks-digital-object.json");
    art_work_t work;
    memset(&work, 0, sizeof(work));
    ASSERT_TRUE(art_rijks_parse_digital_object(json.c_str(), json.size(), &work));
    EXPECT_STREQ(work.image_base, "https://iiif.micr.io/mPymb");
}

TEST(ArtRijks, APictureThatIsNotForDownloadOrFromAnotherHostIsRefused)
{
    art_work_t work;
    memset(&work, 0, sizeof(work));
    std::string json = fixture("rijks-digital-object.json");
    std::string no_download = replaced(json, "downloadbaar", "zichtbaar");
    EXPECT_FALSE(art_rijks_parse_digital_object(no_download.c_str(), no_download.size(), &work));
    std::string foreign = replaced(json, "https://iiif.micr.io/", "https://evil.example/");
    EXPECT_FALSE(art_rijks_parse_digital_object(foreign.c_str(), foreign.size(), &work));
    std::string odd = replaced(json, "mPymb", "mP..ymb");
    EXPECT_FALSE(art_rijks_parse_digital_object(odd.c_str(), odd.size(), &work));
}

// ---------------------------------------------------------------------------------------------
// SMK

TEST(ArtSmk, TheSearchAddressWithEncodedBrackets)
{
    char url[300];
    ASSERT_TRUE(art_smk_search_url(ART_TYPE_PAINTING, 5, 1, url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://api.smk.dk/api/v1/art/search/?keys=*&filters=%5Bhas_image%3Atrue%5D%2C"
                 "%5Bpublic_domain%3Atrue%5D%2C%5Bobject_names%3Amaleri%5D&offset=5&rows=1");
    ASSERT_TRUE(art_smk_search_url(ART_TYPE_DRAWING, 0, 0, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("object_names%3Ategning"), std::string::npos);
    ASSERT_TRUE(art_smk_search_url(ART_TYPE_PRINT, 0, 0, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("object_names%3Agrafik"), std::string::npos);
    EXPECT_FALSE(art_smk_search_url(ART_TYPE_PRINT, 0, 99, url, sizeof(url)));
    char small[40];
    EXPECT_FALSE(art_smk_search_url(ART_TYPE_PAINTING, 5, 1, small, sizeof(small)));
}

TEST(ArtSmk, TheCountOfTheSearch)
{
    std::string json = fixture("smk-count.json");
    EXPECT_EQ(art_smk_found(json.c_str(), json.size()), 13929);
    EXPECT_EQ(art_smk_found("{}", 2), -1);
    EXPECT_EQ(art_smk_found("[", 1), -1);
}

TEST(ArtSmk, APaintingGivesItsDetails)
{
    std::string json = fixture("smk-item-painting.json");
    art_work_t work;
    ASSERT_TRUE(art_smk_parse_item(json.c_str(), json.size(), &work));
    EXPECT_EQ(work.source, ART_SOURCE_SMK);
    EXPECT_STREQ(work.id, "KMS7474");
    EXPECT_STREQ(work.artist, "Kristian Zahrtmann");
    EXPECT_STREQ(work.title, "Selvportræt en face. Lampelys");
    EXPECT_STREQ(work.year, "1914");
    EXPECT_STREQ(work.rights, "PDM");
    EXPECT_STREQ(work.image_base,
                 "https://iip.smk.dk/iiif/jp2/kh04dt02r_KMS7474.TIF.reconstructed.tif.jp2");
}

TEST(ArtSmk, APrintGivesTheRangeOfYears)
{
    std::string json = fixture("smk-item-print.json");
    art_work_t work;
    ASSERT_TRUE(art_smk_parse_item(json.c_str(), json.size(), &work));
    EXPECT_STREQ(work.id, "KKS5261");
    EXPECT_STREQ(work.artist, "Antonio da Trento");
    EXPECT_STREQ(work.year, "1500-1550");
}

TEST(ArtSmk, AWorkThatIsNotPublicDomainOrFromAnotherHostIsRefused)
{
    std::string json = fixture("smk-item-painting.json");
    art_work_t work;
    std::string closed = replaced(json, "\"public_domain\":true", "\"public_domain\":false");
    EXPECT_FALSE(art_smk_parse_item(closed.c_str(), closed.size(), &work));
    std::string foreign = replaced(json, "https://iip.smk.dk/", "https://evil.example/");
    EXPECT_FALSE(art_smk_parse_item(foreign.c_str(), foreign.size(), &work));
    std::string no_picture = replaced(json, "\"image_iiif_id\"", "\"image_iiif_id_not\"");
    EXPECT_FALSE(art_smk_parse_item(no_picture.c_str(), no_picture.size(), &work));
    EXPECT_FALSE(art_smk_parse_item("{\"items\":[]}", 12, &work));
}

TEST(ArtSmk, AnObjectNumberWithASlashBecomesAFileSafeId)
{
    std::string json = replaced(fixture("smk-item-print.json"), "\"KKS5261\"", "\"KKS1975-669/5\"");
    art_work_t work;
    ASSERT_TRUE(art_smk_parse_item(json.c_str(), json.size(), &work));
    EXPECT_STREQ(work.id, "KKS1975-669_5");
}

// ---------------------------------------------------------------------------------------------
// Smithsonian

TEST(ArtSmithsonian, TheKeyLooksLikeAKey)
{
    EXPECT_TRUE(art_si_key_valid(ART_SMITHSONIAN_DEMO_KEY));
    EXPECT_TRUE(art_si_key_valid("a1B2c3D4e5F6g7H8i9J0k1L2m3N4o5P6q7R8s9T0"));
    EXPECT_FALSE(art_si_key_valid(""));
    EXPECT_FALSE(art_si_key_valid("abc"));
    EXPECT_FALSE(art_si_key_valid("key with blank"));
    EXPECT_FALSE(art_si_key_valid("key&x=1"));
    EXPECT_FALSE(art_si_key_valid(nullptr));
    EXPECT_FALSE(art_si_key_valid(std::string(65, 'a').c_str()));
}

TEST(ArtSmithsonian, TheSearchAddress)
{
    char url[400];
    ASSERT_TRUE(art_si_search_url(ART_TYPE_PAINTING, 7, 1, "DEMO_KEY", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://api.si.edu/openaccess/api/v1.0/search?q=unit_code%3A%22SAAM%22%20AND%20"
                 "online_media_type%3A%22Images%22%20AND%20object_type%3A%22Paintings%22&start=7"
                 "&rows=1&api_key=DEMO_KEY");
    ASSERT_TRUE(art_si_search_url(ART_TYPE_DRAWING, 0, 0, "DEMO_KEY", url, sizeof(url)));
    EXPECT_NE(std::string(url).find("%22Drawings%22"), std::string::npos);
    EXPECT_FALSE(art_si_search_url(ART_TYPE_PRINT, 0, 1, "DEMO_KEY", url, sizeof(url)));  // no term
    EXPECT_FALSE(art_si_search_url(ART_TYPE_PAINTING, 0, 1, "bad key", url, sizeof(url)));
    char small[60];
    EXPECT_FALSE(art_si_search_url(ART_TYPE_PAINTING, 0, 1, "DEMO_KEY", small, sizeof(small)));
}

TEST(ArtSmithsonian, TheCountAndTheRow)
{
    std::string json = fixture("si-row-painting.json");
    EXPECT_EQ(art_si_row_count(json.c_str(), json.size()), 4067);
    art_work_t work;
    ASSERT_TRUE(art_si_parse_row(json.c_str(), json.size(), &work));
    EXPECT_EQ(work.source, ART_SOURCE_SMITHSONIAN);
    EXPECT_STREQ(work.title, "The Broyling of Their Fish over the Flame of Fier");
    EXPECT_STREQ(work.artist, "Spencer Nichols");  // cut before ", born ..."
    EXPECT_STREQ(work.year, "");
    EXPECT_STREQ(work.rights, "CC0");
    EXPECT_STREQ(work.id, "SAAM-1985_66_403415_1");
    EXPECT_STREQ(work.image_base,
                 "https://ids.si.edu/ids/deliveryService?id=SAAM-1985.66.403415_1");
    EXPECT_EQ(art_si_row_count("{}", 2), -1);
}

TEST(ArtSmithsonian, APictureThatIsNotCc0OrFromAnotherHostIsRefused)
{
    std::string json = fixture("si-row-painting.json");
    art_work_t work;
    std::string closed =
        replaced(json, "\"access\": \"CC0\"", "\"access\": \"Usage conditions apply\"");
    EXPECT_FALSE(art_si_parse_row(closed.c_str(), closed.size(), &work));
    std::string foreign = replaced(json, "https://ids.si.edu/", "https://evil.example/");
    EXPECT_FALSE(art_si_parse_row(foreign.c_str(), foreign.size(), &work));
    EXPECT_FALSE(art_si_parse_row("{\"response\":{\"rows\":[]}}", 24, &work));
}

// ---------------------------------------------------------------------------------------------
// The picture

TEST(ArtImage, TheSmallestPictureThatCoversThePanel)
{
    art_work_t work;
    memset(&work, 0, sizeof(work));
    char url[256];

    work.source = ART_SOURCE_RIJKS;
    snprintf(work.image_base, sizeof(work.image_base), "https://iiif.micr.io/mPymb");
    ASSERT_TRUE(art_image_url(&work, 800, 480, url, sizeof(url)));
    EXPECT_STREQ(url, "https://iiif.micr.io/mPymb/full/!800,480/0/default.jpg");

    work.source = ART_SOURCE_SMK;
    snprintf(work.image_base, sizeof(work.image_base), "https://iip.smk.dk/iiif/jp2/x.tif.jp2");
    ASSERT_TRUE(art_image_url(&work, 480, 800, url, sizeof(url)));
    EXPECT_STREQ(url, "https://iip.smk.dk/iiif/jp2/x.tif.jp2/full/!480,800/0/default.jpg");

    work.source = ART_SOURCE_SMITHSONIAN;
    snprintf(work.image_base, sizeof(work.image_base),
             "https://ids.si.edu/ids/deliveryService?id=SAAM-1");
    ASSERT_TRUE(art_image_url(&work, 800, 480, url, sizeof(url)));
    EXPECT_STREQ(url, "https://ids.si.edu/ids/deliveryService?id=SAAM-1&max=800");
}

namespace
{
// The markers of a JPEG up to the frame header: a comment, a quantisation table, a Huffman table
// (which starts with the same 0xC4 that is no frame header) and the frame header `sof`
std::vector<uint8_t> jpeg_with(uint8_t sof)
{
    return {0xFF, 0xD8,                                      // start of image
            0xFF, 0xE0, 0x00, 0x04, 0x00, 0x00,              // APP0, 2 bytes
            0xFF, 0xDB, 0x00, 0x03, 0x00,                    // DQT, 1 byte
            0xFF, 0xC4, 0x00, 0x03, 0x00,                    // DHT, 1 byte
            0xFF, sof,  0x00, 0x0B, 0x08, 0x00, 0x10, 0x00,  // the frame header: 8 bits, 16 x 16
            0x10, 0x01, 0x01, 0x11, 0x00,                    // one component
            0xFF, 0xDA, 0x00, 0x02};                         // start of scan
}
}  // namespace

TEST(ArtImage, OnlyABaselineJpegCanBeDecodedByTheFrame)
{
    auto baseline = jpeg_with(0xC0);
    EXPECT_TRUE(art_jpeg_is_baseline(baseline.data(), baseline.size()));
    auto extended = jpeg_with(0xC1);
    EXPECT_TRUE(art_jpeg_is_baseline(extended.data(), extended.size()));
    for (uint8_t sof : {0xC2, 0xC3, 0xC5, 0xC9, 0xCA, 0xCF}) {  // progressive, lossless, arithmetic
        auto other = jpeg_with(sof);
        EXPECT_FALSE(art_jpeg_is_baseline(other.data(), other.size())) << std::hex << (int) sof;
    }
}

TEST(ArtImage, TheJpegCheckSurvivesFillBytesAndRefusesNonsense)
{
    auto fill = jpeg_with(0xC0);
    fill.insert(fill.begin() + 2, {0xFF, 0xFF});  // fill bytes before a marker are allowed
    EXPECT_TRUE(art_jpeg_is_baseline(fill.data(), fill.size()));

    std::vector<uint8_t> no_frame = {0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x02, 0x00, 0x00};
    EXPECT_FALSE(
        art_jpeg_is_baseline(no_frame.data(), no_frame.size()));  // a scan, no frame header
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    EXPECT_FALSE(art_jpeg_is_baseline(png.data(), png.size()));
    std::vector<uint8_t> html = {'<', 'h', 't', 'm', 'l', '>'};
    EXPECT_FALSE(art_jpeg_is_baseline(html.data(), html.size()));
    EXPECT_FALSE(art_jpeg_is_baseline(nullptr, 100));
    EXPECT_FALSE(art_jpeg_is_baseline(fill.data(), 0));

    auto good = jpeg_with(0xC0);
    for (size_t cut = 0; cut <= 21; cut++) {  // cut off before the frame header's length is there
        EXPECT_FALSE(art_jpeg_is_baseline(good.data(), cut)) << cut;
    }
    std::vector<uint8_t> zero_length = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x00, 0xFF, 0xC0, 0x00, 0x0B};
    EXPECT_FALSE(art_jpeg_is_baseline(zero_length.data(), zero_length.size()));
}

TEST(ArtImage, ImpossibleRequestsAreRefused)
{
    art_work_t work;
    memset(&work, 0, sizeof(work));
    char url[256];
    EXPECT_FALSE(art_image_url(&work, 800, 480, url, sizeof(url)));  // no picture
    snprintf(work.image_base, sizeof(work.image_base), "https://iiif.micr.io/mPymb");
    EXPECT_FALSE(art_image_url(&work, 0, 480, url, sizeof(url)));
    EXPECT_FALSE(art_image_url(&work, 800, 100000, url, sizeof(url)));
    char small[20];
    EXPECT_FALSE(art_image_url(&work, 800, 480, small, sizeof(small)));
    EXPECT_FALSE(art_image_url(nullptr, 800, 480, url, sizeof(url)));
}

// ---------------------------------------------------------------------------------------------
// The readers do not trip over a damaged answer

TEST(ArtSources, ACutOffAnswerNeverCrashesAnyReader)
{
    const char *names[] = {"rijks-search-painting.json", "rijks-object.json",
                           "rijks-visual-item.json",     "rijks-digital-object.json",
                           "smk-item-painting.json",     "si-row-painting.json"};
    for (const char *name : names) {
        std::string json = fixture(name);
        for (size_t cut = 0; cut < json.size(); cut += 61) {
            std::string part = json.substr(0, cut);
            art_work_t work;
            memset(&work, 0, sizeof(work));
            char a[160], b[160];
            art_rijks_pick_object(part.c_str(), part.size(), 3, a, sizeof(a));
            art_rijks_parse_object(part.c_str(), part.size(), &work, a, sizeof(a));
            art_rijks_parse_visual_item(part.c_str(), part.size(), &work, b, sizeof(b));
            art_rijks_parse_digital_object(part.c_str(), part.size(), &work);
            art_smk_found(part.c_str(), part.size());
            art_smk_parse_item(part.c_str(), part.size(), &work);
            art_si_row_count(part.c_str(), part.size());
            art_si_parse_row(part.c_str(), part.size(), &work);
        }
    }
}

TEST(ArtSources, ChangedBytesNeverCrashAnyReader)
{
    const char *names[] = {"rijks-search-painting.json", "rijks-object.json",
                           "rijks-visual-item.json",     "rijks-digital-object.json",
                           "smk-item-painting.json",     "si-row-painting.json"};
    uint32_t state = 12345;  // a fixed sequence, so a failure can be repeated
    auto next = [&state]() {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    };
    for (const char *name : names) {
        std::string original = fixture(name);
        for (int round = 0; round < 150; round++) {
            std::string json = original;
            int changes = 1 + (int) (next() % 4);
            for (int i = 0; i < changes; i++) {
                json[next() % json.size()] = (char) (next() % 256);
            }
            art_work_t work;
            memset(&work, 0, sizeof(work));
            char a[160], b[160];
            art_rijks_pick_object(json.c_str(), json.size(), next(), a, sizeof(a));
            art_rijks_parse_object(json.c_str(), json.size(), &work, a, sizeof(a));
            art_rijks_parse_visual_item(json.c_str(), json.size(), &work, b, sizeof(b));
            art_rijks_parse_digital_object(json.c_str(), json.size(), &work);
            art_smk_found(json.c_str(), json.size());
            art_smk_parse_item(json.c_str(), json.size(), &work);
            art_si_row_count(json.c_str(), json.size());
            art_si_parse_row(json.c_str(), json.size(), &work);
            // whatever was read must still be a usable name and a short caption
            char base[ART_BASE_MAX];
            art_store_file_base(&work, base, sizeof(base));
            EXPECT_EQ(std::string(base).find('/'), std::string::npos);
            char caption[ART_CAPTION_TEXT_MAX];
            art_caption_compose(work.artist, work.title, work.year, 47, caption, sizeof(caption));
            EXPECT_LE(art_caption_char_count(caption), 47);
        }
    }
}

TEST(ArtSources, GarbageAndHugeAnswersAreRefused)
{
    art_work_t work;
    memset(&work, 0, sizeof(work));
    char a[160];
    std::string noise(2000, '{');
    EXPECT_FALSE(art_rijks_parse_object(noise.c_str(), noise.size(), &work, a, sizeof(a)));
    EXPECT_FALSE(art_smk_parse_item(noise.c_str(), noise.size(), &work));
    EXPECT_FALSE(art_si_parse_row(noise.c_str(), noise.size(), &work));
    std::string huge(200 * 1024, ' ');
    EXPECT_FALSE(art_smk_parse_item(huge.c_str(), huge.size(), &work));
    EXPECT_EQ(art_smk_found(huge.c_str(), huge.size()), -1);
}

// ---------------------------------------------------------------------------------------------
// Album rule

TEST(ArtStore, TheLimitsAreKeptInRange)
{
    int min = 20, target = 30;
    art_store_clamp_limits(&min, &target);
    EXPECT_EQ(min, 20);
    EXPECT_EQ(target, 30);
    min = 1;
    target = 2;
    art_store_clamp_limits(&min, &target);
    EXPECT_EQ(min, 5);
    EXPECT_EQ(target, 6);
    min = 90;
    target = 99;
    art_store_clamp_limits(&min, &target);
    EXPECT_EQ(min, 80);
    EXPECT_EQ(target, 95);
    min = 40;
    target = 30;  // not above the minimum
    art_store_clamp_limits(&min, &target);
    EXPECT_EQ(target, 41);
}

namespace
{
std::vector<art_item_t> items_of(std::initializer_list<unsigned> sizes)
{
    std::vector<art_item_t> items;
    uint32_t seq = 1;
    for (unsigned size : sizes) {
        art_item_t item;
        memset(&item, 0, sizeof(item));
        snprintf(item.base, sizeof(item.base), "p%u", seq);
        item.bytes = size;
        item.seq = seq++;
        items.push_back(item);
    }
    return items;
}
}  // namespace

TEST(ArtStore, WhileThereIsRoomNothingIsDeleted)
{
    auto items = items_of({60, 60});
    art_plan_t plan = art_store_plan(1000, 500, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_TRUE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 0);
}

TEST(ArtStore, AtTheMinimumTheOldestAreDeletedUpToTheTarget)
{
    auto items = items_of({60, 60, 60, 60});
    // 150 free, 10 for the new picture -> 140 (14 %): delete until 300 (30 %) -> 3 pictures
    art_plan_t plan = art_store_plan(1000, 150, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_TRUE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 3);
}

TEST(ArtStore, ExactlyAtTheMinimumCountsAsReached)
{
    auto items = items_of({100, 100});
    art_plan_t plan = art_store_plan(1000, 210, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_EQ(plan.delete_count, 1);  // 200 = 20 % exactly -> clean up (to 30 % = 300)
    EXPECT_TRUE(plan.can_save);
    plan = art_store_plan(1000, 211, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_EQ(plan.delete_count, 0);  // 201 is above the minimum
    EXPECT_TRUE(plan.can_save);
}

TEST(ArtStore, AboveTheMinimumButNotTheTargetEverythingIsDeleted)
{
    auto items = items_of({30, 30});
    // 155 free, 10 for the new picture -> 145; both pictures give 205: above 20 %, below 30 %
    art_plan_t plan = art_store_plan(1000, 155, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_TRUE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 2);
}

TEST(ArtStore, WhenDeletingEverythingDoesNotHelpNothingIsDeletedAndNothingSaved)
{
    auto items = items_of({10, 10});
    art_plan_t plan = art_store_plan(1000, 150, 10, 20, 30, items.data(), (int) items.size());
    EXPECT_FALSE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 0);
    plan = art_store_plan(1000, 150, 10, 20, 30, nullptr, 0);
    EXPECT_FALSE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 0);
}

TEST(ArtStore, ABigNewPictureCountsAgainstTheFreeSpace)
{
    auto items = items_of({100, 100, 100});
    // 400 free (40 %) but the picture takes 250 -> 150 (15 %): clean up to 300 -> 2 pictures
    art_plan_t plan = art_store_plan(1000, 400, 250, 20, 30, items.data(), (int) items.size());
    EXPECT_TRUE(plan.can_save);
    EXPECT_EQ(plan.delete_count, 2);
    plan = art_store_plan(1000, 100, 400, 20, 30, items.data(),
                          (int) items.size());  // takes more than is free
    EXPECT_TRUE(plan.can_save);                 // 0 + 300 = 300
    EXPECT_EQ(plan.delete_count, 3);
}

TEST(ArtStore, NoStorageNoSaving)
{
    art_plan_t plan = art_store_plan(0, 0, 10, 20, 30, nullptr, 0);
    EXPECT_FALSE(plan.can_save);
}

namespace
{
std::string make_temp_dir()
{
    char path[] = "/tmp/art_store_XXXXXX";
    char *made = mkdtemp(path);
    EXPECT_NE(made, nullptr);
    return made ? made : "";
}

void write_file(const std::string &path, size_t bytes)
{
    std::ofstream file(path, std::ios::binary);
    file << std::string(bytes, 'x');
}

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

art_work_t work_of(const char *id)
{
    art_work_t work;
    memset(&work, 0, sizeof(work));
    work.source = ART_SOURCE_RIJKS;
    snprintf(work.id, sizeof(work.id), "%s", id);
    snprintf(work.rights, sizeof(work.rights), "PDM");
    return work;
}
}  // namespace

TEST(ArtStore, TheFileBaseNamesSourceAndWork)
{
    art_work_t work = work_of("200100988");
    char base[ART_BASE_MAX];
    art_store_file_base(&work, base, sizeof(base));
    EXPECT_STREQ(base, "rijks-200100988");
    work.source = ART_SOURCE_SMITHSONIAN;
    snprintf(work.id, sizeof(work.id), "SAAM-1985_66");
    art_store_file_base(&work, base, sizeof(base));
    EXPECT_STREQ(base, "si-SAAM-1985_66");
}

TEST(ArtStore, TheCaptionFileOfAPicture)
{
    char path[128];
    ASSERT_TRUE(art_store_caption_path("/sdcard/Art/rijks-1.epdgz", path, sizeof(path)));
    EXPECT_STREQ(path, "/sdcard/Art/rijks-1.caption.json");
    ASSERT_TRUE(art_store_caption_path("/sdcard/Art/rijks-1.jpg", path, sizeof(path)));
    EXPECT_STREQ(path, "/sdcard/Art/rijks-1.caption.json");
    EXPECT_FALSE(art_store_caption_path("/sdcard/Art.d/noextension", path, sizeof(path)));
    EXPECT_FALSE(art_store_caption_path(nullptr, path, sizeof(path)));
    char small[8];
    EXPECT_FALSE(art_store_caption_path("/sdcard/Art/rijks-1.png", small, sizeof(small)));
}

TEST(ArtStore, ACaptionIsWrittenAndReadBack)
{
    std::string dir = make_temp_dir();
    art_work_t work = work_of("200100988");
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-200100988", 7, &work,
                                        "Jan Toorop - Käse (1899)"));
    char text[ART_CAPTION_TEXT_MAX];
    ASSERT_TRUE(
        art_store_read_caption((dir + "/rijks-200100988.epdgz").c_str(), text, sizeof(text)));
    EXPECT_STREQ(text, "Jan Toorop - Käse (1899)");
    ASSERT_TRUE(art_store_read_caption((dir + "/rijks-200100988.jpg").c_str(), text, sizeof(text)));
    EXPECT_FALSE(art_store_read_caption((dir + "/other.png").c_str(), text, sizeof(text)));  // none
    EXPECT_STREQ(text, "");
    art_store_remove(dir.c_str(), "rijks-200100988");
    rmdir(dir.c_str());
}

TEST(ArtStore, OnlyCaptionFilesOfThisOptionMakeAPictureOurs)
{
    std::string dir = make_temp_dir();
    // a caption file of another kind, one that is not JSON, one without text
    {
        std::ofstream f(dir + "/photo.caption.json");
        f << "{\"kind\":\"photo\",\"v\":1,\"n\":1,\"text\":\"mine\"}";
    }
    {
        std::ofstream f(dir + "/broken.caption.json");
        f << "not json";
    }
    {
        std::ofstream f(dir + "/old.caption.json");
        f << "{\"kind\":\"art\",\"v\":2,\"n\":1,\"text\":\"future version\"}";
    }
    write_file(dir + "/photo.png", 500);
    write_file(dir + "/broken.png", 500);
    art_item_t items[8];
    EXPECT_EQ(art_store_scan(dir.c_str(), items, 8), 0);
    char text[ART_CAPTION_TEXT_MAX];
    EXPECT_FALSE(art_store_read_caption((dir + "/photo.png").c_str(), text, sizeof(text)));
    for (const char *name : {"photo.caption.json", "broken.caption.json", "old.caption.json",
                             "photo.png", "broken.png"}) {
        unlink((dir + "/" + name).c_str());
    }
    rmdir(dir.c_str());
}

TEST(ArtStore, ThePicturesAreListedOldestFirstWithAllTheirBytes)
{
    std::string dir = make_temp_dir();
    art_work_t a = work_of("a"), b = work_of("b"), c = work_of("c");
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-a", 5, &a, "A"));
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-b", 2, &b, "B"));
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-c", 9, &c, "C"));
    write_file(dir + "/rijks-a.epdgz", 1000);
    write_file(dir + "/rijks-a.jpg", 100);
    write_file(dir + "/rijks-b.png", 2000);
    write_file(dir + "/rijks-c.epdgz", 300);
    write_file(dir + "/user-photo.epdgz", 5000);  // not ours: no caption file
    art_item_t items[8];
    ASSERT_EQ(art_store_scan(dir.c_str(), items, 8), 3);
    EXPECT_STREQ(items[0].base, "rijks-b");
    EXPECT_STREQ(items[1].base, "rijks-a");
    EXPECT_STREQ(items[2].base, "rijks-c");
    EXPECT_EQ(items[0].seq, 2u);
    EXPECT_GE(items[0].bytes, 2000u);
    EXPECT_GE(items[1].bytes, 1100u);  // picture and thumbnail
    EXPECT_LT(items[1].bytes, 1100u + 200u);
    EXPECT_EQ(art_store_scan(dir.c_str(), items, 2), 2);  // the limit
    EXPECT_EQ(art_store_scan("/tmp/art_store_missing_dir", items, 8), 0);

    for (const char *base : {"rijks-a", "rijks-b", "rijks-c"}) {
        art_store_remove(dir.c_str(), base);
    }
    unlink((dir + "/user-photo.epdgz").c_str());
    rmdir(dir.c_str());
}

TEST(ArtStore, ThePicturesDeletedByTheUserLeaveNothingBehind)
{
    std::string dir = make_temp_dir();
    art_work_t kept = work_of("kept"), gone = work_of("gone"), thumb_only = work_of("thumb");
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-kept", 3, &kept, "K"));
    write_file(dir + "/rijks-kept.epdgz", 400);
    // the picture was deleted in the web gallery: its caption file (and maybe the thumbnail) stay
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-gone", 1, &gone, "G"));
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-thumb", 2, &thumb_only, "T"));
    write_file(dir + "/rijks-thumb.jpg", 100);  // only the preview is left: no picture either
    write_file(dir + "/user-photo.epdgz", 900);
    art_item_t items[8];
    ASSERT_EQ(art_store_scan(dir.c_str(), items, 8), 1);
    EXPECT_STREQ(items[0].base, "rijks-kept");
    EXPECT_FALSE(exists(dir + "/rijks-gone.caption.json"));
    EXPECT_FALSE(exists(dir + "/rijks-thumb.caption.json"));
    EXPECT_FALSE(exists(dir + "/rijks-thumb.jpg"));
    EXPECT_TRUE(exists(dir + "/rijks-kept.caption.json"));  // what is there stays
    EXPECT_TRUE(exists(dir + "/rijks-kept.epdgz"));
    EXPECT_TRUE(exists(dir + "/user-photo.epdgz"));
    art_store_remove(dir.c_str(), "rijks-kept");
    unlink((dir + "/user-photo.epdgz").c_str());
    rmdir(dir.c_str());
}

TEST(ArtStore, ManyLeftOversAreCleanedUpOverSeveralScans)
{
    std::string dir = make_temp_dir();
    for (int i = 0; i < 20; i++) {
        art_work_t work = work_of("x");
        std::string base = "rijks-orphan" + std::to_string(i);
        ASSERT_TRUE(art_store_write_caption(dir.c_str(), base.c_str(), (uint32_t) i, &work, "O"));
    }
    art_item_t items[4];
    auto left = [&dir]() {
        int n = 0;
        for (int i = 0; i < 20; i++) {
            n += exists(dir + "/rijks-orphan" + std::to_string(i) + ".caption.json") ? 1 : 0;
        }
        return n;
    };
    EXPECT_EQ(art_store_scan(dir.c_str(), items, 4), 0);
    EXPECT_EQ(left(), 12);  // eight per scan
    EXPECT_EQ(art_store_scan(dir.c_str(), items, 4), 0);
    EXPECT_EQ(left(), 4);
    EXPECT_EQ(art_store_scan(dir.c_str(), items, 4), 0);
    EXPECT_EQ(left(), 0);
    rmdir(dir.c_str());
}

TEST(ArtStore, RemovingAPictureTakesAllItsFilesAndNothingElse)
{
    std::string dir = make_temp_dir();
    art_work_t a = work_of("a");
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-a", 1, &a, "A"));
    write_file(dir + "/rijks-a.epdgz", 10);
    write_file(dir + "/rijks-a.jpg", 10);
    write_file(dir + "/rijks-ab.epdgz", 10);  // another picture whose name starts the same
    write_file(dir + "/note.txt", 10);
    art_store_remove(dir.c_str(), "rijks-a");
    EXPECT_FALSE(exists(dir + "/rijks-a.epdgz"));
    EXPECT_FALSE(exists(dir + "/rijks-a.jpg"));
    EXPECT_FALSE(exists(dir + "/rijks-a.caption.json"));
    EXPECT_TRUE(exists(dir + "/rijks-ab.epdgz"));
    EXPECT_TRUE(exists(dir + "/note.txt"));
    unlink((dir + "/rijks-ab.epdgz").c_str());
    unlink((dir + "/note.txt").c_str());
    rmdir(dir.c_str());
}

TEST(ArtStore, ThePlanDeletesWhatTheScanListedOldestFirst)
{
    std::string dir = make_temp_dir();
    std::vector<std::string> bases = {"rijks-new", "rijks-old", "rijks-mid"};
    uint32_t seqs[] = {30, 10, 20};
    for (size_t i = 0; i < bases.size(); i++) {
        art_work_t w = work_of(bases[i].c_str() + 6);
        ASSERT_TRUE(art_store_write_caption(dir.c_str(), bases[i].c_str(), seqs[i], &w, "x"));
        write_file(dir + "/" + bases[i] + ".epdgz", 100);
    }
    art_item_t items[8];
    int count = art_store_scan(dir.c_str(), items, 8);
    ASSERT_EQ(count, 3);
    // a medium of 1000 with 120 free: the new picture takes 10 -> 110 (11 %); how many of the
    // oldest are needed to reach 300 (30 %) follows from their sizes (picture and caption file)
    int needed = 0;
    uint64_t free_after = 110;
    while (needed < count && free_after < 300) {
        free_after += items[needed++].bytes;
    }
    ASSERT_GE(needed, 1);
    ASSERT_LT(needed, count);  // the picture sizes leave the newest one alone
    art_plan_t plan = art_store_plan(1000, 120, 10, 20, 30, items, count);
    ASSERT_TRUE(plan.can_save);
    ASSERT_EQ(plan.delete_count, needed);
    for (int i = 0; i < plan.delete_count; i++) {
        art_store_remove(dir.c_str(), items[i].base);
    }
    EXPECT_FALSE(exists(dir + "/rijks-old.epdgz"));  // the oldest goes first
    EXPECT_TRUE(exists(dir + "/rijks-new.epdgz"));   // the newest stays
    art_store_remove(dir.c_str(), "rijks-new");
    rmdir(dir.c_str());
}

// ---------------------------------------------------------------------------------------------
// The scale mode and the orientation

TEST(ArtScale, FitIsTheDefaultAndOnlyCoverIsCover)
{
    EXPECT_EQ(art_scale_from_name("cover"), ART_SCALE_COVER);
    EXPECT_EQ(art_scale_from_name("fit"), ART_SCALE_FIT);
    EXPECT_EQ(art_scale_from_name(""), ART_SCALE_FIT);
    EXPECT_EQ(art_scale_from_name("Cover"), ART_SCALE_FIT);  // exact names only
    EXPECT_EQ(art_scale_from_name("covers"), ART_SCALE_FIT);
    EXPECT_EQ(art_scale_from_name(nullptr), ART_SCALE_FIT);
    EXPECT_STREQ(art_scale_name(ART_SCALE_FIT), "fit");
    EXPECT_STREQ(art_scale_name(ART_SCALE_COVER), "cover");
    EXPECT_EQ(art_scale_from_name(art_scale_name(ART_SCALE_COVER)), ART_SCALE_COVER);
}

TEST(ArtOrientation, ThePanelBoxFollowsTheFramesOrientation)
{
    int w = 0, h = 0;
    art_panel_box(800, 480, true, &w, &h);  // a landscape panel set to landscape
    EXPECT_EQ(w, 800);
    EXPECT_EQ(h, 480);
    art_panel_box(800, 480, false, &w, &h);  // the same panel set to portrait
    EXPECT_EQ(w, 480);
    EXPECT_EQ(h, 800);
    art_panel_box(480, 800, true, &w, &h);  // a panel that is portrait by itself
    EXPECT_EQ(w, 800);
    EXPECT_EQ(h, 480);
    art_panel_box(480, 800, false, &w, &h);
    EXPECT_EQ(w, 480);
    EXPECT_EQ(h, 800);
    art_panel_box(1200, 1200, false, &w, &h);  // a square one has no orientation
    EXPECT_EQ(w, 1200);
    EXPECT_EQ(h, 1200);
}

TEST(ArtOrientation, APictureMatchesWhenItsLongerSideIsTheFramesLongerSide)
{
    EXPECT_TRUE(art_orientation_matches(1600, 900, true));
    EXPECT_FALSE(art_orientation_matches(1600, 900, false));
    EXPECT_TRUE(art_orientation_matches(322, 480, false));
    EXPECT_FALSE(art_orientation_matches(322, 480, true));
    EXPECT_TRUE(art_orientation_matches(1000, 1000, true));  // a square one fits both
    EXPECT_TRUE(art_orientation_matches(1000, 1000, false));
    EXPECT_TRUE(art_orientation_matches(801, 800, true));  // by the pixel, no tolerance
}

TEST(ArtOrientation, AnUnknownSizeFitsEverything)
{
    for (bool landscape : {true, false}) {
        EXPECT_TRUE(art_orientation_matches(0, 0, landscape));
        EXPECT_TRUE(art_orientation_matches(0, 480, landscape));
        EXPECT_TRUE(art_orientation_matches(800, 0, landscape));
        EXPECT_TRUE(art_orientation_matches(-1, -1, landscape));
    }
}

namespace
{
struct Probe {
    std::vector<bool> match;
    std::vector<int> asked;
};

bool probe_matches(int index, void *context)
{
    Probe *probe = static_cast<Probe *>(context);
    probe->asked.push_back(index);
    return probe->match[index];
}
}  // namespace

TEST(ArtPick, NothingToPickFromGivesNone)
{
    EXPECT_EQ(art_pick_matching(0, 5, 24, nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(art_pick_matching(-3, 5, 24, nullptr, nullptr, nullptr), -1);
}

TEST(ArtPick, WithoutAQuestionTheRandomStartWins)
{
    bool matched = true;
    EXPECT_EQ(art_pick_matching(10, 23, 24, nullptr, nullptr, &matched), 3);
    EXPECT_FALSE(matched);  // nobody was asked
    Probe probe{{true, true, true}, {}};
    EXPECT_EQ(art_pick_matching(3, 7, 0, probe_matches, &probe, nullptr),
              1);  // no probes: the start
    EXPECT_TRUE(probe.asked.empty());
}

TEST(ArtPick, TheFirstMatchingEntryFromTheStartWins)
{
    Probe probe{{false, false, true, false, true, false}, {}};
    bool matched = false;
    EXPECT_EQ(art_pick_matching(6, 3, 24, probe_matches, &probe, &matched), 4);  // 3 no, 4 yes
    EXPECT_TRUE(matched);
    EXPECT_EQ(probe.asked, (std::vector<int>{3, 4}));
}

TEST(ArtPick, TheSearchWrapsRound)
{
    Probe probe{{true, false, false, false}, {}};
    bool matched = false;
    EXPECT_EQ(art_pick_matching(4, 2, 24, probe_matches, &probe, &matched), 0);  // 2, 3 no, 0 yes
    EXPECT_TRUE(matched);
    EXPECT_EQ(probe.asked, (std::vector<int>{2, 3, 0}));
}

TEST(ArtPick, WhenNothingMatchesTheStartWins)
{
    Probe probe{{false, false, false}, {}};
    bool matched = true;  // set to false by the call
    EXPECT_EQ(art_pick_matching(3, 1, 24, probe_matches, &probe, &matched), 1);
    EXPECT_FALSE(matched);
    EXPECT_EQ(probe.asked.size(), 3u);  // every entry was asked once, none twice
}

TEST(ArtPick, OnlyTheGivenNumberOfEntriesIsAsked)
{
    Probe probe{std::vector<bool>(100, false), {}};
    probe.match[60] = true;  // out of reach of 24 probes from 10
    bool matched = true;
    EXPECT_EQ(art_pick_matching(100, 10, 24, probe_matches, &probe, &matched), 10);
    EXPECT_FALSE(matched);
    EXPECT_EQ(probe.asked.size(), 24u);
}

TEST(ArtPick, AHugeRandomNumberStaysInRange)
{
    Probe probe{{false, false, false, false, false}, {}};
    int pick = art_pick_matching(5, 0xFFFFFFFFu, 24, probe_matches, &probe, nullptr);
    EXPECT_EQ(pick, (int) (0xFFFFFFFFu % 5u));
}

TEST(ArtImage, TheSizeComesFromTheFrameHeader)
{
    auto baseline = jpeg_with(0xC0);
    int w = 0, h = 0;
    ASSERT_TRUE(art_jpeg_size(baseline.data(), baseline.size(), &w, &h));
    EXPECT_EQ(w, 16);
    EXPECT_EQ(h, 16);

    // a portrait picture: height 480 = 0x01E0, width 322 = 0x0142
    auto portrait = jpeg_with(0xC0);
    portrait[23] = 0x01;
    portrait[24] = 0xE0;
    portrait[25] = 0x01;
    portrait[26] = 0x42;
    ASSERT_TRUE(art_jpeg_size(portrait.data(), portrait.size(), &w, &h));
    EXPECT_EQ(w, 322);
    EXPECT_EQ(h, 480);

    // any coding has a size, also the ones the frame cannot decode
    auto progressive = jpeg_with(0xC2);
    ASSERT_TRUE(art_jpeg_size(progressive.data(), progressive.size(), &w, &h));
    EXPECT_EQ(w, 16);
}

TEST(ArtImage, NoSizeFromWhatIsNotAJpegOrCutOff)
{
    int w = 7, h = 7;
    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    EXPECT_FALSE(art_jpeg_size(png.data(), png.size(), &w, &h));
    EXPECT_FALSE(art_jpeg_size(nullptr, 100, &w, &h));
    auto good = jpeg_with(0xC0);
    for (size_t cut = 0; cut < 27; cut++) {  // the size is the 9th byte of the frame header
        EXPECT_FALSE(art_jpeg_size(good.data(), cut, &w, &h)) << cut;
    }
    EXPECT_EQ(w, 7);  // the refused ones left the answer alone
    EXPECT_EQ(h, 7);
    EXPECT_TRUE(art_jpeg_size(good.data(), 27, &w, &h));  // the size is the last thing it needs
    EXPECT_EQ(w, 16);
    auto zero = jpeg_with(0xC0);
    zero[23] = zero[24] = zero[25] = zero[26] = 0;  // a picture of 0 x 0 is no size
    w = h = 7;
    EXPECT_FALSE(art_jpeg_size(zero.data(), zero.size(), &w, &h));
    EXPECT_EQ(h, 7);
    auto short_header = jpeg_with(0xC0);
    short_header[21] = 0x05;  // a frame header too short to hold a size
    EXPECT_FALSE(art_jpeg_size(short_header.data(), short_header.size(), &w, &h));
}

TEST(ArtSmk, TheSizeOfTheOriginalComesWithTheRecord)
{
    std::string json = fixture("smk-item-painting.json");
    art_work_t work;
    ASSERT_TRUE(art_smk_parse_item(json.c_str(), json.size(), &work));
    EXPECT_EQ(work.width, 5493);
    EXPECT_EQ(work.height, 6937);
    EXPECT_FALSE(art_orientation_matches(work.width, work.height, true));  // a portrait work
    EXPECT_TRUE(art_orientation_matches(work.width, work.height, false));

    json = fixture("smk-item-print.json");
    ASSERT_TRUE(art_smk_parse_item(json.c_str(), json.size(), &work));
    EXPECT_EQ(work.width, 4992);
    EXPECT_EQ(work.height, 6287);
}

TEST(ArtSmk, ARecordWithoutASizeLeavesItUnknown)
{
    std::string json = fixture("smk-item-painting.json");
    std::string no_height = replaced(json, "\"image_height\"", "\"image_hoejde\"");
    art_work_t work;
    memset(&work, 0x5A, sizeof(work));
    ASSERT_TRUE(art_smk_parse_item(no_height.c_str(), no_height.size(), &work));
    EXPECT_EQ(work.width, 0);  // both or none
    EXPECT_EQ(work.height, 0);
    EXPECT_TRUE(art_orientation_matches(work.width, work.height, true));

    std::string text = replaced(json, "\"image_width\":5493", "\"image_width\":\"wide\"");
    ASSERT_TRUE(art_smk_parse_item(text.c_str(), text.size(), &work));
    EXPECT_EQ(work.width, 0);
    std::string negative = replaced(json, "\"image_width\":5493", "\"image_width\":-5");
    ASSERT_TRUE(art_smk_parse_item(negative.c_str(), negative.size(), &work));
    EXPECT_EQ(work.width, 0);
    EXPECT_EQ(work.height, 0);
}

TEST(ArtStore, TheSizeOfTheOriginalIsKeptInTheCaptionFile)
{
    std::string dir = make_temp_dir();
    art_work_t work = work_of("200100988");
    work.width = 322;
    work.height = 480;
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-200100988", 7, &work, "x"));
    int w = 0, h = 0;
    ASSERT_TRUE(art_store_read_size((dir + "/rijks-200100988.epdgz").c_str(), &w, &h));
    EXPECT_EQ(w, 322);
    EXPECT_EQ(h, 480);
    ASSERT_TRUE(art_store_read_size((dir + "/rijks-200100988.jpg").c_str(), &w, &h));  // same file
    EXPECT_EQ(h, 480);
    char text[ART_CAPTION_TEXT_MAX];
    ASSERT_TRUE(art_store_read_caption((dir + "/rijks-200100988.png").c_str(), text, sizeof(text)));
    EXPECT_STREQ(text, "x");  // the caption reads as before
    art_store_remove(dir.c_str(), "rijks-200100988");
    rmdir(dir.c_str());
}

TEST(ArtStore, NoSizeWhereNoneWasKnownOrThereIsNoCaptionFile)
{
    std::string dir = make_temp_dir();
    art_work_t work = work_of("200100989");  // width and height 0: unknown
    ASSERT_TRUE(art_store_write_caption(dir.c_str(), "rijks-200100989", 8, &work, "x"));
    int w = 11, h = 12;
    EXPECT_FALSE(art_store_read_size((dir + "/rijks-200100989.epdgz").c_str(), &w, &h));
    EXPECT_EQ(w, 11);  // the answer is left alone
    EXPECT_EQ(h, 12);
    EXPECT_FALSE(art_store_read_size((dir + "/other.epdgz").c_str(), &w, &h));  // no caption file
    EXPECT_FALSE(art_store_read_size(nullptr, &w, &h));

    // a caption file of an older version of this option (no size) still reads
    char text[ART_CAPTION_TEXT_MAX];
    EXPECT_TRUE(
        art_store_read_caption((dir + "/rijks-200100989.epdgz").c_str(), text, sizeof(text)));

    // a file of another kind, or a size that is no number, gives none
    std::ofstream(dir + "/foreign.caption.json")
        << "{\"kind\":\"other\",\"v\":1,\"w\":10,\"h\":20}";
    EXPECT_FALSE(art_store_read_size((dir + "/foreign.epdgz").c_str(), &w, &h));
    std::ofstream(dir + "/odd.caption.json")
        << "{\"kind\":\"art\",\"v\":1,\"text\":\"x\",\"w\":\"10\",\"h\":20}";
    EXPECT_FALSE(art_store_read_size((dir + "/odd.epdgz").c_str(), &w, &h));
    std::ofstream(dir + "/neg.caption.json")
        << "{\"kind\":\"art\",\"v\":1,\"text\":\"x\",\"w\":-10,\"h\":20}";
    EXPECT_FALSE(art_store_read_size((dir + "/neg.epdgz").c_str(), &w, &h));

    art_store_remove(dir.c_str(), "rijks-200100989");
    unlink((dir + "/foreign.caption.json").c_str());
    unlink((dir + "/odd.caption.json").c_str());
    unlink((dir + "/neg.caption.json").c_str());
    rmdir(dir.c_str());
}
