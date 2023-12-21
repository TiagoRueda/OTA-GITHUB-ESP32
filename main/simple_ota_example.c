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
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <sys/socket.h>
#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

const char *firmware_timestamp = __DATE__ " " __TIME__;
#define HASH_LEN 32

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
/* The interface name value can refer to if_desc in esp_netif_defaults.h */
#if CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_ETH;
#elif CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_STA
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;
#endif
#endif

static const char *TAG = "simple_ota";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");
#define OTA_URL_SIZE 256
#define VERSION_APP 1.0
#define UPDATE_JSON_URL	"https://github.com/TiagoRueda/OTA-GITHUB-ESP32/releases/download/teste/firmware.json"

// receive buffer
char rcv_buffer[200];

// Função para obter a versão da partição atual
uint8_t *get_firmwarehash() {
    esp_app_desc_t app_desc;
    esp_ota_get_partition_description(esp_ota_get_running_partition(), &app_desc);

    uint8_t *actual_hash = malloc(sizeof(app_desc.app_elf_sha256));

    if (actual_hash == NULL) {
        fprintf(stderr, "Falha ao alocar memoria\n");
    }

    memcpy(actual_hash, app_desc.app_elf_sha256, sizeof(app_desc.app_elf_sha256));

    return actual_hash;
}

// Função para obter a versão da nova partição após a atualização
uint8_t *get_newfirmwarehash() {
    esp_app_desc_t new_app_desc;
    esp_ota_get_partition_description(esp_ota_get_next_update_partition(NULL), &new_app_desc);

    uint8_t *new_hash = malloc(sizeof(new_app_desc.app_elf_sha256));

    if (new_hash == NULL) {
        fprintf(stderr, "Falha ao alocar memoria\n");
    }

    memcpy(new_hash, new_app_desc.app_elf_sha256, sizeof(new_app_desc.app_elf_sha256));

    return new_hash;
}

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
        if (!esp_http_client_is_chunked_response(evt->client)) {
			strncpy(rcv_buffer, (char*)evt->data, evt->data_len);}
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA task");
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
    esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Nao e possivel encontrar o netif na descricao da interface");
        abort();
    }
    struct ifreq ifr;
    esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
    ESP_LOGI(TAG, "O nome da interface de vinculacao e %s", ifr.ifr_name);
