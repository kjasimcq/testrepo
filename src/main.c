#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include <freertos/FreeRTOS.h>

#include "led_strip.h"
#include "quarklink.h"

#include "quarklink_extras.h"
#include "rsa_sign_alt.h"

#define LED_STRIP_BLINK_GPIO 8  // GPIO assignment
#define LED_STRIP_LED_NUMBERS 1 // LED numbers in the strip
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000) // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

#ifndef LED_COLOUR
#define LED_COLOUR 0
#endif

#define RED 1
#define GREEN 2
#define BLUE 3

/* FreeRTOS event group to signal when we are connected */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static int count = 0;

static const char *TAG = "quarklink-database-direct";
quarklink_context_t quarklink;

/* Intervals */
// How often to check for status, in s
static const int STATUS_CHECK_INTERVAL = 20;
// publish interval in s
static const int PUBLISH_INTERVAL = 5;

#if (LED_COLOUR)
// LED Strip object handle
led_strip_handle_t led_strip;

void led_set_colour(led_strip_handle_t strip, int colour);
#endif

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                          void *event_data) {
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
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGD(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } 
}

/**
 * Local function to handle http events. Mostly needed to copy the response body
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static int output_len = 0; // Stores number of bytes read
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
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }
            else {
                ESP_LOGD(TAG, "it's chunked response");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            // Insert end of string
            ((char *)evt->user_data)[output_len] = '\0';
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

bool isDatabaseDirect(quarklink_context_t *quarklink) {
    return (strcmp(quarklink->token, "") != 0);
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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config;
    /* Load existing configuration and prompt user */
    esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed
     * for the maximum number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see
     * above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which
     * event actually happened. */
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
    ESP_ERROR_CHECK(
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

int databaseDirectPost(const quarklink_context_t *quarklink, int post_count) {
    static char rxBuffer[500];

    /* Configure the https client */
    esp_http_client_config_t config = {
        .host = quarklink->iotHubEndpoint,
        .port = quarklink->iotHubPort,
        .path = quarklink->uri,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = quarklink->iotHubRootCert,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = rxBuffer,
        .keep_alive_enable = true,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "jwtTokenString", quarklink->token);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Access-Control-Request-Headers", "*");

    char body[200];
    sprintf(body,
            "{\"collection\":\"%s\",\"database\":\"%s\",\"dataSource\":\"%s\",\"document\":{\"count\":%d}}",
            quarklink->deviceID, quarklink->database, quarklink->dataSource, post_count);
    esp_http_client_set_post_field(client, body, strlen(body));
    char url[QUARKLINK_MAX_ENDPOINT_LENGTH];
    esp_http_client_get_url(client, url, sizeof(url));
    ESP_LOGD(TAG, "Sending POST request to %s", url);
    esp_err_t err;
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int64_t len = esp_http_client_get_content_length(client);
        ESP_LOGD(TAG, "HTTP POST Status = %d, content length = %" PRIu64, status, len);
        if (esp_http_client_is_complete_data_received(client)) {
            ESP_LOGD(TAG, "Received response (%d): %s", strlen(rxBuffer), rxBuffer);
        }
        if (status == HttpStatus_Unauthorized) {
            ESP_LOGI(TAG, "Token Expired");
            err = HttpStatus_Unauthorized;
        }
    }
    else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        err = QUARKLINK_ERROR;
    }
    esp_http_client_cleanup(client);
    return err;
}

