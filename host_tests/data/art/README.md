# Fixtures of the artworks mode

Real answers of the three museum services (recorded 2026-10-01), used by `host_tests/test_art.cpp`. They contain
only what the museums publish about their works - no keys, no personal data.

| File | What it is |
| --- | --- |
| `rijks-search-painting.json` | Rijksmuseum search `type=painting&imageAvailable=true`: 100 record addresses |
| `rijks-object.json` | The record of one painting (`?_profile=la-framed`): title, maker, year, the visual item |
| `rijks-visual-item.json` | Its visual item: the rights statement (public domain mark) and the digital object |
| `rijks-digital-object.json` | Its digital object: the IIIF address and the "downloadbaar" statement |
| `smk-item-painting.json` | SMK search with one hit, a painting (Danish title) |
| `smk-item-print.json` | The same with a print (a range of years) |
| `smk-count.json` | SMK search with `rows=0` (only `found`), kind `tegning` |
| `si-row-painting.json` | Smithsonian search with one hit, an American Art Museum painting (CC0) |

The tests make the unwanted variants (not public domain, another host, no picture, not for download) by changing a
word in these files, so the files stay what the services sent.
