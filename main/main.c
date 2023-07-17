#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_https_ota.h"

#include "quarklink.h"

#include "mqtt_client.h"
#include "esp_ota_ops.h"


/* FreeRTOS event group to signal when we are connected */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static int count = 0;
char data[10] = "";

static const char *TAG = "quarklink-getting-started";
quarklink_context_t quarklink;

/* Intervals*/
// How often to check for status, in s
static const int STATUS_CHECK_INTERVAL = 20;
// mqtt publish interval in s
static const int MQTT_PUBLISH_INTERVAL = 5;

/* MQTT config */
#define MAX_TOPIC_LENGTH    (QUARKLINK_MAX_DEVICE_ID_LENGTH + 30)

/*Variable to track if the MQTT Task is running*/
static bool is_running = false;


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // TODO Subscribe to something as an example
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/* Build the topic accordingly to the IoT Hub used */
void getMqttTopic(quarklink_context_t *quarklink, char *topic) {
    // If Broker is AWS
    if (strstr(quarklink->iotHubEndpoint, "amazon") != 0) {
        ESP_LOGI(TAG, "Broker is AWS");
        sprintf(topic, "aws/topic/%s", quarklink->deviceID);
    }
    // If Broker is QuarkLink MQTT
    else {
        ESP_LOGI(TAG, "Broker is QuarkLink MQTT");
        sprintf(topic, "local/topic/%s", quarklink->deviceID);
    }
}

/**
 * \brief Initialise the MQTT task using to the QuarkLink details provided. 
 * 
 * \param[in]  quarklink The quarklink context
 * \param[out] client    The handle of the mqtt client created
 * \return int 0 for success
 */
int mqtt_init(quarklink_context_t *quarklink, esp_mqtt_client_handle_t *client) {
    if (is_running) {
        return 0;
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.hostname = quarklink->iotHubEndpoint,
            .address.port = quarklink->iotHubPort,
            .address.transport = MQTT_TRANSPORT_OVER_SSL,
            .verification.certificate = quarklink->iotHubRootCert
        },
        .credentials ={
            .client_id = quarklink->deviceID,
            .authentication = {
                .use_secure_element = true,
                .certificate= quarklink->deviceCert
            }
        }
    };

    *client = esp_mqtt_client_init(&mqtt_cfg);
    if (*client == NULL) {
        return -1;
    }
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(*client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (esp_mqtt_client_start(*client) == ESP_OK) {
        is_running = true;
        return 0;
    }
    else {
        is_running = false;
        return -1;
    }
}

