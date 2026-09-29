set(SOURCES
    "album_manager.c"
    "cert_pin.c"
    "color_palette.c"
    "config_manager.c"
    "debug_log.c"
    "display_flow.c"
    "display_manager.c"
    "dns_server.c"
    "ha_integration.c"
    "http_server.c"
    "image_processor.c"
    "main.c"
    "mdns_service.c"
    "network_backoff.c"
    "ota_manager.c"
    "periodic_tasks.c"
    "png_decoder.c"
    "power_manager.c"
    "processing_settings.c"
    "splash_screen.c"
    "storage.c"
    "cron.c"
    "utils.c"
    "http_auth.c"
    "wifi_manager.c"
    "wifi_provisioning.c"
)

# Optional features (main/Kconfig): a feature that is off contributes no source.
if(CONFIG_FORK_HTTP_FETCH)
    list(APPEND SOURCES "http_fetch.c")
endif()
if(CONFIG_FORK_EXIF)
    list(APPEND SOURCES "exif_reader.c")
endif()
if(CONFIG_FEATURE_TELEGRAM)
    list(APPEND SOURCES "telegram_bot.c")
endif()
if(CONFIG_FEATURE_OVERLAYS)
    list(APPEND SOURCES "overlay_manager.c" "headlines.c")
endif()
if(CONFIG_FORK_WEATHER)
    list(APPEND SOURCES "weather.c")
endif()
if(CONFIG_FORK_CLIMATE_CORE)
    list(APPEND SOURCES "climate.c")
endif()
if(CONFIG_FEATURE_AGENDA)
    list(APPEND SOURCES "agenda_color_profile.c" "agenda_manager.c" "agenda_renderer.c"
                        "calendar_ics.c" "todo.c")
endif()
if(CONFIG_FEATURE_CHIMES)
    list(APPEND SOURCES "chime.c")
endif()
if(CONFIG_FEATURE_CLIMATE)
    list(APPEND SOURCES "climate_history.c")
endif()
if(CONFIG_FEATURE_ALARMCLOCK)
    list(APPEND SOURCES "alarm_manager.c" "alarm_pattern.c" "alarm_setting_ui.c")
endif()
if(CONFIG_FEATURE_VOICE_STOP)
    list(APPEND SOURCES "kws.c" "kws_service.c" "mic_detect.c" "mic_level.c" "mic_monitor.c")
endif()
if(CONFIG_FEATURE_BATTERY_HISTORY)
    list(APPEND SOURCES "battery_history.c")
endif()
if(CONFIG_FEATURE_DISPLAY_HISTORY)
    list(APPEND SOURCES "history_manager.c")
endif()
if(CONFIG_FEATURE_HTTPS)
    list(APPEND SOURCES "https_cert.c")
endif()
if(CONFIG_FEATURE_SOURCE_AUTH)
    list(APPEND SOURCES "source_auth.c")
endif()
if(CONFIG_FEATURE_CALDAV)
    list(APPEND SOURCES "caldav.c")
endif()
if(CONFIG_FEATURE_UPLOAD_DEDUP)
    list(APPEND SOURCES "dedup.c" "dedup_payload.c" "dedup_service.c")
endif()
if(CONFIG_FEATURE_FACECROP)
    list(APPEND SOURCES "facecrop_metadata.c")
endif()

set(INCLUDE_DIRS ".")

set(PRIV_REQUIRES
    app_update
    board_hal
    espressif__qrcode
    epaper
    epaper_src
    esp_app_format
    esp_driver_gpio
    esp_event
    esp_http_client
    esp_http_server
    esp_https_ota
    esp-tls
    esp_netif
    esp_timer
    esp_wifi
    espressif__cjson
    espressif__esp_jpeg
    espressif__libpng
    espressif__zlib
    espressif__mdns
    fatfs
    mbedtls
    memfs
    nvs_flash
    sdcard
    espcoredump
)

set(MAIN_EMBED_FILES
    "webapp/index.html.gz"
    "webapp/icon.svg.gz"
    "webapp/assets/index.css.gz"
    "webapp/assets/index.js.gz"
    "webapp/assets/index2.js.gz"
    "webapp/assets/exif-reader.js.gz"
    "webapp/assets/browser.js.gz"
    "webapp/assets/__vite-browser-external.js.gz"
    "resources/measurement_sample.jpg"
)
# The Agenda colour-profile editor page (built into the web app only with the feature)
# and the default profile.
if(CONFIG_FEATURE_AGENDA)
    list(APPEND MAIN_EMBED_FILES "webapp/profile-editor.html.gz"
                                 "resources/default_agenda_color_profile.json")
endif()

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES ""
    PRIV_REQUIRES ${PRIV_REQUIRES} 
    EMBED_FILES ${MAIN_EMBED_FILES}
)

# Embed splash screen EPDGZ files (generated at build time by generate_splash.py)
target_add_binary_data(${COMPONENT_LIB} "splash_data/splash.epdgz" BINARY)
target_add_binary_data(${COMPONENT_LIB} "splash_data/setup_complete.epdgz" BINARY)

# The HTTPS server is only needed by the HTTPS feature. Kconfig values are not visible
# during IDF's early requirement expansion, so it is added after registration.
if(CONFIG_FEATURE_HTTPS)
    idf_component_optional_requires(PRIVATE esp_https_server)
endif()
# The duplicate index hashes with the ROM's MD5.
if(CONFIG_FEATURE_UPLOAD_DEDUP)
    idf_component_optional_requires(PRIVATE esp_rom)
endif()
