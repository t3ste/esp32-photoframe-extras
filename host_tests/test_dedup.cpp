#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "dedup.h"
}

namespace
{

std::string hex_of(const dedup_digest_t &d)
{
    char hex[DEDUP_HEX_LEN + 1];
    dedup_digest_to_hex(&d, hex);
    return hex;
}

// An album directory in the working directory, emptied for every test.
class Dedup : public ::testing::Test
{
   protected:
    std::string dir = "dedup_test_album";

    void SetUp() override
    {
        std::string cmd = "rm -rf " + dir;
        ASSERT_EQ(system(cmd.c_str()), 0);
        ASSERT_EQ(mkdir(dir.c_str(), 0755), 0);
    }
    void TearDown() override
    {
        std::string cmd = "rm -rf " + dir;
        (void) !system(cmd.c_str());
    }

    void write(const std::string &name, const std::string &content)
    {
        FILE *fp = fopen((dir + "/" + name).c_str(), "wb");
        ASSERT_NE(fp, nullptr);
        fwrite(content.data(), 1, content.size(), fp);
        fclose(fp);
    }

    std::string slurp(const std::string &name)
    {
        FILE *fp = fopen((dir + "/" + name).c_str(), "rb");
        if (!fp) {
            return "<missing>";
        }
        std::string data;
        char chunk[4096];
        size_t n;
        while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
            data.append(chunk, n);
        }
        fclose(fp);
        return data;
    }

    dedup_digest_t md5_of(const std::string &content)
    {
        write("md5_probe.tmp", content);
        dedup_digest_t d;
        EXPECT_EQ(dedup_md5_file((dir + "/md5_probe.tmp").c_str(), &d), ESP_OK);
        unlink((dir + "/md5_probe.tmp").c_str());
        return d;
    }

    dedup_digest_t digest(const char *hex)
    {
        dedup_digest_t d;
        EXPECT_TRUE(dedup_digest_from_hex(hex, &d));
        return d;
    }
};

}  // namespace

TEST_F(Dedup, Md5MatchesTheRfcVectors)
{
    EXPECT_EQ(hex_of(md5_of("")), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(hex_of(md5_of("abc")), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(hex_of(md5_of("The quick brown fox jumps over the lazy dog")),
              "9e107d9d372bb6826bd81d3542a419d6");
    EXPECT_EQ(hex_of(md5_of("12345678901234567890123456789012345678901234567890123456789012345678"
                            "901234567890")),
              "57edf4a22be3c955ac49da2e2107b67a");
}

TEST_F(Dedup, Md5OfAFileLargerThanTheReadBuffer)
{
    // 1,000,000 x 'a' (RFC test data): several read chunks, block boundaries not aligned
    EXPECT_EQ(hex_of(md5_of(std::string(1000000, 'a'))), "7707d6ae4e027c70eea2a935c2296f21");
}

TEST_F(Dedup, Md5OfMissingFile)
{
    dedup_digest_t d;
    EXPECT_EQ(dedup_md5_file((dir + "/nope.png").c_str(), &d), ESP_ERR_NOT_FOUND);
}

TEST_F(Dedup, HexRoundTrip)
{
    dedup_digest_t d = digest("00ff10aB9c8d7e6f5a4b3c2d1e0f1234");
    EXPECT_EQ(hex_of(d), "00ff10ab9c8d7e6f5a4b3c2d1e0f1234");  // lower case out
    dedup_digest_t bad;
    EXPECT_FALSE(dedup_digest_from_hex("zz000000000000000000000000000000", &bad));
    EXPECT_FALSE(dedup_digest_from_hex("00ff10ab9c8d7e6f5a4b3c2d1e0f123", &bad));  // one short
    EXPECT_TRUE(dedup_digest_equal(&d, &d));
}

TEST_F(Dedup, ImageNames)
{
    EXPECT_TRUE(dedup_is_image_name("a.png"));
    EXPECT_TRUE(dedup_is_image_name("B.EPDGZ"));
    EXPECT_TRUE(dedup_is_image_name("c.d.bmp"));
    EXPECT_FALSE(dedup_is_image_name("a.jpg"));  // thumbnails are no images of their own
    EXPECT_FALSE(dedup_is_image_name(".dedup"));
    EXPECT_FALSE(dedup_is_image_name("noext"));
    EXPECT_FALSE(dedup_is_image_name(nullptr));
}

TEST_F(Dedup, FindsWhatWasRecorded)
{
    write("one.png", "x");
    dedup_digest_t d = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &d, "one.png"), ESP_OK);
    char found[64] = "";
    EXPECT_TRUE(
        dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &d, nullptr, found, sizeof(found)));
    EXPECT_STREQ(found, "one.png");
    EXPECT_EQ(slurp(DEDUP_INDEX_NAME), "s 900150983cd24fb0d6963f7d28e17f72 one.png\n");
}

