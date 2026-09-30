// The shape of a personal API key of Twelve Data or Alpha Vantage (build option market-quotes):
// letters and digits, 4 to 64 - what the frame accepts (market_key_valid() in main/market_quotes.c).
// Anything else would never be sent, so the form says so while it is typed.
export function isMarketKey(value) {
  return typeof value === "string" && /^[A-Za-z0-9]{4,64}$/.test(value);
}
