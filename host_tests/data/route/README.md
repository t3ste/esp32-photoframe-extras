# Test answers of the travel-time providers

Used by `host_tests/test_route.cpp` (the readers of main/route_time.c) and `test_route_service.cpp`.

**These are not recorded answers.** The success answers are assembled by hand from the structure the providers document
(TomTom Search API `geocode`, TomTom Routing API `calculateRoute` with `routeRepresentation=summaryOnly`, HERE Geocoding & Search
`geocode`, HERE Routing API v8 `routes` with `return=summary`), with public landmarks in Berlin as places (the Brandenburg Gate,
Berlin Hauptbahnhof, Alexanderplatz) and invented numbers. The error answers `*-error-401.json` were recorded on 2026-10-03 from
the live services with an invalid key; the other error answers follow the documentation.

On 2026-10-03 a real TomTom key was used through a frame: the look-up, the check and the travel time worked, so the readers cope with the real answers. The raw bodies cannot be had
from the frame (its key is write-only), so these files are still assembled ones: to replace the success answers with recorded ones a key is needed outside the frame - for a key
of each provider, record them (public places only) and keep the fields these tests read.
