// The device side of the markets page (main/market_service.c) on the PC: the settings and the HTTP
// helper are faked, the answers are the real ones of host_tests/data/market, the cache is a real
// file in a temporary directory.
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "config.h"
#include "esp_err.h"
#include "market_quotes.h"
#include "market_service.h"
}

// ---- the fakes ---------------------------------------------------------------------------------

namespace
{

struct Reply {
    std::string url_part;  // the reply is for the first request whose address holds this
    esp_err_t err;         // ESP_OK: the server answered
    int status;
    std::string body;
};

std::vector<Reply> g_replies;
std::vector<std::string> g_urls;

std::string g_symbols;
bool g_yahoo = true;
std::string g_key_td, g_key_av;
std::string g_quota;

std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(MARKET_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int calls(const char *host)
{
    int n = 0;
    for (const std::string &url : g_urls) {
        n += url.find(host) != std::string::npos ? 1 : 0;
    }
    return n;
}

void reply(const std::string &part, int status, const std::string &body)
{
    g_replies.push_back({part, ESP_OK, status, body});
}

void no_answer(const std::string &part)
{
    g_replies.push_back({part, ESP_FAIL, 0, ""});
}

const std::string kYahoo = "finance.yahoo.com/v8/finance/chart/";
const std::string kTd = "api.twelvedata.com/time_series?symbol=";
const std::string kAv = "alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=";

// Yahoo answers for the four default symbols
void yahoo_defaults()
{
    reply(kYahoo + "AAPL?", 200, fixture("yahoo-aapl.json"));
    reply(kYahoo + "EUNL.DE?", 200, fixture("yahoo-eunl-de.json"));
    reply(kYahoo + "%5EGDAXI?", 200, fixture("yahoo-gdaxi.json"));
    reply(kYahoo + "BTC-EUR?", 200, fixture("yahoo-btc-eur.json"));
}

std::string cache_text()
{
    std::ifstream in(MARKET_CACHE_PATH, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_cache(const std::string &text)
{
    std::ofstream out(MARKET_CACHE_PATH, std::ios::binary);
    out << text;
}

}  // namespace

extern "C" {

const char *config_manager_get_market_symbols(void)
{
    return g_symbols.c_str();
}
bool config_manager_get_market_yahoo(void)
{
    return g_yahoo;
}
const char *config_manager_get_market_key_twelvedata(void)
{
    return g_key_td.c_str();
}
const char *config_manager_get_market_key_alphavantage(void)
{
    return g_key_av.c_str();
}
const char *config_manager_get_market_quota(void)
{
    return g_quota.c_str();
}
void config_manager_set_market_quota(const char *text)
{
    g_quota = text;
}

esp_err_t http_fetch_get_once(const char *url, int timeout_ms, size_t max_response_bytes,
                              char **out_body, size_t *out_len, int *out_status,
                              const char *user_agent)
{
    (void) timeout_ms;
    (void) max_response_bytes;
    EXPECT_NE(user_agent, nullptr);  // Yahoo answers 429 to a request without one
    g_urls.push_back(url);
    *out_body = nullptr;
    *out_status = 0;
    if (out_len) {
        *out_len = 0;
    }
    for (const Reply &r : g_replies) {
        if (std::string(url).find(r.url_part) == std::string::npos) {
            continue;
        }
        if (r.err != ESP_OK) {
            return r.err;
        }
        *out_status = r.status;
        if (!r.body.empty()) {
            *out_body = strdup(r.body.c_str());
            if (out_len) {
                *out_len = r.body.size();
            }
        }
        return ESP_OK;
    }
    return ESP_FAIL;  // nobody there
}

}  // extern "C"

class MarketService : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        mkdir(FS_MOUNT_POINT, 0777);
        unlink(MARKET_CACHE_PATH);
        g_replies.clear();
        g_urls.clear();
        g_symbols.clear();
        g_yahoo = true;
        g_key_td.clear();
        g_key_av.clear();
        g_quota.clear();
    }
    void TearDown() override
    {
        unlink(MARKET_CACHE_PATH);
    }

    markets_screen_data_t load(bool wifi = true)
    {
        markets_screen_data_t data;
        market_service_load(&data, wifi);
        return data;
    }
    static long today()
    {
        return (long) (time(nullptr) / 86400);
    }
};

// ---- tests -------------------------------------------------------------------------------------

TEST_F(MarketService, WithoutASourceItSaysSoAndAsksNobody)
{
    g_yahoo = false;
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_SOURCE);
    EXPECT_TRUE(g_urls.empty());
    // a key that cannot be one counts as none
    g_key_td = "bad key!";
    g_key_av = "abc";
    data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_SOURCE);
    EXPECT_TRUE(g_urls.empty());
}

