#include "simple_ota_example.h"

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client) && json_sucess) {
			strncpy(buffer_rx, (char*)evt->data, evt->data_len);}
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

void ota_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting OTA task");

    esp_http_client_config_t config = {
        .url = UPDATE_JSON_URL,  // CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
        .host = "github.com",
        .port = 443,             // GitHub uses port 443 for HTTPS
        .username = "YourID",
        .password = "YourPSSWD",
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size = 1024,                        // Receive buffer size
        .buffer_size_tx = 1024,                     // Transmission buffer size
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        cJSON *file_json = cJSON_Parse(buffer_rx);
        if (file_json == NULL)
            ESP_LOGI(TAG, "The file retrieved is not a valid JSON format, consequently, the process is being aborted");
        else {
            cJSON *version = cJSON_GetObjectItemCaseSensitive(file_json, "version");
            cJSON *archive = cJSON_GetObjectItemCaseSensitive(file_json, "file");

            if (!cJSON_IsNumber(version))
            	ESP_LOGI(TAG, "Unable to parse new version");
            else {
                double received_version = version->valuedouble;
                if (received_version > VERSION_APP) {
                    json_sucess = false;
                    ESP_LOGI(TAG, "The current firmware version (%.1f) is outdated", VERSION_APP);

                    if (cJSON_IsString(archive) && (archive->valuestring != NULL)) {
                    	ESP_LOGI(TAG, "Downloading new firmware: (%s)\n", archive->valuestring);

                        esp_http_client_config_t ota_client_config = {
                            .url = archive->valuestring,
                            .host = "github.com",
                            .port = 443,
                            .username = "YourID",
                            .password = "YourPSSWD",
                            .auth_type = HTTP_AUTH_TYPE_BASIC,
                            .method = HTTP_METHOD_GET,
                            .timeout_ms = 10000,
                            .transport_type = HTTP_TRANSPORT_OVER_SSL,
                            .buffer_size = 4096,
                            .buffer_size_tx = 4096,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
                            .crt_bundle_attach = esp_crt_bundle_attach,
#else
                            .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
                            .event_handler = _http_event_handler,
                            .keep_alive_enable = true,
                        };

                        esp_https_ota_config_t ota_config = {
                            .http_config = &ota_client_config,
                        };

                        esp_err_t ret = esp_https_ota(&ota_config);

                        if (ret == ESP_OK) {
                        	ESP_LOGI(TAG, "Download sucess, restarting");
                            esp_restart();
                        } else {
                        	ESP_LOGI(TAG, "Download failed\n");
                        }
                    } else
                    	ESP_LOGI(TAG, "Error name archive");
                } else
                	ESP_LOGI(TAG, "The version is the same or higher");
            }
        }
    } else
    	ESP_LOGI(TAG, "Download failed json file");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    get_sha256_of_partitions();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_CONNECT_WIFI
    /* Be sure to disable any WiFi power-saving modes, this allows for better throughput*/
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

    xTaskCreate(&ota_task, "task_ota", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Rev Teste Github %.1f", VERSION_APP);
    ESP_LOGI(TAG, "Timestamp do Firmware: %s", firmware_timestamp);
}
