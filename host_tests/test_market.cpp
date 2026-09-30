#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "market_quotes.h"
}

namespace
{

std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(MARKET_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool valid_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *) s;
    while (*p) {
        int extra = *p < 0x80         ? 0
                    : (*p >> 5) == 6  ? 1
                    : (*p >> 4) == 14 ? 2
                    : (*p >> 3) == 30 ? 3
                                      : -1;
        if (extra < 0) {
            return false;
        }
        for (int i = 1; i <= extra; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                return false;
            }
        }
        p += 1 + extra;
    }
    return true;
}

// Dates ascending, values positive
void expect_well_formed(const market_series_t &s)
{
    ASSERT_GT(s.count, 0);
    ASSERT_LE(s.count, MARKET_MAX_POINTS);
    for (int i = 0; i < s.count; i++) {
        EXPECT_EQ(strlen(s.dates[i]), 10u) << i;
        EXPECT_GT(s.values[i], 0.0f) << i;
        if (i > 0) {
            EXPECT_LT(strcmp(s.dates[i - 1], s.dates[i]), 0) << i;
        }
    }
}

int symbols(const char *text, char (*out)[MARKET_SYMBOL_LEN], int max = MARKET_MAX_SYMBOLS)
{
    return market_parse_symbols(text, out, max);
}

}  // namespace

// ---- the symbol list ---------------------------------------------------------------------------

TEST(MarketSymbols, SeparatorsAndCase)
{
    char s[8][MARKET_SYMBOL_LEN];
    ASSERT_EQ(symbols("aapl, eunl.de; ^gdaxi\nGC=F", s, 8), 4);
    EXPECT_STREQ(s[0], "AAPL");
    EXPECT_STREQ(s[1], "EUNL.DE");
    EXPECT_STREQ(s[2], "^GDAXI");
    EXPECT_STREQ(s[3], "GC=F");
}

TEST(MarketSymbols, DoublesAreDropped)
{
    char s[8][MARKET_SYMBOL_LEN];
    ASSERT_EQ(symbols("AAPL aapl MSFT AAPL", s, 8), 2);
    EXPECT_STREQ(s[1], "MSFT");
}

TEST(MarketSymbols, LimitAndEmpty)
{
    char s[8][MARKET_SYMBOL_LEN];
    EXPECT_EQ(symbols("A,B,C,D,E,F", s, 4), 4);
    EXPECT_EQ(symbols("", s, 4), 0);
    EXPECT_EQ(symbols(" , ;; \n", s, 4), 0);
    EXPECT_EQ(market_parse_symbols(nullptr, s, 4), 0);
}

TEST(MarketSymbols, InvalidOnesAreLeftOut)
{
    char s[8][MARKET_SYMBOL_LEN];
    // a slash, a quote, a URL character and a symbol that is too long
    ASSERT_EQ(symbols("EUR/USD AA\"PL a&b=c MSFT ABCDEFGHIJKLMNOP ABCDEFGHIJKLMNO", s, 8), 2);
    EXPECT_STREQ(s[0], "MSFT");
    EXPECT_STREQ(s[1], "ABCDEFGHIJKLMNO");
}

TEST(MarketSymbols, UmlautsAreNotSymbols)
{
    char s[4][MARKET_SYMBOL_LEN];
    EXPECT_EQ(symbols("BÄR AAPL", s, 4), 1);
    EXPECT_STREQ(s[0], "AAPL");
}

// ---- keys and requests
// ---------------------------------------------------------------------------

TEST(MarketKey, Validation)
{
    EXPECT_TRUE(market_key_valid("abcdef1234567890"));
    EXPECT_TRUE(market_key_valid("12345678"));
    EXPECT_TRUE(market_key_valid("demo"));  // the public key of both services
    EXPECT_FALSE(market_key_valid("dem"));
    EXPECT_FALSE(market_key_valid(""));
    EXPECT_FALSE(market_key_valid(nullptr));
    EXPECT_FALSE(market_key_valid("abcdef12&x=1234"));
    EXPECT_FALSE(market_key_valid("abcdef12 34567"));
    EXPECT_FALSE(market_key_valid("abcdef12\n34567"));
    EXPECT_TRUE(market_key_valid(std::string(MARKET_KEY_MAX, 'a').c_str()));
    EXPECT_FALSE(market_key_valid(std::string(MARKET_KEY_MAX + 1, 'a').c_str()));
}

