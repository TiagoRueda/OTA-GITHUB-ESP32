#ifndef SIMPLE_OTA_EXAMPLE_H
#define SIMPLE_OTA_EXAMPLE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "protocol_examples_common.h"
#include "string.h"
#include "cJSON.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <sys/socket.h>

static const char *TAG = "simple_ota";
const char *firmware_timestamp = __DATE__ " " __TIME__;
#define OTA_URL_SIZE 256
#define VERSION_APP 1.0
#define HASH_LEN 32
#define UPDATE_JSON_URL	"https://github.com/TiagoRueda/OTA-GITHUB-ESP32/releases/download/teste/firmware.json"

char buffer_rx[200];
bool json_sucess = true;

#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
/* The interface name value can refer to if_desc in esp_netif_defaults.h */
#if CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_ETH;
#elif CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_STA
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;
#endif
#endif

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#endif
