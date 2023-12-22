#include <freertos/FreeRTOS.h>
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "led_strip.h"

#include "quarklink.h"
#include "quarklink_extras.h"
#include "rsa_sign_alt.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define LED_STRIP_BLINK_GPIO  48 // GPIO assignment esp32-s3
#else
#define LED_STRIP_BLINK_GPIO  8 // GPIO assignment esp32-c3
#endif
#define LED_STRIP_LED_NUMBERS 1 // LED numbers in the strip
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

#ifndef LED_COLOUR
#define LED_COLOUR  0
#endif

#define RED     1
#define GREEN   2
#define BLUE    3

/* FreeRTOS event group to signal when we are connected */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static int count = 0;

static const char *TAG = "quarklink-getting-started";
quarklink_context_t quarklink;

/* Intervals */
// How often to check for status, in s
static const int STATUS_CHECK_INTERVAL = 20;
// mqtt publish interval in s
static const int MQTT_PUBLISH_INTERVAL = 5;

/* MQTT config */
#define MAX_TOPIC_LENGTH    (QUARKLINK_MAX_DEVICE_ID_LENGTH + 30)
#define MAX_MESSAGE_LENGTH  30
char mqtt_topic[MAX_TOPIC_LENGTH] = "";

/* Variable to track if the MQTT Task is running */
static bool is_running = false;

#if (LED_COLOUR)
// LED Strip object handle
led_strip_handle_t led_strip;

void led_set_colour(led_strip_handle_t strip, int colour);
#endif

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Connection to the AP failed");
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, 10);
        }
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        // ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
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
        ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "topic/#", 0);
        ESP_LOGD(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        ESP_LOGD(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGD(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGD(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGD(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGD(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else {
            ESP_LOGD(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

bool isAzure(quarklink_context_t *quarklink) {
    return ((strstr(quarklink->iotHubEndpoint, "azure") != 0)  && (strlen(quarklink->scopeID) == 0));
}

bool isAzureCentral(quarklink_context_t *quarklink) {
    return ((strstr(quarklink->iotHubEndpoint, "azure") != 0)  && (strlen(quarklink->scopeID) != 0));
}

/**
 * \brief Initialise the MQTT task using to the QuarkLink details provided. 
 *
 * \param[in]  quarklink The quarklink context
 * \param[out] client    The handle of the mqtt client created
 * \return int 0 for success
 */
int mqtt_init(quarklink_context_t *quarklink, esp_mqtt_client_handle_t* client) {
    if (is_running) {
        return 0;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.hostname = quarklink->iotHubEndpoint,
            .address.port = quarklink->iotHubPort,
            .address.transport = MQTT_TRANSPORT_OVER_SSL,
            .verification.certificate = quarklink->iotHubRootCert
        },
        .credentials = {
            .client_id = quarklink->deviceID,
            .authentication = {
                .certificate = quarklink->deviceCert,
            }
        }
    };

    /* Using Digital Signature module */
    static esp_ds_data_ctx_t ds_data;
    quarklink_esp32_getDSData(&ds_data);
    mqtt_cfg.credentials.authentication.ds_data = &ds_data;

    if (isAzure(quarklink) || isAzureCentral(quarklink)) {
        char userName[256] = "";
        sprintf(userName, "%s/%s/?api-version=2018-06-30", quarklink->iotHubEndpoint, quarklink->deviceID);
        mqtt_cfg.credentials.username = userName;
        mqtt_cfg.session.last_will.topic = "";
        mqtt_cfg.session.last_will.msg = "";
        mqtt_cfg.session.last_will.qos = 0;
        mqtt_cfg.session.last_will.retain = false;
        mqtt_cfg.session.keepalive = 10;
        sprintf(mqtt_topic, "devices/%s/messages/events/", quarklink->deviceID);
    }

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
    ESP_LOGD(TAG, "wifi_init_sta finished.");

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
        ESP_LOGI(TAG, "Reached maximum retry limit for connection to the AP");
        ESP_LOGI(TAG, "Restarting");    
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        esp_restart();
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
    char message[MAX_MESSAGE_LENGTH] = "";
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
                    if (strcmp(quarklink.iotHubEndpoint, "") == 0) {
                        ESP_LOGI(TAG, "No enrolment info saved. Re-enrolling");
                        ql_status = QUARKLINK_STATUS_NOT_ENROLLED;
                    }
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
                    #if (LED_COLOUR)
                    led_set_colour(led_strip, RED);
                    #endif
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
                        ESP_LOGI(TAG, "Successfully enrolled!");
                        ql_ret = quarklink_persistEnrolmentContext(&quarklink);
                        if (ql_ret != QUARKLINK_SUCCESS) {
                            ESP_LOGW(TAG, "Failed to store the Enrolment context");
                        }
                        #if (LED_COLOUR)
                        led_set_colour(led_strip, LED_COLOUR);
                        #endif
                        /* Update Status to avoid delaying MQTT Client init */
                        ql_status = QUARKLINK_STATUS_ENROLLED;
                        break;
                    case QUARKLINK_DEVICE_DOES_NOT_EXIST:
                        ESP_LOGW(TAG, "Device does not exist");
                        break;
                    case QUARKLINK_DEVICE_REVOKED:
                        #if (LED_COLOUR)
                        led_set_colour(led_strip, RED);
                        #endif
                        ESP_LOGW(TAG, "Device revoked");
                        break;
                    case QUARKLINK_CACERTS_ERROR:
                    default:
                        ESP_LOGE(TAG, "Error during enrol");
                        break;
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
                        break;
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
                sprintf(mqtt_topic, "topic/%s", quarklink.deviceID);
            }
            sprintf(message, "{\"count\":%d}", count);
            int msg_id = esp_mqtt_client_publish(mqtt_client, mqtt_topic, message, 0, 0, 0);
            if (msg_id < 0) {
                ESP_LOGE(TAG, "Failed to publish to %s (ret %d)", mqtt_topic, msg_id);
            }
            else {
                ESP_LOGI(TAG, "Published data=%d to %s", count, mqtt_topic);
                #if (LED_COLOUR)
                led_strip_clear(led_strip);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                led_set_colour(led_strip, LED_COLOUR);
                #endif
            }
            count++;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        round++;
    }
}

#if (LED_COLOUR)
void led_set_colour(led_strip_handle_t strip, int colour) {
    if (colour == RED)
        led_strip_set_pixel(led_strip, 0, 10, 0, 0);
    else if (colour == GREEN)
        led_strip_set_pixel(led_strip, 0, 0, 10, 0);
    else if (colour == BLUE)
        led_strip_set_pixel(led_strip, 0, 0, 0, 10);
    else
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    led_strip_refresh(led_strip);
}

void set_led(void) {

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
}
#endif

void app_main(void) {
    ESP_LOGI(TAG, "quarklink-getting-started-esp32");
    
    #if (LED_COLOUR)
        set_led(); // esp32-c3 and esp32-s3 RGB LED
        led_set_colour(led_strip, LED_COLOUR); // LED_RED or LED_GREEN or LED_BLUE
    #endif

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
