#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sys.h"

static const char TAG[] = "server";

EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;
     case SYSTEM_EVENT_STA_GOT_IP:
       ESP_LOGI(TAG, "IP: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
       xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
       break;
     case SYSTEM_EVENT_STA_DISCONNECTED:
       esp_wifi_connect();
       xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
       break;
     default:
       break;
  }
  return ESP_OK;
}

void wifi_init() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_event_group = xEventGroupCreate();

  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_WIFI_SSID,
      .password = CONFIG_WIFI_PASSWORD,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}
