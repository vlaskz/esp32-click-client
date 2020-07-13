/*

void app_main()
{
    hd44780_init(&lcd);
    wifi_init();

    
}
*/
/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "hd44780.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID "Vlaskz"
#define EXAMPLE_ESP_WIFI_PASS "taytaytay"
#define EXAMPLE_ESP_MAXIMUM_RETRY 5

static char IPADDR[16];
static char GWADDR[16];
static char MKADDR[16];

hd44780_t lcd = {
    .write_cb = NULL,
    .font = HD44780_FONT_5X8,
    .lines = 2,
    .pins = {
        .rs = GPIO_NUM_27,
        .e = GPIO_NUM_26,
        .d4 = GPIO_NUM_25, //NEVER EVER USE PIN-24, for God's sake.
        .d5 = GPIO_NUM_23,
        .d6 = GPIO_NUM_22,
        .d7 = GPIO_NUM_21,
        .bl = HD44780_NOT_USED}};

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        /*
            in the next lines we got some important info about WiFi
            connection, such as IP, Gateway, Mask and SSID;

        */
        sprintf(IPADDR, IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(GWADDR, IPSTR, IP2STR(&event->ip_info.gw));
        sprintf(MKADDR, IPSTR, IP2STR(&event->ip_info.netmask));

        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
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

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

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
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
}

void show_wifi_info()
{
    int16_t time_interval = 2000;
    for (;;)
    {
        hd44780_clear(&lcd);
        hd44780_gotoxy(&lcd, 0, 0);
        hd44780_puts(&lcd, "IP ADDRESS:");
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, IPADDR);
        vTaskDelay(time_interval / portTICK_PERIOD_MS);
        hd44780_clear(&lcd);
        hd44780_gotoxy(&lcd, 0, 0);
        hd44780_puts(&lcd, "GATEWAY ADDRESS:");
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, GWADDR);
        vTaskDelay(time_interval / portTICK_PERIOD_MS);
        hd44780_clear(&lcd);
        hd44780_gotoxy(&lcd, 0, 0);
        hd44780_puts(&lcd, "NETWORK MASK:");
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, MKADDR);
        vTaskDelay(time_interval / portTICK_PERIOD_MS);
        hd44780_clear(&lcd);
        hd44780_gotoxy(&lcd, 0, 0);
        hd44780_puts(&lcd, "ACCESS POINT:");
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, EXAMPLE_ESP_WIFI_SSID);
        vTaskDelay(time_interval / portTICK_PERIOD_MS);
    }
}

void nvs_init()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    nvs_init(); //always call this first, if you want to use the non-volatile storage (ROM);

    wifi_init_sta(); //starts the uC as a station;

    hd44780_init(&lcd); //initializes the display;

    xTaskCreate(show_wifi_info, "show_wifi_info", configMINIMAL_STACK_SIZE * 5, NULL, 8, NULL); //task to show some info regarding wifi connection.
}