TEST(MarketUrls, Yahoo)
{
    char url[160];
    ASSERT_TRUE(market_yahoo_url("AAPL", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://query1.finance.yahoo.com/v8/finance/chart/AAPL?range=1mo&interval=1d");
    ASSERT_TRUE(market_yahoo_url("^GDAXI", url, sizeof(url)));
    EXPECT_NE(strstr(url, "/chart/%5EGDAXI?"), nullptr);
    ASSERT_TRUE(market_yahoo_url("GC=F", url, sizeof(url)));
    EXPECT_NE(strstr(url, "/chart/GC%3DF?"), nullptr);
    ASSERT_TRUE(market_yahoo_url("EUNL.DE", url, sizeof(url)));
    EXPECT_NE(strstr(url, "/chart/EUNL.DE?"), nullptr);
}

TEST(MarketUrls, YahooRejectsWhatIsNotASymbol)
{
    char url[160];
    EXPECT_FALSE(market_yahoo_url("", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("AA PL", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("AAPL?x=1", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("AAPL/../x", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("AAPL#", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("AAPL\r\nHost: x", url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url(nullptr, url, sizeof(url)));
    EXPECT_FALSE(market_yahoo_url("ABCDEFGHIJKLMNOP", url, sizeof(url)));
}

TEST(MarketUrls, YahooBufferTooSmall)
{
    char url[40];
    EXPECT_FALSE(market_yahoo_url("AAPL", url, sizeof(url)));
}

TEST(MarketMapping, TwelveData)
{
    char out[24];
    ASSERT_TRUE(market_twelvedata_symbol("AAPL", out, sizeof(out)));
    EXPECT_STREQ(out, "AAPL");
    ASSERT_TRUE(market_twelvedata_symbol("BRK-B", out, sizeof(out)));
    EXPECT_STREQ(out, "BRK.B");
    ASSERT_TRUE(market_twelvedata_symbol("EURUSD=X", out, sizeof(out)));
    EXPECT_STREQ(out, "EUR/USD");
    ASSERT_TRUE(market_twelvedata_symbol("BTC-EUR", out, sizeof(out)));
    EXPECT_STREQ(out, "BTC/EUR");
    ASSERT_TRUE(market_twelvedata_symbol("ETH-USD", out, sizeof(out)));
    EXPECT_STREQ(out, "ETH/USD");
}

TEST(MarketMapping, TwelveDataHasNoEquivalentFor)
{
    char out[24];
    EXPECT_FALSE(market_twelvedata_symbol("^GDAXI", out, sizeof(out)));   // index
    EXPECT_FALSE(market_twelvedata_symbol("GC=F", out, sizeof(out)));     // future
    EXPECT_FALSE(market_twelvedata_symbol("EUNL.DE", out, sizeof(out)));  // exchange suffix
    EXPECT_FALSE(market_twelvedata_symbol("EUR/USD", out, sizeof(out)));  // not in Yahoo notation
    EXPECT_FALSE(market_twelvedata_symbol("EUR1=X", out, sizeof(out)));
    EXPECT_FALSE(market_twelvedata_symbol("A-BCDEFG", out, sizeof(out)));
    EXPECT_FALSE(market_twelvedata_symbol("", out, sizeof(out)));
    EXPECT_FALSE(market_twelvedata_symbol("AAPL", out, 4));  // buffer too small
}

TEST(MarketMapping, AlphaVantage)
{
    char out[24];
    ASSERT_TRUE(market_alphavantage_symbol("IBM", out, sizeof(out)));
    EXPECT_STREQ(out, "IBM");
    ASSERT_TRUE(market_alphavantage_symbol("EUNL.DE", out, sizeof(out)));
    EXPECT_STREQ(out, "EUNL.DEX");
    ASSERT_TRUE(market_alphavantage_symbol("VOD.L", out, sizeof(out)));
    EXPECT_STREQ(out, "VOD.LON");
    ASSERT_TRUE(market_alphavantage_symbol("SHOP.TO", out, sizeof(out)));
    EXPECT_STREQ(out, "SHOP.TRT");
    ASSERT_TRUE(market_alphavantage_symbol("ABC.V", out, sizeof(out)));
    EXPECT_STREQ(out, "ABC.TRV");
    ASSERT_TRUE(market_alphavantage_symbol("RELIANCE.BO", out, sizeof(out)));
    EXPECT_STREQ(out, "RELIANCE.BSE");
    ASSERT_TRUE(market_alphavantage_symbol("600104.SS", out, sizeof(out)));
    EXPECT_STREQ(out, "600104.SHH");
    ASSERT_TRUE(market_alphavantage_symbol("000002.SZ", out, sizeof(out)));
    EXPECT_STREQ(out, "000002.SHZ");
    ASSERT_TRUE(market_alphavantage_symbol("BRK-B", out, sizeof(out)));
    EXPECT_STREQ(out, "BRK-B");  // Alpha Vantage writes the share class with a dash as well
}

TEST(MarketMapping, AlphaVantageHasNoEquivalentFor)
{
    char out[24];
    EXPECT_FALSE(market_alphavantage_symbol("^GDAXI", out, sizeof(out)));
    EXPECT_FALSE(market_alphavantage_symbol("GC=F", out, sizeof(out)));
    EXPECT_FALSE(market_alphavantage_symbol("EURUSD=X", out, sizeof(out)));
    EXPECT_FALSE(market_alphavantage_symbol("BTC-EUR", out, sizeof(out)));
    EXPECT_FALSE(market_alphavantage_symbol("ABC.XY", out, sizeof(out)));  // unknown suffix
    EXPECT_FALSE(market_alphavantage_symbol("", out, sizeof(out)));
    EXPECT_FALSE(market_alphavantage_symbol("EUNL.DE", out, 5));
}

TEST(MarketUrls, TwelveDataAndAlphaVantage)
{
    char url[200];
    ASSERT_TRUE(market_twelvedata_url("EUR/USD", "abcdef1234", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://api.twelvedata.com/time_series?symbol=EUR%2FUSD&interval=1day&outputsize="
                 "30&apikey=abcdef1234");
    ASSERT_TRUE(market_alphavantage_url("EUNL.DEX", "abcdef1234", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=EUNL.DEX&"
                 "apikey=abcdef1234");
    // "compact" is the default: no outputsize, so the answer is the last 100 days, not 20 years
    EXPECT_EQ(strstr(url, "outputsize"), nullptr);
}

TEST(MarketUrls, KeysAreValidatedBeforeTheyGoIntoARequest)
{
    char url[200];
    EXPECT_FALSE(market_twelvedata_url("AAPL", "abc", url, sizeof(url)));
    EXPECT_FALSE(market_twelvedata_url("AAPL", "abcdef12&outputsize=5000", url, sizeof(url)));
    EXPECT_FALSE(market_twelvedata_url("AAPL", nullptr, url, sizeof(url)));
    EXPECT_FALSE(market_alphavantage_url("IBM", "abcdef12&function=x", url, sizeof(url)));
    EXPECT_FALSE(market_alphavantage_url("I&BM", "abcdef1234", url, sizeof(url)));
    EXPECT_FALSE(market_alphavantage_url("IBM", "abcdef1234", url, 30));
}

// ---- Yahoo -------------------------------------------------------------------------------------

TEST(MarketYahoo, UsStock)
{
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-aapl.json").c_str(), &s), MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.symbol, "AAPL");
    EXPECT_STREQ(s.name, "Apple Inc.");
    EXPECT_STREQ(s.currency, "USD");
    EXPECT_EQ(s.provider, MARKET_YAHOO);
    EXPECT_EQ(s.count, 21);
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-29");
    EXPECT_NEAR(s.values[s.count - 1], 329.4f, 0.01f);
    EXPECT_NEAR(s.values[s.count - 2], 338.4f, 0.01f);
    EXPECT_FALSE(s.stale);
}

TEST(MarketYahoo, TheDateFollowsTheExchangeTimezone)
{
    // The German ETF's last timestamp is 15:00 UTC on the day; the Asian and the US ones would
    // move a day across midnight if the offset were left out.
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-eunl-de.json").c_str(), &s), MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.symbol, "EUNL.DE");
    EXPECT_STREQ(s.currency, "EUR");
    EXPECT_TRUE(valid_utf8(s.name));
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-30");
}

TEST(MarketYahoo, NullClosesAreSkipped)
{
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-eunl-de.json").c_str(), &s), MARKET_PARSE_OK);
    EXPECT_EQ(s.count, 22);  // 23 timestamps, one without a price
}

TEST(MarketYahoo, IndexFutureCrypto)
{
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-gdaxi.json").c_str(), &s), MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.symbol, "^GDAXI");
    EXPECT_NEAR(s.values[s.count - 1], 25336.02f, 0.5f);

    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-gold-future.json").c_str(), &s), MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.symbol, "GC=F");
    EXPECT_STREQ(s.name, "Gold Dec 26");  // no longName: the short name is taken

    ASSERT_EQ(market_parse_yahoo(fixture("yahoo-btc-eur.json").c_str(), &s), MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.currency, "EUR");
    EXPECT_EQ(s.count, 30);  // 32 days: only the newest 30 are kept
    EXPECT_NEAR(s.values[s.count - 1], 75066.26f, 0.5f);
}

TEST(MarketYahoo, NotFound)
{
    market_series_t s;
    EXPECT_EQ(market_parse_yahoo(fixture("yahoo-not-found.json").c_str(), &s),
              MARKET_PARSE_NOT_FOUND);
    EXPECT_EQ(s.count, 0);
}

TEST(MarketYahoo, GarbageAndShapes)
{
    market_series_t s;
    EXPECT_EQ(market_parse_yahoo(nullptr, &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("not json", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("[]", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("{}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("{\"chart\":{\"result\":[]}}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_yahoo("{\"chart\":{\"result\":[{\"meta\":{}}]}}", &s), MARKET_PARSE_BAD);
    // Too Many Requests answers are plain text
    EXPECT_EQ(market_parse_yahoo("Too Many Requests", &s), MARKET_PARSE_BAD);
    // a symbol without any closing price
    EXPECT_EQ(market_parse_yahoo("{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"X\"},"
                                 "\"timestamp\":[1,2],\"indicators\":{\"quote\":[{\"close\":"
                                 "[null,null]}]}}]}}",
                                 &s),
              MARKET_PARSE_BAD);
}

TEST(MarketYahoo, CutOffAnswerIsBad)
{
    std::string json = fixture("yahoo-aapl.json");
    market_series_t s;
    EXPECT_EQ(market_parse_yahoo(json.substr(0, json.size() / 2).c_str(), &s), MARKET_PARSE_BAD);
}

TEST(MarketYahoo, NameIsCutAtACharacterBoundary)
{
    // 40 two-byte characters as the name: the copy must not end inside one
    std::string name;
    for (int i = 0; i < 40; i++) {
        name += "\xC3\xA4";
    }
    std::string json =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"X\",\"currency\":\"EUR\",\"longName\":\"" +
        name +
        "\",\"gmtoffset\":0},\"timestamp\":[86400,172800],\"indicators\":{\"quote\":[{\"close\":"
        "[1.5,2.5]}]}}]}}";
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(json.c_str(), &s), MARKET_PARSE_OK);
    EXPECT_TRUE(valid_utf8(s.name));
    EXPECT_LT(strlen(s.name), sizeof(s.name));
    EXPECT_STREQ(s.dates[0], "1970-01-02");
    EXPECT_STREQ(s.dates[1], "1970-01-03");
}

TEST(MarketYahoo, DatesOfEveryKind)
{
    // 2000-02-29 12:00 UTC = 951825600, 2024-12-31 23:00 UTC = 1735686000; offset moves the second
    // to the next year
    std::string json =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"X\",\"currency\":\"USD\",\"gmtoffset\":"
        "7200},\"timestamp\":[951825600,1735686000],\"indicators\":{\"quote\":[{\"close\":"
        "[1.0,2.0]}]}}]}}";
    market_series_t s;
    ASSERT_EQ(market_parse_yahoo(json.c_str(), &s), MARKET_PARSE_OK);
    EXPECT_STREQ(s.dates[0], "2000-02-29");
    EXPECT_STREQ(s.dates[1], "2025-01-01");
}

// ---- Twelve Data
// -----------------------------------------------------------------------------------

TEST(MarketTwelveData, UsStock)
{
    market_series_t s;
    ASSERT_EQ(market_parse_twelvedata(fixture("twelvedata-aapl.json").c_str(), &s),
              MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_EQ(s.provider, MARKET_TWELVEDATA);
    EXPECT_STREQ(s.symbol, "AAPL");
    EXPECT_STREQ(s.currency, "USD");
    EXPECT_EQ(s.count, 30);
    EXPECT_STREQ(s.dates[0], "2026-08-18");  // oldest first
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-29");
    EXPECT_NEAR(s.values[0], 310.03f, 0.01f);
    EXPECT_NEAR(s.values[s.count - 1], 329.4f, 0.01f);
}

TEST(MarketTwelveData, CryptoTakesTheCurrencyFromThePair)
{
    market_series_t s;
    ASSERT_EQ(market_parse_twelvedata(fixture("twelvedata-btc-usd.json").c_str(), &s),
              MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_STREQ(s.symbol, "BTC/USD");
    EXPECT_STREQ(s.currency, "USD");  // the answer only has "US Dollar"
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-30");
    EXPECT_NEAR(s.values[s.count - 1], 85300.02f, 0.5f);
}

TEST(MarketTwelveData, Errors)
{
    market_series_t s;
    EXPECT_EQ(market_parse_twelvedata(fixture("twelvedata-bad-key.json").c_str(), &s),
              MARKET_PARSE_KEY_REFUSED);
    // Synthetic answers in the documented shape of the other error codes
    EXPECT_EQ(market_parse_twelvedata(
                  "{\"code\":429,\"message\":\"You have run out of API credits\",\"status\":"
                  "\"error\"}",
                  &s),
              MARKET_PARSE_RATE_LIMITED);
    EXPECT_EQ(market_parse_twelvedata(
                  "{\"code\":404,\"message\":\"symbol not found\",\"status\":\"error\"}", &s),
              MARKET_PARSE_NOT_FOUND);
    EXPECT_EQ(market_parse_twelvedata(
                  "{\"code\":400,\"message\":\"symbol is missing\",\"status\":\"error\"}", &s),
              MARKET_PARSE_NOT_FOUND);
    EXPECT_EQ(
        market_parse_twelvedata("{\"code\":403,\"message\":\"plan\",\"status\":\"error\"}", &s),
        MARKET_PARSE_KEY_REFUSED);
    EXPECT_EQ(
        market_parse_twelvedata("{\"code\":500,\"message\":\"oops\",\"status\":\"error\"}", &s),
        MARKET_PARSE_BAD);
}

TEST(MarketTwelveData, GarbageAndShapes)
{
    market_series_t s;
    EXPECT_EQ(market_parse_twelvedata(nullptr, &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_twelvedata("", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_twelvedata("{}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_twelvedata("{\"values\":[]}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_twelvedata("{\"values\":[{\"datetime\":\"2026-01-01\"}]}", &s),
              MARKET_PARSE_BAD);
    // a number instead of a string, a bad number, a short date: all points are skipped
    EXPECT_EQ(market_parse_twelvedata("{\"values\":[{\"datetime\":\"2026-01-01\",\"close\":5},"
                                      "{\"datetime\":\"2026-01-02\",\"close\":\"abc\"},"
                                      "{\"datetime\":\"2026\",\"close\":\"1.0\"},"
                                      "{\"datetime\":\"2026-01-04\",\"close\":\"-3\"}]}",
                                      &s),
              MARKET_PARSE_BAD);
}

TEST(MarketTwelveData, OneGoodPointIsEnough)
{
    market_series_t s;
    ASSERT_EQ(market_parse_twelvedata(
                  "{\"meta\":{\"symbol\":\"X\",\"currency\":\"USD\"},\"values\":[{\"datetime\":"
                  "\"2026-01-02 15:00:00\",\"close\":\"12.5\"}]}",
                  &s),
              MARKET_PARSE_OK);
    EXPECT_EQ(s.count, 1);
    EXPECT_STREQ(s.dates[0], "2026-01-02");
}

// ---- Alpha Vantage
// ---------------------------------------------------------------------------------

TEST(MarketAlphaVantage, Daily)
{
    market_series_t s;
    ASSERT_EQ(market_parse_alphavantage(fixture("alphavantage-ibm-daily.json").c_str(), &s),
              MARKET_PARSE_OK);
    expect_well_formed(s);
    EXPECT_EQ(s.provider, MARKET_ALPHAVANTAGE);
    EXPECT_STREQ(s.symbol, "IBM");
    EXPECT_EQ(s.count, 30);  // 100 days in the answer, the newest 30 are kept
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-29");
    EXPECT_STREQ(s.dates[0], "2026-08-18");
    EXPECT_NEAR(s.values[s.count - 1], 219.99f, 0.01f);
    EXPECT_NEAR(s.values[s.count - 2], 220.67f, 0.01f);
    EXPECT_STREQ(s.currency, "");  // this answer carries none
}

TEST(MarketAlphaVantage, CutOffAnswerStillGivesItsBeginning)
{
    // the answer stops in the middle of the tenth day: nine days, the newest ones
    std::string json = fixture("alphavantage-ibm-daily.json");
    size_t at = 0;
    for (int i = 0; i < 10; i++) {
        at = json.find("\"1. open\"", at + 1);
    }
    market_series_t s;
    ASSERT_EQ(market_parse_alphavantage(json.substr(0, at + 20).c_str(), &s), MARKET_PARSE_OK);
    EXPECT_EQ(s.count, 9);
    EXPECT_STREQ(s.dates[s.count - 1], "2026-09-29");
    expect_well_formed(s);
}

TEST(MarketAlphaVantage, Messages)
{
    market_series_t s;
    // the real answer to the demo key
    EXPECT_EQ(market_parse_alphavantage(fixture("alphavantage-information.json").c_str(), &s),
              MARKET_PARSE_KEY_REFUSED);
    // Synthetic answers in the documented shapes of the other messages
    EXPECT_EQ(market_parse_alphavantage(
                  "{\"Note\":\"Thank you for using Alpha Vantage! Our standard API call "
                  "frequency is 5 calls per minute.\"}",
                  &s),
              MARKET_PARSE_RATE_LIMITED);
    EXPECT_EQ(market_parse_alphavantage(
                  "{\"Information\":\"Thank you for using Alpha Vantage! Our standard API rate "
                  "limit is 25 requests per day.\"}",
                  &s),
              MARKET_PARSE_RATE_LIMITED);
    EXPECT_EQ(market_parse_alphavantage(
                  "{\"Error Message\":\"Invalid API call. Please retry or visit the "
                  "documentation (https://www.alphavantage.co/documentation/) for TIME_SERIES_"
                  "DAILY.\"}",
                  &s),
              MARKET_PARSE_NOT_FOUND);
    EXPECT_EQ(market_parse_alphavantage(
                  "{\"Error Message\":\"the parameter apikey is invalid or missing.\"}", &s),
              MARKET_PARSE_KEY_REFUSED);
}

TEST(MarketAlphaVantage, GarbageAndShapes)
{
    market_series_t s;
    EXPECT_EQ(market_parse_alphavantage(nullptr, &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("{}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("<html>bad gateway</html>", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("{\"Time Series (Daily)\":", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("{\"Time Series (Daily)\":{}}", &s), MARKET_PARSE_BAD);
    EXPECT_EQ(market_parse_alphavantage("{\"Time Series (Daily)\":[1,2]}", &s), MARKET_PARSE_BAD);
    // a day without the close member
    EXPECT_EQ(market_parse_alphavantage(
                  "{\"Time Series (Daily)\":{\"2026-01-02\":{\"1. open\":\"5\"}}}", &s),
              MARKET_PARSE_BAD);
}

TEST(MarketAlphaVantage, BracesInsideStringsDoNotConfuseTheScan)
{
    market_series_t s;
    ASSERT_EQ(
        market_parse_alphavantage("{\"Meta Data\":{\"2. Symbol\":\"X{\"},\"Time Series (Daily)\":{"
                                  "\"2026-01-03\":{\"note\":\"}{\",\"4. close\":\"7.5\"},"
                                  "\"2026-01-02\":{\"4. close\":\"7.0\"}}}",
                                  &s),
        MARKET_PARSE_OK);
    EXPECT_EQ(s.count, 2);
    EXPECT_STREQ(s.dates[0], "2026-01-02");
    EXPECT_NEAR(s.values[1], 7.5f, 0.001f);
}

// ---- figures -----------------------------------------------------------------------------------

TEST(MarketFigures, ChangePercent)
{
    market_series_t s;
    memset(&s, 0, sizeof(s));
    EXPECT_FLOAT_EQ(market_change_percent(&s), 0.0f);
    EXPECT_FLOAT_EQ(market_change_percent(nullptr), 0.0f);
    s.count = 1;
    s.values[0] = 10.0f;
    EXPECT_FLOAT_EQ(market_change_percent(&s), 0.0f);
    s.count = 2;
    s.values[0] = 200.0f;
    s.values[1] = 190.0f;
    EXPECT_NEAR(market_change_percent(&s), -5.0f, 0.001f);
    s.values[1] = 210.0f;
    EXPECT_NEAR(market_change_percent(&s), 5.0f, 0.001f);
    s.values[0] = 0.0f;  // cannot happen with parsed data, must not divide by zero
    EXPECT_FLOAT_EQ(market_change_percent(&s), 0.0f);
}

TEST(MarketFigures, Price)
{
    char out[16];
    market_format_price("BTC-EUR", 85300.02f, out, sizeof(out));
    EXPECT_STREQ(out, "85300");
    market_format_price("^GDAXI", 10000.0f, out, sizeof(out));
    EXPECT_STREQ(out, "10000");
    market_format_price("AAPL", 329.4f, out, sizeof(out));
    EXPECT_STREQ(out, "329.40");
    market_format_price("AAPL", 1.0f, out, sizeof(out));
    EXPECT_STREQ(out, "1.00");
    market_format_price("DOGE-EUR", 0.9234f, out, sizeof(out));
    EXPECT_STREQ(out, "0.9234");
    market_format_price("SHIB-EUR", 0.00001234f, out, sizeof(out));
    EXPECT_STREQ(out, "0.000012");
    char tiny[4];
    market_format_price("AAPL", 329.4f, tiny, sizeof(tiny));  // cut, but terminated
    EXPECT_EQ(strlen(tiny), 3u);
}

TEST(MarketFigures, CurrencyPairsGetFourDecimals)
{
    char out[16];
    market_format_price("EURUSD=X", 1.1355f, out, sizeof(out));
    EXPECT_STREQ(out, "1.1355");
    market_format_price("EURGBP=X", 0.8674f, out, sizeof(out));
    EXPECT_STREQ(out, "0.8674");
    market_format_price("USDJPY=X", 162.34f, out, sizeof(out));  // the usual two
    EXPECT_STREQ(out, "162.34");
    market_format_price(nullptr, 1.1355f, out, sizeof(out));
    EXPECT_STREQ(out, "1.14");
}

TEST(MarketFigures, ProviderNames)
{
    EXPECT_STREQ(market_provider_name(MARKET_YAHOO), "Yahoo Finance");
    EXPECT_STREQ(market_provider_name(MARKET_TWELVEDATA), "Twelve Data");
    EXPECT_STREQ(market_provider_name(MARKET_ALPHAVANTAGE), "Alpha Vantage");
}

// ---- the quota ---------------------------------------------------------------------------------

TEST(MarketQuota, Limits)
{
    EXPECT_EQ(market_quota_limit(MARKET_YAHOO), 0);
    EXPECT_EQ(market_quota_limit(MARKET_TWELVEDATA), 800);
    EXPECT_EQ(market_quota_limit(MARKET_ALPHAVANTAGE), 25);
}

TEST(MarketQuota, RoundTrip)
{
    market_quota_t q;
    market_quota_parse("20361,12,34,5", &q);
    EXPECT_EQ(q.day, 20361);
    EXPECT_EQ(q.used[MARKET_YAHOO], 12);
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 34);
    EXPECT_EQ(q.used[MARKET_ALPHAVANTAGE], 5);
    char text[40];
    market_quota_format(&q, text, sizeof(text));
    EXPECT_STREQ(text, "20361,12,34,5");
}

TEST(MarketQuota, DamagedTextGivesAnEmptyQuota)
{
    const char *bad[] = {"", "abc", "1,2,3", "0,1,2,3", "-5,1,2,3", nullptr, "20361;1;2;3"};
    for (const char *text : bad) {
        market_quota_t q;
        q.day = 99;
        q.used[1] = 99;
        market_quota_parse(text, &q);
        EXPECT_EQ(q.day, 0) << (text ? text : "null");
        EXPECT_EQ(q.used[1], 0);
    }
    market_quota_t q;
    market_quota_parse("20361,-4,5,6", &q);
    EXPECT_EQ(q.used[MARKET_YAHOO], 0);  // a negative count is not believed
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 5);
}

TEST(MarketQuota, NewDayStartsFromZero)
{
    market_quota_t q;
    market_quota_parse("20361,12,34,25", &q);
    market_quota_roll(&q, 20361);
    EXPECT_EQ(q.used[MARKET_ALPHAVANTAGE], 25);  // same day: kept
    market_quota_roll(&q, 20362);
    EXPECT_EQ(q.day, 20362);
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 0);
    EXPECT_EQ(q.used[MARKET_ALPHAVANTAGE], 0);
}

TEST(MarketQuota, Allows)
{
    market_quota_t q;
    memset(&q, 0, sizeof(q));
    q.used[MARKET_YAHOO] = 100000;
    q.used[MARKET_TWELVEDATA] = 799;
    q.used[MARKET_ALPHAVANTAGE] = 24;
    EXPECT_TRUE(market_quota_allows(&q, MARKET_YAHOO));  // no limit
    EXPECT_TRUE(market_quota_allows(&q, MARKET_TWELVEDATA));
    EXPECT_TRUE(market_quota_allows(&q, MARKET_ALPHAVANTAGE));
    q.used[MARKET_TWELVEDATA] = 800;
    q.used[MARKET_ALPHAVANTAGE] = 25;
    EXPECT_FALSE(market_quota_allows(&q, MARKET_TWELVEDATA));
    EXPECT_FALSE(market_quota_allows(&q, MARKET_ALPHAVANTAGE));
}

// ---- which source for which symbol
// ---------------------------------------------------------------

namespace
{

std::string plan(const market_options_t &options, const char *symbol, const market_quota_t &quota)
{
    market_provider_t order[MARKET_PROVIDER_COUNT];
    int n = market_plan(&options, symbol, &quota, order);
    std::string text;
    for (int i = 0; i < n; i++) {
        text += order[i] == MARKET_YAHOO ? "Y" : order[i] == MARKET_TWELVEDATA ? "T" : "A";
    }
    return text;
}

}  // namespace

TEST(MarketPlan, Order)
{
    market_options_t all = {true, true, true};
    market_quota_t q;
    memset(&q, 0, sizeof(q));
    EXPECT_EQ(plan(all, "AAPL", q), "YTA");
    EXPECT_EQ(plan(all, "EUNL.DE", q), "YA");  // Twelve Data does not do the suffix
    EXPECT_EQ(plan(all, "BTC-EUR", q), "YT");  // Alpha Vantage does not do crypto
    EXPECT_EQ(plan(all, "EURUSD=X", q), "YT");
    EXPECT_EQ(plan(all, "^GDAXI", q), "Y");
    EXPECT_EQ(plan(all, "GC=F", q), "Y");
}

TEST(MarketPlan, SwitchesAndKeys)
{
    market_quota_t q;
    memset(&q, 0, sizeof(q));
    EXPECT_EQ(plan({false, true, true}, "AAPL", q), "TA");
    EXPECT_EQ(plan({true, false, false}, "AAPL", q), "Y");
    EXPECT_EQ(plan({false, false, true}, "AAPL", q), "A");
    EXPECT_EQ(plan({false, false, false}, "AAPL", q), "");
    EXPECT_EQ(plan({false, true, true}, "^GDAXI", q), "");  // nobody can serve it
}

TEST(MarketPlan, UsedUpQuotaLeavesASourceOut)
{
    market_options_t all = {true, true, true};
    market_quota_t q;
    memset(&q, 0, sizeof(q));
    q.used[MARKET_ALPHAVANTAGE] = 25;
    EXPECT_EQ(plan(all, "AAPL", q), "YT");
    q.used[MARKET_TWELVEDATA] = 800;
    EXPECT_EQ(plan(all, "AAPL", q), "Y");
}

TEST(MarketPlan, InvalidSymbolsGiveNoPlan)
{
    market_options_t all = {true, true, true};
    market_quota_t q;
    memset(&q, 0, sizeof(q));
    EXPECT_EQ(plan(all, "", q), "");
    EXPECT_EQ(plan(all, "A B", q), "");
    EXPECT_EQ(plan(all, "EUR/USD", q), "");
}

// ---- the cache ---------------------------------------------------------------------------------

namespace
{

market_series_t sample(const char *symbol, int points, market_provider_t provider)
{
    market_series_t s;
    memset(&s, 0, sizeof(s));
    snprintf(s.symbol, sizeof(s.symbol), "%s", symbol);
    snprintf(s.name, sizeof(s.name), "Name of %s", symbol);
    snprintf(s.currency, sizeof(s.currency), "EUR");
    s.provider = provider;
    s.fetched = 1790700000;
    s.count = points;
    for (int i = 0; i < points; i++) {
        snprintf(s.dates[i], sizeof(s.dates[i]), "2026-09-%02d", i + 1);
        s.values[i] = 100.25f + (float) i * 1.5f;
    }
    return s;
}

}  // namespace

TEST(MarketCache, RoundTrip)
{
    market_series_t in[3] = {sample("AAPL", 30, MARKET_YAHOO),
                             sample("EUNL.DE", 7, MARKET_ALPHAVANTAGE),
                             sample("^GDAXI", 1, MARKET_TWELVEDATA)};
    snprintf(in[2].name, sizeof(in[2].name), "K\xC3\xB6nig & Co | \"quoted\"\n2nd line");
    static char text[8192];
    size_t len = market_cache_encode(in, 3, text, sizeof(text));
    ASSERT_GT(len, 0u);
    EXPECT_EQ(len, strlen(text));

    market_series_t out[4];
    ASSERT_EQ(market_cache_decode(text, out, 4), 3);
    for (int i = 0; i < 3; i++) {
        EXPECT_STREQ(out[i].symbol, in[i].symbol);
        EXPECT_STREQ(out[i].currency, in[i].currency);
        EXPECT_EQ(out[i].provider, in[i].provider);
        EXPECT_EQ(out[i].count, in[i].count);
        EXPECT_EQ(out[i].fetched, in[i].fetched);
        for (int p = 0; p < in[i].count; p++) {
            EXPECT_STREQ(out[i].dates[p], in[i].dates[p]);
            EXPECT_FLOAT_EQ(out[i].values[p], in[i].values[p]);
        }
    }
    EXPECT_STREQ(out[0].name, "Name of AAPL");
    // the separator and the line break in a name are replaced, the umlaut stays
    EXPECT_TRUE(valid_utf8(out[2].name));
    EXPECT_NE(strstr(out[2].name, "K\xC3\xB6nig"), nullptr);
    EXPECT_EQ(strchr(out[2].name, '|'), nullptr);
    EXPECT_EQ(strchr(out[2].name, '\n'), nullptr);
}

TEST(MarketCache, PricesKeepTheirPrecision)
{
    market_series_t in[1] = {sample("X", 2, MARKET_YAHOO)};
    in[0].values[0] = 0.00012345f;
    in[0].values[1] = 85300.02f;
    char text[1024];
    ASSERT_GT(market_cache_encode(in, 1, text, sizeof(text)), 0u);
    market_series_t out[1];
    ASSERT_EQ(market_cache_decode(text, out, 1), 1);
    EXPECT_FLOAT_EQ(out[0].values[0], in[0].values[0]);
    EXPECT_FLOAT_EQ(out[0].values[1], in[0].values[1]);
}

TEST(MarketCache, EncodeReportsAnOverflow)
{
    market_series_t in[1] = {sample("AAPL", 30, MARKET_YAHOO)};
    char small[200];
    EXPECT_EQ(market_cache_encode(in, 1, small, sizeof(small)), 0u);
    char tiny[8];
    EXPECT_EQ(market_cache_encode(in, 1, tiny, sizeof(tiny)), 0u);
}

TEST(MarketCache, EmptyCacheHasNoSeries)
{
    char text[64];
    ASSERT_GT(market_cache_encode(nullptr, 0, text, sizeof(text)), 0u);
    market_series_t out[2];
    EXPECT_EQ(market_cache_decode(text, out, 2), 0);
}

TEST(MarketCache, RespectsTheCapacityOfTheCaller)
{
    market_series_t in[3] = {sample("A", 3, MARKET_YAHOO), sample("B", 3, MARKET_YAHOO),
                             sample("C", 3, MARKET_YAHOO)};
    char text[2048];
    ASSERT_GT(market_cache_encode(in, 3, text, sizeof(text)), 0u);
    market_series_t out[2];
    EXPECT_EQ(market_cache_decode(text, out, 2), 2);
    EXPECT_STREQ(out[1].symbol, "B");
}

TEST(MarketCache, DamagedTextGivesWhatCameBeforeTheDamage)
{
    market_series_t in[2] = {sample("AAPL", 5, MARKET_YAHOO), sample("MSFT", 5, MARKET_YAHOO)};
    char text[2048];
    ASSERT_GT(market_cache_encode(in, 2, text, sizeof(text)), 0u);
    market_series_t out[4];

    // cut in the middle of the second series
    std::string cut(text);
    cut.resize(cut.rfind("MSFT") + 60);
    EXPECT_EQ(market_cache_decode(cut.c_str(), out, 4), 1);
    EXPECT_STREQ(out[0].symbol, "AAPL");

    // a point line that is not a number
    std::string bad(text);
    bad.replace(bad.find("2026-09-03 ") + 11, 3, "zzz");
    EXPECT_EQ(market_cache_decode(bad.c_str(), out, 4), 0);

    // the first series is intact, the second header is broken
    std::string header(text);
    header.replace(header.find("S|MSFT"), 6, "S|");
    EXPECT_EQ(market_cache_decode(header.c_str(), out, 4), 1);
}

TEST(MarketCache, WrongOrMissingHeaderGivesNothing)
{
    market_series_t out[2];
    EXPECT_EQ(market_cache_decode(nullptr, out, 2), 0);
    EXPECT_EQ(market_cache_decode("", out, 2), 0);
    EXPECT_EQ(market_cache_decode("PF-MARKETS 2\nS|A|n|EUR|0|0|1\n2026-01-01 1\n", out, 2), 0);
    EXPECT_EQ(market_cache_decode("hello world", out, 2), 0);
    // implausible counts and sources
    EXPECT_EQ(market_cache_decode("PF-MARKETS 1\nS|A|n|EUR|0|0|31\n", out, 2), 0);
    EXPECT_EQ(market_cache_decode("PF-MARKETS 1\nS|A|n|EUR|9|0|1\n2026-01-01 1\n", out, 2), 0);
    EXPECT_EQ(market_cache_decode("PF-MARKETS 1\nS|A|n|EUR|0|0|0\n", out, 2), 0);
    EXPECT_EQ(market_cache_decode("PF-MARKETS 1\nS||n|EUR|0|0|1\n2026-01-01 1\n", out, 2), 0);
}

TEST(MarketCache, WindowsLineEndingsAreNotAccepted)
{
    // we only read what we wrote: a foreign file is dropped, not half-understood
    market_series_t out[2];
    EXPECT_EQ(market_cache_decode("PF-MARKETS 1\r\nS|A|n|EUR|0|0|1\r\n2026-01-01 1\r\n", out, 2),
              0);
}

// ---- the answer of a source, by HTTP status
// ---------------------------------------------------------

TEST(MarketAnswer, TwoHundredIsReadFromTheBody)
{
    market_series_t s;
    EXPECT_EQ(market_parse_answer(MARKET_YAHOO, 200, fixture("yahoo-aapl.json").c_str(), &s),
              MARKET_PARSE_OK);
    EXPECT_EQ(s.provider, MARKET_YAHOO);
    EXPECT_EQ(
        market_parse_answer(MARKET_TWELVEDATA, 200, fixture("twelvedata-aapl.json").c_str(), &s),
        MARKET_PARSE_OK);
    EXPECT_EQ(s.provider, MARKET_TWELVEDATA);
    EXPECT_EQ(market_parse_answer(MARKET_ALPHAVANTAGE, 200,
                                  fixture("alphavantage-ibm-daily.json").c_str(), &s),
              MARKET_PARSE_OK);
    EXPECT_EQ(s.provider, MARKET_ALPHAVANTAGE);
    // the services also answer some errors with 200 and a message
    EXPECT_EQ(market_parse_answer(MARKET_ALPHAVANTAGE, 200,
                                  fixture("alphavantage-information.json").c_str(), &s),
              MARKET_PARSE_KEY_REFUSED);
    EXPECT_EQ(market_parse_answer(MARKET_YAHOO, 200, fixture("yahoo-not-found.json").c_str(), &s),
              MARKET_PARSE_NOT_FOUND);
    EXPECT_EQ(s.count, 0);
}

TEST(MarketAnswer, TwoHundredWithoutABodyIsBad)
{
    market_series_t s;
    for (market_provider_t p : {MARKET_YAHOO, MARKET_TWELVEDATA, MARKET_ALPHAVANTAGE}) {
        EXPECT_EQ(market_parse_answer(p, 200, nullptr, &s), MARKET_PARSE_BAD);
        EXPECT_EQ(market_parse_answer(p, 200, "", &s), MARKET_PARSE_BAD);
        EXPECT_EQ(market_parse_answer(p, 200, "<html>", &s), MARKET_PARSE_BAD);
    }
}

TEST(MarketAnswer, TheStatusDecidesWhereTheSourceSaysSo)
{
    market_series_t s;
    for (market_provider_t p : {MARKET_YAHOO, MARKET_TWELVEDATA, MARKET_ALPHAVANTAGE}) {
        EXPECT_EQ(market_parse_answer(p, 401, nullptr, &s), MARKET_PARSE_KEY_REFUSED);
        EXPECT_EQ(market_parse_answer(p, 403, "", &s), MARKET_PARSE_KEY_REFUSED);
        EXPECT_EQ(market_parse_answer(p, 429, "Too Many Requests", &s), MARKET_PARSE_RATE_LIMITED);
        EXPECT_EQ(market_parse_answer(p, 404, nullptr, &s), MARKET_PARSE_NOT_FOUND);
        EXPECT_EQ(market_parse_answer(p, 500, nullptr, &s), MARKET_PARSE_BAD);
        EXPECT_EQ(market_parse_answer(p, 503, "unavailable", &s), MARKET_PARSE_BAD);
        EXPECT_EQ(market_parse_answer(p, 0, nullptr, &s), MARKET_PARSE_BAD);
        EXPECT_EQ(market_parse_answer(p, 302, nullptr, &s), MARKET_PARSE_BAD);
    }
}

TEST(MarketAnswer, AnErrorAnswerNeverBringsPoints)
{
    // a 429 that carries a perfectly good body is still a refusal, and leaves no points behind
    market_series_t s;
    EXPECT_EQ(market_parse_answer(MARKET_YAHOO, 429, fixture("yahoo-aapl.json").c_str(), &s),
              MARKET_PARSE_RATE_LIMITED);
    EXPECT_EQ(s.count, 0);
}

TEST(MarketAnswer, AnErrorStatusWithTheServicesOwnMessage)
{
    market_series_t s;
    // Twelve Data answers a bad key with HTTP 401 and the message; a status this code does not
    // know (say 400) still gets its meaning from the message
    EXPECT_EQ(
        market_parse_answer(MARKET_TWELVEDATA, 401, fixture("twelvedata-bad-key.json").c_str(), &s),
        MARKET_PARSE_KEY_REFUSED);
    EXPECT_EQ(
        market_parse_answer(MARKET_TWELVEDATA, 400, fixture("twelvedata-bad-key.json").c_str(), &s),
        MARKET_PARSE_KEY_REFUSED);
    EXPECT_EQ(market_parse_answer(MARKET_YAHOO, 400, fixture("yahoo-not-found.json").c_str(), &s),
              MARKET_PARSE_NOT_FOUND);
}

// ---- absurd numbers (found by a mutation fuzz under ASan/UBSan)
// -----------------------------------

TEST(MarketYahoo, AbsurdTimesAndPricesAreSkippedNotConverted)
{
    market_series_t s;
    // a time of 1e19 seconds, one of -5, a NaN-like huge price and a good point at the end
    std::string json =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"X\",\"currency\":\"USD\",\"gmtoffset\":"
        "1e30},\"timestamp\":[10000000000000000000,-5,86400,172800],\"indicators\":{\"quote\":["
        "{\"close\":[1.5,2.5,1e300,3.5]}]}}]}}";
    ASSERT_EQ(market_parse_yahoo(json.c_str(), &s), MARKET_PARSE_OK);
    ASSERT_EQ(s.count, 1);                   // only the last point is sane
    EXPECT_STREQ(s.dates[0], "1970-01-03");  // the absurd offset counts as none
    EXPECT_FLOAT_EQ(s.values[0], 3.5f);
}

TEST(MarketTwelveData, AbsurdPricesAreSkipped)
{
    market_series_t s;
    EXPECT_EQ(
        market_parse_twelvedata("{\"values\":[{\"datetime\":\"2026-01-02\",\"close\":\"1e300\"},"
                                "{\"datetime\":\"2026-01-01\",\"close\":\"inf\"}]}",
                                &s),
        MARKET_PARSE_BAD);
}