void main_task(void *pvParameter) {

    quarklink_return_t ql_ret;
    quarklink_return_t ql_status = QUARKLINK_ERROR;
    uint32_t round = 0;

    while (1) {

        // If it's time for a status check
        if (round % STATUS_CHECK_INTERVAL == 0) {
            /* get status */
            ESP_LOGI(TAG, "Get status");
            ql_status = quarklink_status(&quarklink);
            switch (ql_status) {
                case QUARKLINK_STATUS_ENROLLED:
                    if (strcmp(quarklink.iotHubEndpoint, "") == 0) {
                        ESP_LOGI(TAG, "No enrolment info saved. Re-enrolling");
                        ql_status = QUARKLINK_STATUS_NOT_ENROLLED;
                    }
                    ESP_LOGI(TAG, "Enrolled");
                    break;
                case QUARKLINK_STATUS_FWUPDATE_REQUIRED:
                    ESP_LOGI(TAG, "Firmware Update required");
                    break;
                case QUARKLINK_STATUS_NOT_ENROLLED:
                    ESP_LOGI(TAG, "Not enrolled");
                    break;
                case QUARKLINK_STATUS_CERTIFICATE_EXPIRED:
                    //Ignore this case as not relevant for DBD
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
                ql_status == QUARKLINK_STATUS_REVOKED) {
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
        }

        // If it's time to publish
        if ((round % PUBLISH_INTERVAL == 0) && (ql_status == QUARKLINK_STATUS_ENROLLED)) {

            if (isDatabaseDirect(&quarklink) == 1) {
                int ret_post = 0;
                ret_post = databaseDirectPost(&quarklink, count);
                if (ret_post == 0) {
                    count++;
                    ESP_LOGI(TAG, "Published data=%d", count);
                    #if (LED_COLOUR)
                        led_strip_clear(led_strip);
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        led_set_colour(led_strip, LED_COLOUR);
                    #endif
                    round++;
                }
                else {
                    strcpy(quarklink.deviceCert, "");
                    strcpy(quarklink.token, "");
                    ESP_LOGI(TAG, "Deleting Enrolment Context");
                    quarklink_deleteEnrolmentContext(&quarklink);
                    round = 0;
                    ql_status = QUARKLINK_ERROR;
                    ESP_LOGI(TAG, "ReEnrolling");
                    ql_ret = quarklink_enrol(&quarklink);
                    if (ql_ret == QUARKLINK_SUCCESS) {
                        ESP_LOGI(TAG, "Successfully enrolled!");
                    }
                    ql_ret = quarklink_persistEnrolmentContext(&quarklink);
                    if (ql_ret != QUARKLINK_SUCCESS) {
                        ESP_LOGW(TAG, "Failed to store the Enrolment context");
                    }
                    #if (LED_COLOUR)
                        led_set_colour(led_strip, LED_COLOUR);
                    #endif
                }
            }
            else {
                ESP_LOGW(TAG, "This example is only meant to work with Database Direct policies");
                strcpy(quarklink.deviceCert, "");
                strcpy(quarklink.token, "");
                quarklink_deleteEnrolmentContext(&quarklink);
                round = 0;
                ql_status = QUARKLINK_ERROR;
            }
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
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false, // DMA feature is available on ESP target like ESP32-S3
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
}
#endif

void app_main(void) {
    ESP_LOGI(TAG, "quarklink-database-direct-esp32");

#if (LED_COLOUR)
    set_led();                             // esp32-c3 RGB LED
    led_set_colour(led_strip, LED_COLOUR); // LED_RED or LED_GREEN or LED_BLUE
#endif

    /* quarklink init */
    ESP_LOGI(TAG, "Loading stored QuarkLink context");
    // Need to initialise a local quarklink_context_t in order to retrieve the stored one. Doesn't
    // matter what values it is given.
    quarklink_return_t ql_ret = quarklink_init(&quarklink, "", 1, "");
    ql_ret = quarklink_loadStoredContext(&quarklink);
    if (ql_ret == QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED) {
        // Should get here the first time after provisioning as the device hasn't enrolled yet
        ESP_LOGI(TAG, "No QuarkLink enrolment info stored");
    } else if (ql_ret != QUARKLINK_SUCCESS) {
        // Any return other than QUARKLINK_SUCCESS or QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED is
        // to be considered an error
        ESP_LOGE(TAG, "Failed to load stored QuarkLink context (%d)", ql_ret);
        // should not happen, restart and retry
        esp_restart();
    }

    ESP_LOGI(TAG, "Successfully loaded QuarkLink details for: %s", quarklink.endpoint);
    ESP_LOGI(TAG, "Device ID: %s", quarklink.deviceID);

    wifi_init_sta();

    xTaskCreate(&main_task, "main_task", 1024 * 8, NULL, 5, NULL);
}