TEST_F(Dedup, NoMatchForOtherDigestOrKind)
{
    write("one.png", "x");
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    dedup_digest_t b = digest("d41d8cd98f00b204e9800998ecf8427e");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "one.png"), ESP_OK);
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &b, nullptr, nullptr, 0));
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_PAYLOAD, &a, nullptr, nullptr, 0));
}

TEST_F(Dedup, NoIndexMeansNoMatch)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr, nullptr, 0));
    EXPECT_FALSE(dedup_index_has(dir.c_str(), DEDUP_HASH_STORED, "one.png"));
}

TEST_F(Dedup, AnEntryForAGoneFileIsIgnored)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "gone.png"), ESP_OK);
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr, nullptr, 0));
    write("gone.png", "back");
    EXPECT_TRUE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr, nullptr, 0));
}

TEST_F(Dedup, TheReplacedFileDoesNotCountAgainstItself)
{
    write("same.png", "x");
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "same.png"), ESP_OK);
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, "same.png", nullptr, 0));
    write("other.png", "x");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "other.png"), ESP_OK);
    char found[64];
    EXPECT_TRUE(
        dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, "same.png", found, sizeof(found)));
    EXPECT_STREQ(found, "other.png");
}

TEST_F(Dedup, SettingAgainReplacesTheEntry)
{
    write("a.png", "x");
    dedup_digest_t old_d = digest("900150983cd24fb0d6963f7d28e17f72");
    dedup_digest_t new_d = digest("d41d8cd98f00b204e9800998ecf8427e");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &old_d, "a.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &new_d, "a.png"), ESP_OK);
    EXPECT_FALSE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &old_d, nullptr, nullptr, 0));
    EXPECT_TRUE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &new_d, nullptr, nullptr, 0));
    EXPECT_EQ(slurp(DEDUP_INDEX_NAME), "s d41d8cd98f00b204e9800998ecf8427e a.png\n");
}

TEST_F(Dedup, ReplacingDropsTheOtherKindToo)
{
    write("a.png", "x");
    dedup_digest_t d = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_PAYLOAD, &d, "a.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &d, "a.png"), ESP_OK);
    EXPECT_TRUE(dedup_index_has(dir.c_str(), DEDUP_HASH_STORED, "a.png"));
    EXPECT_FALSE(
        dedup_index_has(dir.c_str(), DEDUP_HASH_PAYLOAD, "a.png"));  // stale after a replace
}

TEST_F(Dedup, RemoveKeepsTheOthers)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    dedup_digest_t b = digest("d41d8cd98f00b204e9800998ecf8427e");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "a.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &b, "b.png"), ESP_OK);
    ASSERT_EQ(dedup_index_remove(dir.c_str(), "a.png"), ESP_OK);
    EXPECT_FALSE(dedup_index_has(dir.c_str(), DEDUP_HASH_STORED, "a.png"));
    EXPECT_TRUE(dedup_index_has(dir.c_str(), DEDUP_HASH_STORED, "b.png"));
    EXPECT_EQ(slurp(DEDUP_INDEX_NAME), "s d41d8cd98f00b204e9800998ecf8427e b.png\n");
    EXPECT_EQ(dedup_index_remove(dir.c_str(), "never-there.png"), ESP_OK);
    EXPECT_EQ(access((dir + "/.dedup.tmp").c_str(), F_OK), -1);  // no temp file is left behind
}