TEST_F(MarketService, EmptySymbolsAreTheBuiltInFour)
{
    yahoo_defaults();
    markets_screen_data_t data = load();
    ASSERT_EQ(data.status, MARKETS_SCREEN_OK);
    ASSERT_EQ(data.count, 4);
    const char *expect[] = {"AAPL", "EUNL.DE", "^GDAXI", "BTC-EUR"};
    for (int i = 0; i < 4; i++) {
        EXPECT_STREQ(data.series[i].symbol, expect[i]);
        EXPECT_GT(data.series[i].count, 10);
        EXPECT_EQ(data.series[i].provider, MARKET_YAHOO);
        EXPECT_FALSE(data.series[i].stale);
        EXPECT_GT(data.series[i].fetched, 1700000000L);
    }
    EXPECT_EQ(calls("yahoo.com"), 4);
    EXPECT_EQ(calls("twelvedata.com"), 0);
}

TEST_F(MarketService, TheSymbolsOfTheSettingsAreUsedAsTypedButCleaned)
{
    yahoo_defaults();
    g_symbols = "aapl, AAPL; btc-eur  x&y";  // a double, a bad one
    markets_screen_data_t data = load();
    ASSERT_EQ(data.count, 2);
    EXPECT_STREQ(data.series[0].symbol, "AAPL");
    EXPECT_STREQ(data.series[1].symbol, "BTC-EUR");
    EXPECT_EQ(calls("yahoo.com"), 2);
}

TEST_F(MarketService, MoreThanFourSymbolsAreCut)
{
    yahoo_defaults();
    g_symbols = "AAPL, EUNL.DE, ^GDAXI, BTC-EUR, MSFT, IBM";
    markets_screen_data_t data = load();
    EXPECT_EQ(data.count, 4);
    EXPECT_EQ(calls("yahoo.com"), 4);
}

TEST_F(MarketService, GoodAnswersAreKeptOnTheStorage)
{
    yahoo_defaults();
    load();
    std::string text = cache_text();
    EXPECT_EQ(text.rfind("PF-MARKETS 1\n", 0), 0u);
    market_series_t cached[4];
    EXPECT_EQ(market_cache_decode(text.c_str(), cached, 4), 4);
    EXPECT_STREQ(cached[0].symbol, "AAPL");
    EXPECT_STREQ(cached[0].name, "Apple Inc.");
}

TEST_F(MarketService, WithoutNetworkTheLastKnownPricesAreShownAsStale)
{
    yahoo_defaults();
    load();
    g_urls.clear();
    markets_screen_data_t data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    ASSERT_EQ(data.count, 4);
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(data.series[i].stale) << i;
        EXPECT_GT(data.series[i].count, 10);
    }
    EXPECT_TRUE(g_urls.empty());  // no network: nobody is asked
}

TEST_F(MarketService, WithoutNetworkAndWithoutCacheItIsTheNoNetworkMessage)
{
    markets_screen_data_t data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_NETWORK);
    EXPECT_TRUE(g_urls.empty());
}

TEST_F(MarketService, WhenEverythingFailsTheCacheStillShows)
{
    yahoo_defaults();
    load();
    g_replies.clear();  // from now on nobody answers
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(data.series[i].stale) << i;
    }
}

TEST_F(MarketService, WhenEverythingFailsAndNothingIsKeptItIsTheFailedMessage)
{
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_FETCH_FAILED);
    ASSERT_EQ(data.count, 4);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(data.series[i].count, 0);
        EXPECT_GT(strlen(data.series[i].symbol), 0u);
    }
    // a source that does not answer is asked once, not once per symbol
    EXPECT_EQ(calls("yahoo.com"), 1);
    EXPECT_TRUE(cache_text().empty());  // nothing to remember
}

TEST_F(MarketService, OneSymbolWithoutDataDoesNotSpoilTheOthers)
{
    yahoo_defaults();
    g_replies[1] = {kYahoo + "EUNL.DE?", ESP_OK, 404, fixture("yahoo-not-found.json")};
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_GT(data.series[0].count, 0);
    EXPECT_EQ(data.series[1].count, 0);
    EXPECT_STREQ(data.series[1].symbol, "EUNL.DE");
    EXPECT_GT(data.series[2].count, 0);
    // "not found" is about the symbol: Yahoo is still asked for the next ones
    EXPECT_EQ(calls("yahoo.com"), 4);
    // and the row without data is not written to the cache
    market_series_t cached[4];
    EXPECT_EQ(market_cache_decode(cache_text().c_str(), cached, 4), 3);
}