#endif
    esp_http_client_config_t config = {
        .url = UPDATE_JSON_URL,//CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
	    .host = "github.com",
	    .port = 443,  // GitHub utiliza a porta 443 para HTTPS
	    .username = "??",
	    .password = "??",
	    .auth_type = HTTP_AUTH_TYPE_BASIC,  // Usar a autenticação básica para o GitHub
	    .method = HTTP_METHOD_GET,  // Método HTTP GET para baixar o arquivo
	    .timeout_ms = 20000,
	    .transport_type = HTTP_TRANSPORT_OVER_SSL,  // Usar o transporte SSL para conexão segura
	    .buffer_size = 1024,  // Tamanho do buffer de recebimento
	    .buffer_size_tx = 1024,  // Tamanho do buffer de transmissão
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
        .if_name = &ifr,
#endif
    };

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG, "Incompatibilidade de configuracao: URL de imagem de atualizacao de firmware incorreto");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    //esp_https_ota_config_t ota_config = {
    //    .http_config = &config,
    //};

    //ESP_LOGI(TAG, "Tentando baixar a atualizacao: %s", config.url);

    //esp_err_t ret = esp_https_ota(&ota_config);
    // downloading the json file
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    // downloading the json file
    		if(err == ESP_OK) {

    			// parse the json file
    			cJSON *json = cJSON_Parse(rcv_buffer);
    			if(json == NULL) printf("downloaded file is not a valid json, aborting...\n");
    			else {
    				cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    				cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");
    				//char *file = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL;

    				// check the version
    				if(!cJSON_IsNumber(version)) printf("unable to read new version, aborting...\n");
    				else {

    					double new_version = version->valuedouble;
    					if(new_version > VERSION_APP) {

    						printf("current firmware version (%.1f) is lower than the available one (%.1f), upgrading...\n", VERSION_APP, new_version);
    						if(cJSON_IsString(file) && (file->valuestring != NULL)) {
    							printf("downloading and installing new firmware (%s)...\n", file->valuestring);

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
    esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Nao e possivel encontrar o netif na descricao da interface");
        abort();
    }
    struct ifreq ifr;
    esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
    ESP_LOGI(TAG, "O nome da interface de vinculacao e %s", ifr.ifr_name);
#endif
    esp_http_client_config_t ota_client_config = {
        .url = file->valuestring,//CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
	    .host = "github.com",
	    .port = 443,  // GitHub utiliza a porta 443 para HTTPS
	    .username = "??",
	    .password = "??",
	    .auth_type = HTTP_AUTH_TYPE_BASIC,  // Usar a autenticação básica para o GitHub
	    .method = HTTP_METHOD_GET,  // Método HTTP GET para baixar o arquivo
	    .timeout_ms = 20000,
	    .transport_type = HTTP_TRANSPORT_OVER_SSL,  // Usar o transporte SSL para conexão segura
	    .buffer_size = 4096,  // Tamanho do buffer de recebimento
	    .buffer_size_tx = 4096,  // Tamanho do buffer de transmissão
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
        .if_name = &ifr,
#endif
    };

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG, "Incompatibilidade de configuracao: URL de imagem de atualizacao de firmware incorreto");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif


    							   esp_https_ota_config_t ota_config = {
    							       .http_config = &ota_client_config,
    							    };

    							//ESP_LOGI(TAG, "Tentando baixar a atualizacao: %s", config.url);
    							esp_err_t ret = esp_https_ota(&ota_config);

    							//esp_err_t ret = esp_https_ota(&ota_client_config);

    							if (ret == ESP_OK) {
    								printf("OTA OK, restarting...\n");
    								esp_restart();
    							} else {
    								printf("OTA failed...\n");
    							}
    						}
    						else printf("unable to read the new file name, aborting...\n");
    					}
    					else printf("current firmware version (%.1f) is greater or equal to the available one (%.1f), nothing to do...\n", VERSION_APP, new_version);
    				}
    			}
    		}
    		else printf("unable to download the json file, aborting...\n");

    		printf("\n");
            vTaskDelay(30000 / portTICK_PERIOD_MS);

    /*if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Verificando atualizacao OTA");

        uint8_t *actual_hash = get_firmwarehash();
        uint8_t *new_hash = get_newfirmwarehash();

        // Log dos hashes
        ESP_LOGI(TAG, "actual_hash: ");
        for (int i = 0; i < HASH_LEN; i++) {
            ESP_LOGI(TAG, "%02x ", actual_hash[i]);
        }
        ESP_LOGI(TAG, "\n");

        ESP_LOGI(TAG, "new_hash: ");
        for (int i = 0; i < HASH_LEN; i++) {
            ESP_LOGI(TAG, "%02x ", new_hash[i]);
        }
        ESP_LOGI(TAG, "\n");

        if (memcmp(actual_hash, new_hash, HASH_LEN) == 0) {
            ESP_LOGI(TAG, "Firmware igual, nao reinicia");
        } else {
            ESP_LOGI(TAG, "Firmware diferente, reiniciando...");
            esp_restart();
        }

        // Libera a memória alocada
        free(actual_hash);
        free(new_hash);
    } else {
        ESP_LOGE(TAG, "A atualizacao do firmware falhou");
    }*/

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
    /* Certifique-se de desativar qualquer modo de economia de energia WiFi, isso permite melhor rendimento*/
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

    xTaskCreate(&ota_task, "task_ota", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Rev Teste Github %.1f", VERSION_APP);
    ESP_LOGI(TAG, "Timestamp do Firmware: %s", firmware_timestamp);

}