TEST_F(Dedup, NamesWithSpacesAndUmlauts)
{
    const std::string name = "Urlaub am See (Bäume) 1.png";
    write(name, "x");
    dedup_digest_t d = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &d, name.c_str()), ESP_OK);
    char found[128];
    ASSERT_TRUE(
        dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &d, nullptr, found, sizeof(found)));
    EXPECT_EQ(std::string(found), name);
    EXPECT_TRUE(dedup_index_has(dir.c_str(), DEDUP_HASH_STORED, name.c_str()));
}

TEST_F(Dedup, GarbageLinesAreIgnoredAndDroppedOnRewrite)
{
    write("a.png", "x");
    write(DEDUP_INDEX_NAME,
          "not an entry\n"
          "s zz0150983cd24fb0d6963f7d28e17f72 bad-hex.png\n"
          "x 900150983cd24fb0d6963f7d28e17f72 bad-kind.png\n"
          "s 900150983cd24fb0d6963f7d28e17f72 a.png\n"
          "s 900150983cd24fb0d6963f7d28e17f72\n");
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    EXPECT_TRUE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr, nullptr, 0));
    dedup_digest_t b = digest("d41d8cd98f00b204e9800998ecf8427e");
    write("b.png", "y");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &b, "a.png"), ESP_OK);
    EXPECT_EQ(slurp(DEDUP_INDEX_NAME), "s d41d8cd98f00b204e9800998ecf8427e a.png\n");
}

TEST_F(Dedup, OverlongLineIsSkippedWhole)
{
    write("a.png", "x");
    write(DEDUP_INDEX_NAME, "s 900150983cd24fb0d6963f7d28e17f72 " + std::string(2000, 'n') +
                                ".png\n" + "s d41d8cd98f00b204e9800998ecf8427e a.png\n");
    dedup_digest_t b = digest("d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_TRUE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &b, nullptr, nullptr, 0));
}

TEST_F(Dedup, LastLineWithoutNewline)
{
    write("a.png", "x");
    write(DEDUP_INDEX_NAME, "s 900150983cd24fb0d6963f7d28e17f72 a.png");
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    EXPECT_TRUE(dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr, nullptr, 0));
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "c.png"), ESP_OK);
    EXPECT_EQ(
        slurp(DEDUP_INDEX_NAME),
        "s 900150983cd24fb0d6963f7d28e17f72 a.png\ns 900150983cd24fb0d6963f7d28e17f72 c.png\n");
}

TEST_F(Dedup, RefusesNamesThatWouldBreakTheFormat)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, ""), ESP_ERR_INVALID_ARG);
    EXPECT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "x\ny.png"), ESP_ERR_INVALID_ARG);
    EXPECT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, nullptr), ESP_ERR_INVALID_ARG);
}

TEST_F(Dedup, ManyEntries)
{
    for (int i = 0; i < 500; i++) {
        char name[32];
        snprintf(name, sizeof(name), "img%03d.png", i);
        write(name, "x");
        dedup_digest_t d = md5_of(std::to_string(i));
        ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &d, name), ESP_OK);
    }
    dedup_digest_t d = md5_of("321");
    char found[64];
    ASSERT_TRUE(
        dedup_index_find(dir.c_str(), DEDUP_HASH_STORED, &d, nullptr, found, sizeof(found)));
    EXPECT_STREQ(found, "img321.png");
}

namespace
{

using Groups = std::vector<std::vector<std::string>>;

void collect_group(const char *const *names, int count, void *ctx)
{
    Groups *groups = static_cast<Groups *>(ctx);
    groups->emplace_back(names, names + count);
}

}  // namespace

