#pragma once

// Copy this file to ota_config.local.h, then replace every value.
// ota_config.local.h is ignored by Git and must never be committed.
// The Wi-Fi values are optional migration defaults; phone provisioning stores
// credentials in ESP32 NVS and is preferred for new installations.
#define FRIDGE_WIFI_SSID "your-wifi-name"
#define FRIDGE_WIFI_PASSWORD "your-wifi-password"
#define FRIDGE_OTA_HOSTNAME "fridge-controller"
#define FRIDGE_OTA_PASSWORD "replace-with-a-strong-ota-password"
