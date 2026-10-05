#include "wifi_ap.hpp"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "photo_frame_wifi.h"

static const char* TAG = "wifi_ap";

static void on_wifi_event(void*, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base != WIFI_EVENT) {
        return;
    }

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t* ev = (const wifi_event_ap_staconnected_t*)event_data;
        ESP_LOGI(TAG, "sta connected " MACSTR " aid=%d", MAC2STR(ev->mac), (int)ev->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t* ev = (const wifi_event_ap_stadisconnected_t*)event_data;
        ESP_LOGI(TAG, "sta disconnected " MACSTR " aid=%d", MAC2STR(ev->mac), (int)ev->aid);
    }
}

esp_err_t wifi_ap_start()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t ip_info{};
    esp_netif_str_to_ip4(PHOTO_FRAME_PAPER_IP, &ip_info.ip);
    esp_netif_str_to_ip4(PHOTO_FRAME_GATEWAY, &ip_info.gw);
    esp_netif_str_to_ip4(PHOTO_FRAME_NETMASK, &ip_info.netmask);
    esp_netif_dhcps_stop(ap_netif);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    esp_netif_dhcps_start(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, nullptr));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, PHOTO_FRAME_AP_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len       = strlen(PHOTO_FRAME_AP_SSID);
    wifi_config.ap.channel        = 6;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode       = WIFI_AUTH_WPA2_PSK;
    strncpy((char*)wifi_config.ap.password, PHOTO_FRAME_AP_PASSWORD, sizeof(wifi_config.ap.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP %s @ %s", PHOTO_FRAME_AP_SSID, PHOTO_FRAME_PAPER_IP);
    return ESP_OK;
}
