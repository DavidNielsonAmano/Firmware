/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"

// Libraries

// Wi-Fi configuration
#define WIFI_SSID "Anas Saeed"
#define WIFI_PASSWORD "9016da44"

static const char *TAG = "MAIN";

// API Endpoints
#define POST_URL "https://amano-api-945679465613.us-west3.run.app/v1/deviceInterface/AMANO_D4D4DA724E00/firmware"

//Connect to WiFi

// Wi-Fi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Retrying connection to Wi-Fi...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        esp_ip4_addr_t *ip_addr = (esp_ip4_addr_t *)event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(ip_addr));
    }
}

// Connect to Wi-Fi
void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi initialized.");
}

//Send data to POST endpoint
// HTTP POST request
void http_post_task(void *pvParameters) {
    esp_http_client_config_t config = {
        .url = POST_URL,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    const char *post_data = "{\"exampleKey\":\"exampleValue\"}";
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP POST Status: %d, Content Length: %d", status_code, content_length);

        char *response_buffer = malloc(content_length + 1);
        if (response_buffer) {
            esp_http_client_read(client, response_buffer, content_length);
            response_buffer[content_length] = '\0';
            ESP_LOGI(TAG, "Response: %s", response_buffer);

            // Parse JSON response
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) {
                cJSON *binary_url = cJSON_GetObjectItem(json, "binaryURL");
                if (cJSON_IsString(binary_url)) {
                    ESP_LOGI(TAG, "binaryURL: %s", binary_url->valuestring);
                } else {
                    ESP_LOGE(TAG, "binaryURL not found in response.");
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "Failed to parse JSON response.");
            }

            free(response_buffer);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for response buffer.");
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST Request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

//Parse Data from response

//Print out binaryURL string

// Main Function
void app_main(void)
{
    ESP_LOGI(TAG, "Starting app...");

    // Initialize Wi-Fi
    wifi_init();

    // Create HTTP POST task
    xTaskCreate(&http_post_task, "http_post_task", 8192, NULL, 5, NULL);
}