void wifi_init_sta(void) {
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
        ESP_LOGI(TAG, "connected to ap SSID: %s", wifi_config.sta.ssid);
    } 
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s", wifi_config.sta.ssid);
    } 
    else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void getting_started_task(void *pvParameter) {

    quarklink_return_t ql_ret;
    quarklink_return_t ql_status = QUARKLINK_ERROR;

    esp_mqtt_client_handle_t mqtt_client = NULL;
    char mqtt_topic[MAX_TOPIC_LENGTH] = "";
    uint32_t round = 0;

    while (1) {

        // If it's time for a status check
        if (round % STATUS_CHECK_INTERVAL == 0) {
            /* get status */
            ESP_LOGI(TAG, "Get status");
            ql_status = quarklink_status(&quarklink);
            switch (ql_status) {
                case QUARKLINK_STATUS_ENROLLED:
                    ESP_LOGI(TAG, "Enrolled");
                    break;
                case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
                    ESP_LOGI(TAG, "Firmware Update required");
                    break;
                case QUARKLINK_STATUS_NOT_ENROLLED:
                    ESP_LOGI(TAG, "Not enrolled");
                    break;
                case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
                    ESP_LOGI(TAG, "Certificate expired");
                    break;
                case QUARKLINK_STATUS_REVOKED:
                    ESP_LOGI(TAG, "Device revoked");
                    break;
                default:
                    ESP_LOGE(TAG, "Error during status request");
                    continue;
            }

            if (ql_status == QUARKLINK_STATUS_NOT_ENROLLED ||
                ql_status == QUARKLINK_STATUS_CERTIFICATE_EXPIRED ||
                ql_status == QUARKLINK_STATUS_REVOKED) {
                /* Reset mqtt */
                strcpy(mqtt_topic, "");
                esp_mqtt_client_stop(mqtt_client);
                is_running = false;
                /* enroll */
                ESP_LOGI(TAG, "Enrol to %s", quarklink.endpoint);
                ql_ret = quarklink_enrol(&quarklink);
                switch (ql_ret) {
                    case QUARKLINK_SUCCESS:
                        ESP_LOGI(TAG, "Successfully enrolled");
                        ql_ret = quarklink_persistEnrolmentContext(&quarklink);
                        if (ql_ret != QUARKLINK_SUCCESS) {
                            ESP_LOGW(TAG, "Failed to store the enrolment context");
                        }
                        /* Update Status to avoid delaying MQTT Client init */
                        ql_status = QUARKLINK_STATUS_ENROLLED;
                        break;
                    case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                        ESP_LOGW(TAG, "Device does not exist");
                        break;
                    case QUARKLINK_DEVICE_REVOKED:
                        ESP_LOGW(TAG, "Device revoked");
                        break;
                    case QUARKLINK_CACERTS_ERROR:
                    default:
                        ESP_LOGE(TAG, "Error during enrol");
                        continue;
                }
            }
            
            if (ql_status == QUARKLINK_STATUS_FWUPDATE_REQUIRED) {
                /* firmware update */
                ESP_LOGI(TAG, "Get firmware update");
                ql_ret = quarklink_firmwareUpdate(&quarklink, NULL);
                switch (ql_ret) {
                    case QUARKLINK_FWUPDATE_UPDATED:
                        ESP_LOGI(TAG, "Firmware updated. Rebooting...");
                        esp_restart();
                        break;
                    case QUARKLINK_FWUPDATE_NO_UPDATE:
                        ESP_LOGI(TAG, "No firmware update");
                        break;
                    case QUARKLINK_FWUPDATE_WRONG_SIGNATURE:
                        ESP_LOGI(TAG, "Wrong firmware signature");
                        break;
                    case QUARKLINK_FWUPDATE_MISSING_SIGNATURE:
                        ESP_LOGI(TAG, "Missing required firmware signature");
                        break;
                    case QUARKLINK_FWUPDATE_ERROR:
                    default:
                        ESP_LOGE(TAG, "Error while updating firmware");
                        continue;
                }
            }
            
            if (ql_status == QUARKLINK_STATUS_ENROLLED) {
                /* Start the MQTT task */
                if (mqtt_init(&quarklink, &mqtt_client) != 0) {
                    ESP_LOGE(TAG, "Failed to initialise the MQTT Client");
                    continue;
                }
            }
        }

        // If it's time to publish
        if ((round % MQTT_PUBLISH_INTERVAL == 0) && (ql_status == QUARKLINK_STATUS_ENROLLED)) {
            if (strcmp(mqtt_topic, "") == 0) {
                getMqttTopic(&quarklink, mqtt_topic);
            }
            // len = 0 and data not NULL is valid, length is determined by strlen
            sprintf(data, "%d", count++);
            int msg_id = esp_mqtt_client_publish(mqtt_client, mqtt_topic, data, 0, 0, 0);
            if (msg_id < 0) {
                ESP_LOGE(TAG, "Failed to publish to %s (ret %d)", mqtt_topic, msg_id);
            }
            else {
                ESP_LOGI(TAG, "Published data=%s, to %s", data, mqtt_topic);
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        round++;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "quarklink-getting_started-m5edukit-ecc608");

    /* quarklink init */
    ESP_LOGI(TAG, "Loading stored QuarkLink context");
    // Need to initialise a local quarklink_context_t in order to retrieve the stored one. Doesn't matter what values it is given.
    quarklink_return_t ql_ret = quarklink_init(&quarklink, "", 1, "");
    ql_ret = quarklink_loadStoredContext(&quarklink);
    if (ql_ret == QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED) {
        // Should get here the first time after provisioning as the device hasn't enrolled yet
        ESP_LOGI(TAG, "No QuarkLink enrolment info stored");
    }
    else if (ql_ret != QUARKLINK_SUCCESS) {
        // Any return other than QUARKLINK_SUCCESS or QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED is to be considered an error 
        ESP_LOGE(TAG, "Failed to load stored QuarkLink context (%d)", ql_ret);
        // should not happen, restart and retry
        esp_restart();
        // TODO provide some backup default values from Kconfig?
        // if (ql_ret == QUARKLINK_CONTEXT_NO_CREDENTIALS_STORED || ql_ret == QUARKLINK_CONTEXT_NOTHING_STORED) {
        //     ESP_LOGE(TAG, "No QuarkLink credentials stored, using default (%s)", QUARKLINK_DEFAULT_ENDPOINT);
        //     strcpy(quarklink.endpoint, QUARKLINK_DEFAULT_ENDPOINT);
        //     strcpy(quarklink.rootCert, QUARKLINK_DEFAULT_ROOTCA);
        //     quarklink.port = 6000;
        // }
    }

    ESP_LOGI(TAG, "Successfully loaded QuarkLink details for: %s", quarklink.endpoint);
    ESP_LOGI(TAG, "Device ID: %s", quarklink.deviceID);

    wifi_init_sta();

    xTaskCreate(&getting_started_task, "getting_started_task", 1024 * 8, NULL, 5, NULL);
}
