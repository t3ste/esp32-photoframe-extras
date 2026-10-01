cmake_minimum_required(VERSION 3.14)
project(esp32_photoframe_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# cJSON for the parsers that read JSON (the fuel-prices and market-quotes features): the copy the firmware build fetched
# (managed_components/ is not in the repository), else the same release from upstream.
set(CJSON_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__cjson/cJSON")
if(EXISTS "${CJSON_LOCAL}/cJSON.c")
  set(CJSON_DIR "${CJSON_LOCAL}")
else()
  # only the source is used by the tests; keep cJSON's own tests and install rules out of the build
  set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
  set(ENABLE_CJSON_UNINSTALL OFF CACHE BOOL "" FORCE)
  set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    cjson
    URL https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.19.tar.gz
  )
  FetchContent_MakeAvailable(cjson)
  set(CJSON_DIR "${cjson_SOURCE_DIR}")
endif()

# Enable testing
enable_testing()

# There is no board on the host: skip the hardware checks of main/feature_config.h.
# (stubs/sdkconfig.h is empty, so every optional feature is off unless a target
# defines its CONFIG_* symbols - see FORK_FEATURE_DEFS below.)
add_compile_definitions(FEATURE_CONFIG_NO_HW_CHECK)

# Every optional feature switched on, as `build.py --all-features` builds it. The fork
# tests below compile the feature modules and the shared image pipeline with it; the
# plain tests above stay on the upstream code.
set(FORK_FEATURE_DEFS
  CONFIG_FEATURE_TELEGRAM
  CONFIG_FEATURE_OVERLAYS
  CONFIG_FEATURE_AGENDA
  CONFIG_FEATURE_CHIMES
  CONFIG_FEATURE_CLIMATE
  CONFIG_FEATURE_ALARMCLOCK
  CONFIG_FEATURE_VOICE_STOP
  CONFIG_FEATURE_BATTERY_HISTORY
  CONFIG_FEATURE_DISPLAY_HISTORY
  CONFIG_FEATURE_HTTPS
  CONFIG_FEATURE_OFFLINE_HOTSPOT
  CONFIG_FEATURE_ERROR_BANNER
  CONFIG_FEATURE_OTA_CHANNEL
  CONFIG_FEATURE_WIFI_RESILIENCE
  CONFIG_FEATURE_WEBCAL
  CONFIG_FEATURE_SOURCE_AUTH
  CONFIG_FEATURE_CALDAV
  CONFIG_FEATURE_CALDAV_TODO
  CONFIG_FEATURE_GLYPHS
  CONFIG_FEATURE_INFO_SCREENS
  CONFIG_FEATURE_CHORE_WHEEL
  CONFIG_FEATURE_WEATHER_SCREEN
  CONFIG_FEATURE_FACT_OF_THE_DAY
  CONFIG_FEATURE_FINANCE_SNAPSHOT
  CONFIG_FEATURE_FUEL_PRICES
  CONFIG_FEATURE_MARKET_QUOTES
  CONFIG_FEATURE_UPLOAD_DEDUP
  CONFIG_FEATURE_MULTI_UPLOAD
  CONFIG_FEATURE_FACECROP
  CONFIG_FORK_FIXES
  CONFIG_FORK_HTTP_FETCH
  CONFIG_FORK_WEATHER
  CONFIG_FORK_CLIMATE_CORE
  CONFIG_FORK_EXIF
  CONFIG_FORK_AUDIO_HAL
  CONFIG_FORK_IMAGE_PIPELINE
)

# Cron engine tests
add_executable(
  cron_test
  test_cron.cpp
  ../main/cron.c
)

target_link_libraries(
  cron_test
  GTest::gtest_main
)

target_include_directories(
  cron_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Wake-schedule simulation (early-wake re-sleep loop vs. the real cron engine)
add_executable(
  wake_schedule_test
  test_wake_schedule.cpp
  ../main/cron.c
)

target_link_libraries(
  wake_schedule_test
  GTest::gtest_main
)

target_include_directories(
  wake_schedule_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Backoff between failed unattended network wakes (#121), against the real cron engine
add_executable(
  network_backoff_test
  test_network_backoff.cpp
  ../main/network_backoff.c
  ../main/cron.c
)

target_link_libraries(
  network_backoff_test
  GTest::gtest_main
)

target_include_directories(
  network_backoff_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# HTTP Basic credential checking for the optional device API password (#130)
add_executable(
  http_auth_test
  test_http_auth.cpp
  ../main/http_auth.c
)

target_link_libraries(
  http_auth_test
  GTest::gtest_main
)

target_include_directories(
  http_auth_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Login inside a download URL (user:password@host, source-auth feature)
add_executable(
  source_auth_test
  test_source_auth.cpp
  ../main/source_auth.c
)

target_link_libraries(
  source_auth_test
  GTest::gtest_main
)

target_include_directories(
  source_auth_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Duplicate detection index and MD5 (upload-dedup feature)
add_executable(
  dedup_test
  test_dedup.cpp
  test_dedup_payload.cpp
  ../main/dedup.c
  ../main/dedup_payload.c
)

target_link_libraries(
  dedup_test
  GTest::gtest_main
  PNG::PNG
  m
)

target_include_directories(
  dedup_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# CalDAV calendar queries: request body and multistatus extraction (caldav feature)
add_executable(
  caldav_test
  test_caldav.cpp
  ../main/caldav.c
)

target_link_libraries(
  caldav_test
  GTest::gtest_main
)

target_include_directories(
  caldav_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# iCalendar to-dos as ToDo-column items (caldav-todo feature)
add_executable(
  vtodo_test
  test_vtodo.cpp
  ../main/vtodo.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  vtodo_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  vtodo_test
  GTest::gtest_main
)

target_include_directories(
  vtodo_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Extra glyphs (umlauts, sharp s, degree, euro) in the bitmap font (glyphs feature); the real Font24
# table of the e-paper driver is linked so the umlauts are checked against the font itself
add_executable(
  glyph_extras_test
  test_glyph_extras.cpp
  ../main/glyph_extras.c
  ../components/epaper_src/Fonts/font24.c
)

target_link_libraries(
  glyph_extras_test
  GTest::gtest_main
)

target_include_directories(
  glyph_extras_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# Drawing primitives of the info screens (info-screens feature); the real Font24 table and the
# extra glyphs are linked, so text is checked against the font itself
add_executable(
  screen_canvas_test
  test_screen_canvas.cpp
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  screen_canvas_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  screen_canvas_test
  GTest::gtest_main
  m
)

target_include_directories(
  screen_canvas_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# Big stroke digits for the info screens (temperatures, prices)
add_executable(
  screen_digits_test
  test_screen_digits.cpp
  ../main/screen_digits.c
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  screen_digits_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  screen_digits_test
  GTest::gtest_main
  m
)

target_include_directories(
  screen_digits_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# Facts of the fact-of-the-day page: pack format, pick of the day, built-in facts
add_executable(
  fact_pack_test
  test_fact_pack.cpp
  ../main/fact_pack.c
)

target_link_libraries(
  fact_pack_test
  GTest::gtest_main
)

target_include_directories(
  fact_pack_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Exchange-rate parser and page of the finance-snapshot feature (real ECB answers as fixtures)
add_executable(
  finance_test
  test_finance.cpp
  ../main/fx_rates.c
  ../main/screen_finance.c
  ../main/screen_digits.c
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../main/info_screens_core.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  finance_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  FINANCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/ecb"
)

target_link_libraries(
  finance_test
  GTest::gtest_main
  m
)

target_include_directories(
  finance_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# Answer parser and page of the fuel-prices feature (Tankerkoenig answers as fixtures, the real cJSON)
add_executable(
  fuel_test
  test_fuel.cpp
  ../main/fuel_prices.c
  ../main/json_scan.c
  ../main/screen_fuel.c
  ../main/screen_digits.c
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../main/info_screens_core.c
  ${CJSON_DIR}/cJSON.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  fuel_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  FUEL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/tankerkoenig"
)

target_link_libraries(
  fuel_test
  GTest::gtest_main
  m
)

target_include_directories(
  fuel_test
  PRIVATE
  ${CJSON_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# Parsers, requests, quota and cache of the market-quotes feature (real answers of the three sources as
# fixtures, the real cJSON)
add_executable(
  market_test
  test_market.cpp
  test_market_screen.cpp
  test_json_scan.cpp
  ../main/market_quotes.c
  ../main/json_scan.c
  ../main/screen_markets.c
  ../main/screen_digits.c
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../main/info_screens_core.c
  ${CJSON_DIR}/cJSON.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  market_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  MARKET_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/market"
)

target_link_libraries(
  market_test
  GTest::gtest_main
  m
)

target_include_directories(
  market_test
  PRIVATE
  ${CJSON_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

# The device side of the markets page (main/market_service.c) with the settings and the HTTP helper faked,
# real answers as fixtures, and the cache as a real file in a temporary directory
add_executable(
  market_service_test
  test_market_service.cpp
  ../main/market_service.c
  ../main/market_quotes.c
  ../main/json_scan.c
  ${CJSON_DIR}/cJSON.c
)

target_compile_definitions(
  market_service_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  MARKET_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/market"
  FS_MOUNT_POINT="/tmp/pf_market_service_test"
)

target_link_libraries(
  market_service_test
  GTest::gtest_main
  m
)

target_include_directories(
  market_service_test
  PRIVATE
  ${CJSON_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# The pure parts of the info screens: ISO weeks, names, rotation, list parsing
add_executable(
  info_screens_core_test
  test_info_screens_core.cpp
  ../main/info_screens_core.c
)

target_link_libraries(
  info_screens_core_test
  GTest::gtest_main
)

target_include_directories(
  info_screens_core_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# The screens themselves, drawn at the sizes of the boards (info-screens features). All screens
# share the sources list below; the render harness draws the same cases as pictures.
set(INFO_SCREEN_SOURCES
  ../main/screen_canvas.c
  ../main/glyph_extras.c
  ../main/info_screens_core.c
  ../main/screen_chore_wheel.c
  ../main/screen_digits.c
  ../main/screen_weather.c
  ../main/screen_fact.c
  ../main/fact_pack.c
  ../main/screen_finance.c
  ../main/fx_rates.c
  ../main/screen_fuel.c
  ../main/fuel_prices.c
  ../main/json_scan.c
  ../main/screen_markets.c
  ../main/market_quotes.c
  ${CJSON_DIR}/cJSON.c
  ../components/epaper_src/Fonts/font24.c
)

add_executable(
  screens_test
  test_screens.cpp
  test_info_notes.cpp
  ${INFO_SCREEN_SOURCES}
)

add_executable(
  render_screens
  render_screens.cpp
  ${INFO_SCREEN_SOURCES}
)

foreach(screens_target screens_test render_screens)
  target_compile_definitions(${screens_target} PRIVATE ${FORK_FEATURE_DEFS})
  target_include_directories(
    ${screens_target}
    PRIVATE
    ${CJSON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
  )
endforeach()

target_link_libraries(screens_test GTest::gtest_main m)
target_link_libraries(render_screens PNG::PNG m)

# Microphone level maths (RMS/peak in dBFS, text bar) behind the console level test
# (voice-stop feature)
add_executable(
  mic_level_test
  test_mic_level.cpp
  test_mic_detect.cpp
  ../main/mic_level.c
  ../main/mic_detect.c
)

target_compile_definitions(
  mic_level_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  mic_level_test
  GTest::gtest_main
)

target_include_directories(
  mic_level_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Ring pattern of the alarm (notes / pauses / when the microphone must ignore the room)
# (alarmclock feature)
add_executable(
  alarm_pattern_test
  test_alarm_pattern.cpp
  ../main/alarm_pattern.c
)

target_compile_definitions(
  alarm_pattern_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  alarm_pattern_test
  GTest::gtest_main
)

target_include_directories(
  alarm_pattern_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/board_hal/include
)

# Thinning a long history log out to a chartable number of points (climate feature)
add_executable(
  history_decimate_test
  test_history_decimate.cpp
)

target_link_libraries(
  history_decimate_test
  GTest::gtest_main
)

target_include_directories(
  history_decimate_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Stop-word (keyword) recognition: MFCC + DTW template matching, tested on
# text-to-speech samples (host_tests/data/kws) (voice-stop feature)
add_executable(
  kws_test
  test_kws.cpp
  ../main/kws.c
  ../main/alarm_pattern.c
)

target_compile_definitions(
  kws_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  KWS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/kws"
)

target_link_libraries(
  kws_test
  GTest::gtest_main
)

target_include_directories(
  kws_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Headline/RSS extraction (pure parsing logic split out of headlines.c's
# network-calling headlines_fetch(), specifically so it's host-testable
# without a real HTTP stack) (overlays feature)
add_executable(
  headlines_test
  test_headlines.cpp
  ../main/headlines.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  headlines_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  headlines_test
  GTest::gtest_main
)

target_include_directories(
  headlines_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# ToDo (todo.txt) parsing (pure parsing logic split out of todo.c's
# network-calling todo_fetch(), specifically so it's host-testable without
# a real HTTP stack) - same shape as headlines_test above (agenda feature)
add_executable(
  todo_test
  test_todo.cpp
  ../main/todo.c
  ../main/caldav.c
  ../main/caldav_fetch.c
  ../main/vtodo.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  todo_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  CALDAV_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/caldav"
)

target_link_libraries(
  todo_test
  GTest::gtest_main
)

target_include_directories(
  todo_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Calendar (ICS/iCalendar) parsing (pure parsing logic split out of
# calendar_ics.c's network-calling calendar_ics_fetch(), specifically so
# it's host-testable without a real HTTP stack) - same shape as
# headlines_test/todo_test above (agenda feature)
add_executable(
  calendar_ics_test
  test_calendar_ics.cpp
  ../main/calendar_ics.c
  ../main/caldav.c
  ../main/caldav_fetch.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  calendar_ics_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  CALDAV_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/caldav"
)

target_link_libraries(
  calendar_ics_test
  GTest::gtest_main
)

target_include_directories(
  calendar_ics_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Demo package (examples/waveshare_photopainter_73/, docs/DEMO_PLAN.md): the
# example calendars and todo.txt run through the firmware's own parsers, not
# a re-implementation - links calendar_ics.c and todo.c directly, same stubs
# as calendar_ics_test/todo_test above.
add_executable(
  example_calendars_test
  test_example_calendars.cpp
  ../main/calendar_ics.c
  ../main/caldav.c
  ../main/caldav_fetch.c
  ../main/vtodo.c
  ../main/todo.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  example_calendars_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples/waveshare_photopainter_73"
)

target_link_libraries(
  example_calendars_test
  GTest::gtest_main
)

target_include_directories(
  example_calendars_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Last-crash record: reason and the one-line summary
add_executable(
  crash_record_test
  test_crash_record.cpp
  ../main/crash_record.c
)

target_link_libraries(
  crash_record_test
  GTest::gtest_main
)

target_include_directories(
  crash_record_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(cron_test)
gtest_discover_tests(wake_schedule_test)
gtest_discover_tests(http_auth_test)
gtest_discover_tests(source_auth_test)
gtest_discover_tests(caldav_test)
gtest_discover_tests(dedup_test)
gtest_discover_tests(vtodo_test)
gtest_discover_tests(glyph_extras_test)
gtest_discover_tests(screen_canvas_test)
gtest_discover_tests(screen_digits_test)
gtest_discover_tests(fact_pack_test)
gtest_discover_tests(finance_test)
gtest_discover_tests(fuel_test)
gtest_discover_tests(market_test)
gtest_discover_tests(market_service_test)
gtest_discover_tests(info_screens_core_test)
gtest_discover_tests(screens_test)
gtest_discover_tests(network_backoff_test)
gtest_discover_tests(crash_record_test)
gtest_discover_tests(mic_level_test)
gtest_discover_tests(alarm_pattern_test)
gtest_discover_tests(history_decimate_test)
gtest_discover_tests(kws_test)
gtest_discover_tests(headlines_test)
gtest_discover_tests(todo_test)
gtest_discover_tests(calendar_ics_test)
gtest_discover_tests(example_calendars_test)

# On-device image pipeline characterization tests (host build of
# main/image_processor.c against ESP-IDF stubs + system libpng)
find_package(PNG REQUIRED)

add_executable(
  image_pipeline_test
  test_image_pipeline.cpp
  ../main/image_processor.c
  stubs/esp_stubs.c
  stubs/fake_display_manager.c
  stubs/fake_config_manager.c
  stubs/fake_processing_settings.c
)

target_include_directories(
  image_pipeline_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_link_libraries(
  image_pipeline_test
  GTest::gtest_main
  PNG::PNG
  m
)

gtest_discover_tests(image_pipeline_test)

# .current.* / thumbnail lifecycle tests for the shared display flow.
# FS_MOUNT_POINT is redirected to a local directory so the tests exercise
# the real path scheme against the host filesystem.
add_executable(
  display_flow_test
  test_display_flow.cpp
  ../main/display_flow.c
  ../main/image_processor.c
  stubs/esp_stubs.c
  stubs/fake_display_manager.c
  stubs/fake_config_manager.c
  stubs/fake_processing_settings.c
  stubs/fake_storage.c
)

target_compile_definitions(
  display_flow_test
  PRIVATE
  FS_MOUNT_POINT="pf_storage"
)

target_include_directories(
  display_flow_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_link_libraries(
  display_flow_test
  GTest::gtest_main
  PNG::PNG
  m
)

gtest_discover_tests(display_flow_test)

# The same two characterization tests once more, on the optional image pipeline
# (CONFIG_FORK_IMAGE_PIPELINE: caption/overlay drawing, streaming JPEG decode, ...).
# The plain variants above prove the upstream behaviour is untouched.
foreach(test_name image_pipeline display_flow)
  set(fork_target ${test_name}_fork_test)
  set(fork_sources
    test_${test_name}.cpp
    ../main/image_processor.c
    ../main/glyph_extras.c
    stubs/esp_stubs.c
    stubs/fake_display_manager.c
    stubs/fake_config_manager.c
    stubs/fake_processing_settings.c
    stubs/fork_stubs.c
    ../components/epaper_src/Fonts/font24.c
  )
  if(test_name STREQUAL display_flow)
    list(APPEND fork_sources ../main/display_flow.c stubs/fake_storage.c)
  endif()

  add_executable(${fork_target} ${fork_sources})

  target_compile_definitions(
    ${fork_target}
    PRIVATE
    ${FORK_FEATURE_DEFS}
  )
  if(test_name STREQUAL display_flow)
    target_compile_definitions(${fork_target} PRIVATE FS_MOUNT_POINT="pf_storage_fork")
  endif()

  target_include_directories(
    ${fork_target}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
  )

  target_link_libraries(
    ${fork_target}
    GTest::gtest_main
    PNG::PNG
    m
  )

  gtest_discover_tests(${fork_target} TEST_SUFFIX " (fork)")
endforeach()

# The umlaut/degree/euro glyphs through the real sanitizer and text drawing of image_processor.c
# (glyphs feature)
add_executable(
  glyph_text_test
  test_glyph_text.cpp
  ../main/image_processor.c
  ../main/glyph_extras.c
  ../main/screen_canvas.c
  stubs/esp_stubs.c
  stubs/fake_display_manager.c
  stubs/fake_config_manager.c
  stubs/fake_processing_settings.c
  stubs/fork_stubs.c
  ../components/epaper_src/Fonts/font24.c
)

target_compile_definitions(
  glyph_text_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_include_directories(
  glyph_text_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
)

target_link_libraries(
  glyph_text_test
  GTest::gtest_main
  PNG::PNG
  m
)

gtest_discover_tests(glyph_text_test)