TEST_F(MarketService, ASourceThatIsDownFallsBackToTheNextOne)
{
    g_key_td = "demo";
    g_symbols = "AAPL, BTC-USD";
    reply(kYahoo, 500, "oops");
    reply(kTd + "AAPL&", 200, fixture("twelvedata-aapl.json"));
    reply(kTd + "BTC%2FUSD&", 200, fixture("twelvedata-btc-usd.json"));
    markets_screen_data_t data = load();
    ASSERT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_EQ(data.series[0].provider, MARKET_TWELVEDATA);
    EXPECT_EQ(data.series[1].provider, MARKET_TWELVEDATA);
    EXPECT_STREQ(data.series[0].symbol, "AAPL");
    EXPECT_STREQ(data.series[1].symbol, "BTC-USD");  // the user's notation, not "BTC/USD"
    EXPECT_STREQ(data.series[1].currency, "USD");
    EXPECT_EQ(calls("yahoo.com"), 2);  // a 500 says nothing about the next symbol
    EXPECT_EQ(calls("twelvedata.com"), 2);
}

TEST_F(MarketService, AServerThatDoesNotAnswerIsLeftOutForTheRestOfTheDraw)
{
    g_key_td = "demo";
    g_symbols = "AAPL, BTC-USD";
    no_answer(kYahoo);
    reply(kTd + "AAPL&", 200, fixture("twelvedata-aapl.json"));
    reply(kTd + "BTC%2FUSD&", 200, fixture("twelvedata-btc-usd.json"));
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_EQ(calls("yahoo.com"), 1);
    EXPECT_EQ(calls("twelvedata.com"), 2);
}

TEST_F(MarketService, ARefusedKeyIsNotTriedAgainInTheSameDraw)
{
    g_yahoo = false;
    g_key_td = "wrongkey1";
    g_symbols = "AAPL, BTC-USD, MSFT";
    reply(kTd, 401, fixture("twelvedata-bad-key.json"));
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_FETCH_FAILED);
    EXPECT_EQ(calls("twelvedata.com"), 1);
}

TEST_F(MarketService, AnAnswerOfRateLimitStopsTheSourceForTheDraw)
{
    g_yahoo = false;
    g_key_td = "somekey123";
    g_symbols = "AAPL, MSFT";
    reply(kTd, 429, "{\"code\":429,\"message\":\"limit\",\"status\":\"error\"}");
    load();
    EXPECT_EQ(calls("twelvedata.com"), 1);
}

TEST_F(MarketService, TheChainEndsAtAlphaVantageForWhatTheOthersCannotServe)
{
    g_key_td = "demo";
    g_key_av = "demo";
    g_symbols = "EUNL.DE";  // Twelve Data has no listings with a suffix
    reply(kYahoo, 429, "Too Many Requests");
    reply(kAv + "EUNL.DEX&", 200, fixture("alphavantage-ibm-daily.json"));
    markets_screen_data_t data = load();
    ASSERT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_EQ(data.series[0].provider, MARKET_ALPHAVANTAGE);
    EXPECT_STREQ(data.series[0].symbol, "EUNL.DE");
    EXPECT_EQ(calls("twelvedata.com"), 0);
}

TEST_F(MarketService, RequestsToTheLimitedSourcesAreCountedPerDay)
{
    g_yahoo = false;
    g_key_td = "demo";
    g_symbols = "AAPL, BTC-USD";
    reply(kTd + "AAPL&", 200, fixture("twelvedata-aapl.json"));
    reply(kTd + "BTC%2FUSD&", 200, fixture("twelvedata-btc-usd.json"));
    load();
    market_quota_t q;
    market_quota_parse(g_quota.c_str(), &q);
    EXPECT_EQ(q.day, today());
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 2);
    EXPECT_EQ(q.used[MARKET_YAHOO], 0);
    load();
    market_quota_parse(g_quota.c_str(), &q);
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 4);
}

TEST_F(MarketService, YahooIsNotCountedAndNothingIsWrittenForIt)
{
    yahoo_defaults();
    load();
    EXPECT_TRUE(g_quota.empty());
}

TEST_F(MarketService, ASourceWithNoQuotaLeftIsNotAsked)
{
    g_yahoo = false;
    g_key_td = "demo";
    g_symbols = "AAPL";
    char text[48];
    market_quota_t q = {today(), {0, 800, 0}};
    market_quota_format(&q, text, sizeof(text));
    g_quota = text;
    reply(kTd, 200, fixture("twelvedata-aapl.json"));
    markets_screen_data_t data = load();
    EXPECT_EQ(calls("twelvedata.com"), 0);
    EXPECT_EQ(data.status, MARKETS_SCREEN_FETCH_FAILED);
}

