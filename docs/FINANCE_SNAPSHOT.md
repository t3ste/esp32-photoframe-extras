# Exchange-rate screen

> **Build option:** compiled in only with `python build.py --with finance-snapshot` (needs `info-screens`; the build pulls it in together
> with what that needs). Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

A page for the [information screens](INFO_SCREENS.md) with the **reference exchange rates of the European Central Bank**: one row per
currency with the price of one euro in that currency in big digits, the change against the working day before (a green arrow up, a red
arrow down) and a line chart of the last 30 working days.

## Settings

- Settings -> Agenda -> Information screens: tick **Exchange rates**.
- **Currencies:** up to four three-letter codes, separated by commas (`USD, GBP, CHF, JPY`); `EUR` is left out, doubles and anything that
  is not three letters are ignored. Empty means USD, GBP, CHF and JPY. The ECB currently publishes AUD, BRL, CAD, CHF, CNY, CZK, DKK,
  GBP, HKD, HUF, IDR, ILS, INR, ISK, JPY, KRW, MXN, MYR, NOK, NZD, PHP, PLN, RON, SEK, SGD, THB, TRY, USD and ZAR (checked 2026-09-30).
  A code the ECB has no rate for is left out of the page - so are currencies it still lists but stopped publishing (BGN, RUB, HRK, ...):
  a rate that is more than 14 days old does not pass for a current one.
- The frame must be online when the page is drawn; the rates are fetched at that moment, in one small request (about 2 KB per currency).
  No account and no key are needed.

## What the page shows

| Part | Content |
| --- | --- |
| Header | a blue band with the heading and the date of the newest rate |
| Row | the currency code and `1 EUR =`, the rate in big digits (4, 3 or 2 decimals, by size), the change in percent against the working day before with a coloured arrow, a line chart of the last 30 working days ending in a coloured dot |
| Footer | the source and, under it, when the rates were fetched (`Updated 30 Sep 14:35`); under each line chart how long it runs (`41 d`) |

The ECB publishes once a working day in the afternoon (the rates are fixed at 14:15 Central European Time); on weekends and bank holidays the newest rate is that of the
last working day. The colour shows the *direction of the rate*, not good or bad: a green arrow means the euro bought more of that currency
than the day before.

If there is nothing to show the page says why (no network on this wake, or the ECB did not answer) instead of an empty picture; the next
turn of the rotation tries again.

## Limits

- Currencies only. Stocks, ETFs, indices, crypto and commodities are the [markets page](MARKET_QUOTES.md) (`market-quotes`), which asks a chain of sources - Yahoo's
  chart endpoint (no key, worldwide, but unofficial), Twelve Data and Alpha Vantage (free keys) - instead of one.
- The rates are reference rates at a fixed time of the day, not live quotes.
- Up to four currencies, 30 working days.
