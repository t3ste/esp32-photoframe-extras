# Tankerkoenig fixtures

Answers of the Tankerkoenig API (`https://creativecommons.tankerkoenig.de/json/list.php`) for the parser test of the `fuel-prices` feature
(`host_tests/test_fuel.cpp`). Data of the Markttransparenzstelle fuer Kraftstoffe, provided by tankerkoenig.de under CC BY 4.0
(https://creativecommons.tankerkoenig.de).

| File | What it is |
| --- | --- |
| `list-e5-demo.json` | The service's answer for the public demo key of its documentation (test prices, all 1.009), saved on 2026-09-30 |
| `error-bad-key.json` | The service's answer for a key it does not know (`"ok": false` with a message) |
| `list-diesel-mixed.json` | Written by hand in the documented format: different prices, a closed station, a station without a price and one with the price 0, umlauts as `ä` escapes, long names |

No personal API key is stored here or anywhere in the repository.
