#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_https_ota.h"

#include "quarklink.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static const char *TAG = "wifi station";
static const char *IEF_TAG = "IEF";
static const char *QL_TAG = "QL";

quarklink_context_t quarklink;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(IEF_TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(IEF_TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(IEF_TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(IEF_TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(IEF_TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(IEF_TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(IEF_TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(IEF_TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(IEF_TAG, "OTA abort");
                break;
        }
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config;

    /* Load existing configuration and prompt user */
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 wifi_config.sta.ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 wifi_config.sta.ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void error_loop(const char *message) {
    if (message != NULL) {
        ESP_LOGE(QL_TAG, "%s", message);
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void getting_started_task(void *pvParameter) {

    quarklink_return_t ql_ret;
    quarklink_return_t ql_status = QUARKLINK_ERROR;
    while (1) {
        /* get status */
        ESP_LOGI(QL_TAG, "Get status");
        ql_status = quarklink_status(&quarklink);
        switch (ql_status) {
            case QUARKLINK_STATUS_ENROLLED:
                ESP_LOGI(QL_TAG, "Enrolled");
                break;
            case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
                ESP_LOGI(QL_TAG, "Firmware Update required");
                break;
            case QUARKLINK_STATUS_NOT_ENROLLED:
                ESP_LOGI(QL_TAG, "Not enrolled");
                break;
            case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
                ESP_LOGI(QL_TAG, "Certificate expired");
                break;
            case QUARKLINK_STATUS_REVOKED:
                ESP_LOGI(QL_TAG, "Device revoked");
                break;
            default:
                error_loop("Error during status request");
        }

        if (ql_status == QUARKLINK_STATUS_NOT_ENROLLED ||
            ql_status == QUARKLINK_STATUS_CERTIFICATE_EXPIRED ||
            ql_status == QUARKLINK_STATUS_REVOKED) {
            /* enroll */
            ESP_LOGI(QL_TAG, "Enrol to %s", quarklink.endpoint);
            ql_ret = quarklink_enrol(&quarklink);
            switch (ql_ret) {
                case QUARKLINK_SUCCESS:
                    ESP_LOGI(QL_TAG, "Successfully enrolled!");
                    break;
                case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                    ESP_LOGW(QL_TAG, "Device does not exist");
                    break;
                case QUARKLINK_DEVICE_REVOKED:
                    ESP_LOGW(QL_TAG, "Device revoked");
                    break;
                case QUARKLINK_CACERTS_ERROR:
                default:
                    error_loop("Error during enrol");
            }
        }
   
        if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
            /* firmware update */
            ESP_LOGI(QL_TAG, "Get firmware update");
            ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
            switch (ql_ret) {
                case QUARKLINK_FWUPDATE_UPDATED:
                    ESP_LOGI(QL_TAG, "Firmware updated. Rebooting...");
                    esp_restart();
                    break;
                case QUARKLINK_FWUPDATE_NO_UPDATE:
                    ESP_LOGI(QL_TAG, "No firmware update");
                    break;
                case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Wrong firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                    ESP_LOGI(QL_TAG, "Missing required firmware signature");
                    break;
                case QUARKLINK_FWUPDATE_ERROR:
                default:
                    error_loop("error while updating firmware");
            }
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

}

void app_main(void)
{

    printf("\n quarklink-getting_started esp32-m5edukit-ecc608 0.1.1\n");

    /* quarklink init */
    ESP_LOGI(QL_TAG, "Initialising QuarkLink");
    quarklink_return_t ql_ret = quarklink_init(&quarklink, "", 6000, "");
    // TODO calling quarklink init will initialise the key at the first boot. init will be called again to update URL, PORT, ROOTCA
    ql_ret = quarklink_loadStoredContext(&quarklink);
    if (ql_ret != QUARKLINK_SUCCESS) {
        printf("ql_ret %d\n", ql_ret);
    }
    ESP_LOGI(QL_TAG, "Device ID: %s", quarklink.deviceID);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    xTaskCreate(&getting_started_task, "getting_started_task", 1024 * 8, NULL, 5, NULL);
}