TEST_F(Dedup, ReportGroupsTheFilesWithOneDigest)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    dedup_digest_t b = digest("d41d8cd98f00b204e9800998ecf8427e");
    dedup_digest_t c = digest("9e107d9d372bb6826bd81d3542a419d6");
    const struct {
        const char *name;
        const dedup_digest_t *d;
    } files[] = {{"z.png", &a},   {"a.png", &a}, {"m.epdgz", &a},
                 {"one.png", &b}, {"p.png", &c}, {"q.png", &c}};
    for (const auto &f : files) {
        write(f.name, "x");
        ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, f.d, f.name), ESP_OK);
    }
    Groups groups;
    int images = -1, indexed = -1;
    ASSERT_EQ(dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, collect_group, &groups, &images,
                                 &indexed),
              ESP_OK);
    EXPECT_EQ(images, 6);
    EXPECT_EQ(indexed, 6);
    std::sort(groups.begin(), groups.end());
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0], (std::vector<std::string>{"a.png", "m.epdgz", "z.png"}));  // sorted names
    EXPECT_EQ(groups[1], (std::vector<std::string>{"p.png", "q.png"}));
}

TEST_F(Dedup, ReportIgnoresGoneFilesOtherKindsAndNotesTheUnindexed)
{
    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    write("one.png", "x");
    write("two.png", "x");
    write("three.png", "x");  // no entry at all
    write("thumb.jpg", "x");  // not an image of its own
    write(".hidden.png", "x");
    write("._resource.png", "x");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "one.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "two.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "gone.png"), ESP_OK);
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_PAYLOAD, &a, "three.png"), ESP_OK);

    Groups groups;
    int images = 0, indexed = 0;
    ASSERT_EQ(dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, collect_group, &groups, &images,
                                 &indexed),
              ESP_OK);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0], (std::vector<std::string>{"one.png", "two.png"}));  // "gone" is not there
    EXPECT_EQ(images, 3);                                                    // one, two, three
    EXPECT_EQ(indexed, 2);  // three has only a payload entry; gone has no file
}

TEST_F(Dedup, ReportWithoutIndexOrDuplicates)
{
    write("only.png", "x");
    Groups groups;
    int images = -1, indexed = -1;
    ASSERT_EQ(dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, collect_group, &groups, &images,
                                 &indexed),
              ESP_OK);
    EXPECT_TRUE(groups.empty());
    EXPECT_EQ(images, 1);
    EXPECT_EQ(indexed, 0);

    dedup_digest_t a = digest("900150983cd24fb0d6963f7d28e17f72");
    ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &a, "only.png"), ESP_OK);
    ASSERT_EQ(dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, collect_group, &groups, &images,
                                 &indexed),
              ESP_OK);
    EXPECT_TRUE(groups.empty());  // one file is no duplicate
    EXPECT_EQ(indexed, 1);
    // no callback and no counters are fine as well
    EXPECT_EQ(
        dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, nullptr, nullptr, nullptr, nullptr),
        ESP_OK);
}

TEST_F(Dedup, ReportOfAMissingFolder)
{
    Groups groups;
    int images = -1, indexed = -1;
    EXPECT_EQ(dedup_index_groups("dedup_no_such_folder", DEDUP_HASH_STORED, collect_group, &groups,
                                 &images, &indexed),
              ESP_OK);
    EXPECT_EQ(images, 0);
    EXPECT_EQ(indexed, 0);
}

TEST_F(Dedup, ReportOfAWholeAlbum)
{
    // 300 files in 100 pairs and triples of one picture each - all found
    for (int i = 0; i < 300; i++) {
        char name[32];
        snprintf(name, sizeof(name), "img%03d.png", i);
        write(name, "x");
        dedup_digest_t d = md5_of(std::to_string(i / 3));
        ASSERT_EQ(dedup_index_set(dir.c_str(), DEDUP_HASH_STORED, &d, name), ESP_OK);
    }
    Groups groups;
    int images = 0, indexed = 0;
    ASSERT_EQ(dedup_index_groups(dir.c_str(), DEDUP_HASH_STORED, collect_group, &groups, &images,
                                 &indexed),
              ESP_OK);
    EXPECT_EQ(groups.size(), 100u);
    for (const auto &g : groups) {
        EXPECT_EQ(g.size(), 3u);
    }
    EXPECT_EQ(images, 300);
    EXPECT_EQ(indexed, 300);
}
