# ECB reference-rate fixtures

Real answers of the ECB data portal (`https://data-api.ecb.europa.eu/service/data/EXR/...`), saved on 2026-09-30 for the parser test of the
`finance-snapshot` feature (`host_tests/test_finance.cpp`). The rates are public reference data of the European Central Bank
(source: ECB, https://data.ecb.europa.eu, reproduction is allowed with acknowledgement of the source).

| File | Request |
| --- | --- |
| `exr-four-30.csv` | `D.USD+GBP+JPY+CHF.EUR.SP00.A?lastNObservations=30&format=csvdata&detail=dataonly` - the answer the frame asks for (the ECB orders the series by its own keys: CHF, GBP, JPY, USD) |
| `exr-usd-full-3.csv` | `D.USD.EUR.SP00.A?lastNObservations=3&format=csvdata` - the full column set with quoted titles, to check that the parser copes with it |