TEST_F(MarketService, ANewDayStartsTheCountFromZero)
{
    g_yahoo = false;
    g_key_td = "demo";
    g_symbols = "AAPL";
    char text[48];
    market_quota_t q = {today() - 1, {0, 800, 0}};  // yesterday the quota was used up
    market_quota_format(&q, text, sizeof(text));
    g_quota = text;
    reply(kTd, 200, fixture("twelvedata-aapl.json"));
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    market_quota_parse(g_quota.c_str(), &q);
    EXPECT_EQ(q.day, today());
    EXPECT_EQ(q.used[MARKET_TWELVEDATA], 1);
}

TEST_F(MarketService, ANameFromEarlierIsKeptWhenTheNewSourceHasNone)
{
    g_symbols = "AAPL";
    reply(kYahoo, 200, fixture("yahoo-aapl.json"));
    load();  // Yahoo: "Apple Inc."
    g_replies.clear();
    g_key_td = "demo";
    reply(kYahoo, 500, "oops");
    reply(kTd, 200, fixture("twelvedata-aapl.json"));
    markets_screen_data_t data = load();
    ASSERT_EQ(data.series[0].provider, MARKET_TWELVEDATA);
    EXPECT_STREQ(data.series[0].name, "Apple Inc.");
    EXPECT_FALSE(data.series[0].stale);
}

TEST_F(MarketService, OldCacheEntriesAreDropped)
{
    // a cache that is 15 days old (the limit is 14) is not shown
    long old = (long) time(nullptr) - 15L * 86400L;
    char text[256];
    snprintf(text, sizeof(text),
             "PF-MARKETS 1\nS|AAPL|Apple Inc.|USD|0|%ld|2\n2026-01-01 100\n2026-01-02 101\n", old);
    write_cache(text);
    g_symbols = "AAPL";
    markets_screen_data_t data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_NETWORK);
    // one day younger is still shown
    snprintf(text, sizeof(text),
             "PF-MARKETS 1\nS|AAPL|Apple Inc.|USD|0|%ld|2\n2026-01-01 100\n2026-01-02 101\n",
             old + 2L * 86400L);
    write_cache(text);
    data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_TRUE(data.series[0].stale);
}

TEST_F(MarketService, ADamagedCacheIsIgnored)
{
    write_cache("PF-MARKETS 1\nS|AAPL|Apple|USD|0|zzz|9\n2026-01-01 nonsense\n");
    g_symbols = "AAPL";
    markets_screen_data_t data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_NETWORK);
    write_cache(std::string(5000, 'x'));  // longer than the buffer, not a cache
    data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_NETWORK);
    write_cache(std::string("PF-MARKETS 1\n") +
                std::string(6000, 'S'));  // a cache that runs past the buffer
    data = load(false);
    EXPECT_EQ(data.status, MARKETS_SCREEN_NO_NETWORK);
}

TEST_F(MarketService, TheCacheOnlyAnswersForItsOwnSymbols)
{
    yahoo_defaults();
    load();
    g_symbols = "AAPL, MSFT";  // MSFT was never fetched
    markets_screen_data_t data = load(false);
    ASSERT_EQ(data.count, 2);
    EXPECT_GT(data.series[0].count, 0);
    EXPECT_EQ(data.series[1].count, 0);
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
}

TEST_F(MarketService, RequestsNeverCarryTheKeyInAnythingButTheirOwnAddress)
{
    g_yahoo = false;
    g_key_td = "secretkey123";
    g_symbols = "AAPL";
    reply(kTd, 200, fixture("twelvedata-aapl.json"));
    load();
    ASSERT_EQ(g_urls.size(), 1u);
    EXPECT_NE(g_urls[0].find("apikey=secretkey123"), std::string::npos);
    EXPECT_EQ(g_urls[0].rfind("https://api.twelvedata.com/", 0), 0u);
}

TEST_F(MarketService, IfTheCacheCannotBeWrittenThePricesAreStillShown)
{
    yahoo_defaults();
    rmdir(FS_MOUNT_POINT);  // the storage is gone
    markets_screen_data_t data = load();
    EXPECT_EQ(data.status, MARKETS_SCREEN_OK);
    EXPECT_GT(data.series[0].count, 0);
    mkdir(FS_MOUNT_POINT, 0777);
}
