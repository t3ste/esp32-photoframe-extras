#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>

#include "esp_err.h"
#include "feature_config.h"

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_UPDATE_AVAILABLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_INSTALLING,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR
} ota_state_t;

typedef struct {
    ota_state_t state;
    char current_version[32];
    char latest_version[32];
    char error_message[128];
    int progress_percent;
#if FEATURE_OTA_CHANNEL
    bool latest_prerelease;  // the release found by the last check is a pre-release
#endif
} ota_status_t;

#if FEATURE_OTA_CHANNEL
typedef enum {
    OTA_CHANNEL_STABLE = 0,      // GitHub's "latest" release (never a pre-release)
    OTA_CHANNEL_PRERELEASE = 1,  // the newest release of any kind, pre-releases included
} ota_channel_t;

typedef struct {
    ota_channel_t channel;
} ota_options_t;

#endif
esp_err_t ota_manager_init(void);
esp_err_t ota_check_for_update(bool *update_available, int timeout);
esp_err_t ota_start_update(void);
#if FEATURE_OTA_CHANNEL

void ota_get_options(ota_options_t *out);

/**
 * Persists which release channel the next check/update uses. Forgets the
 * result of the previous check (it was for another channel).
 * ESP_ERR_INVALID_STATE while a check/update runs.
 */
esp_err_t ota_set_options(ota_channel_t channel);
#endif
void ota_get_status(ota_status_t *status);
const char *ota_get_current_version(void);
bool ota_should_check_daily(void);
void ota_update_last_check_time(void);

#endif  // OTA_MANAGER_H
