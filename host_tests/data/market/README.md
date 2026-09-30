# Market fixtures

Answers of the three sources of the `market-quotes` feature, for the parser and page tests (`host_tests/test_market.cpp`, `host_tests/test_market_screen.cpp`).
Saved on 2026-09-30 from the public interfaces; prices of shares and indices are shown as facts of that day, only for testing.

| File | What it is |
| --- | --- |
| `yahoo-aapl.json` | Yahoo chart endpoint, `AAPL`, range one month, daily |
| `yahoo-eunl-de.json` | the same for an ETF on Xetra (`EUNL.DE`): a day without a price (`null` close), a UTC offset of +2 h |
| `yahoo-gdaxi.json` | an index (`^GDAXI`) |
| `yahoo-gold-future.json` | a future (`GC=F`): no `longName`, only `shortName` |
| `yahoo-btc-eur.json` | a crypto pair (`BTC-EUR`): 32 days, all trading days |
| `yahoo-not-found.json` | the answer for a symbol Yahoo does not know (`Not Found`) |
| `twelvedata-aapl.json` | Twelve Data `time_series`, `AAPL`, 30 days, prices as strings, newest first |
| `twelvedata-btc-usd.json` | the same for `BTC/USD`: no `currency`, only `currency_quote` with the name of the currency |
| `twelvedata-bad-key.json` | the answer for a missing or wrong key (`status: error`, `code: 401`) |
| `alphavantage-ibm-daily.json` | Alpha Vantage `TIME_SERIES_DAILY` for `IBM` (the only symbol its demo key serves), 100 days, newest first |
| `alphavantage-information.json` | the message the demo key gets for any other symbol |

The answers to rate limits and to other error codes are not saved here (they cannot be provoked on purpose); the tests build them by hand in the documented shapes
and say so in a comment. No personal API key is stored here or anywhere in the repository (the requests used the public `demo` key of the services).
