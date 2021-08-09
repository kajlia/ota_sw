/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "driver/gpio.h"
#include "protocol_examples_common.h"
#include "string.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

SemaphoreHandle_t otaSemaphore;

const int sw_version = 3;
esp_app_desc_t runningPartitionDesc;

static const char *TAG = "simple_ota_example";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");

void simple_ota_example_task(void *pvParameter);

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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void on_button_pushed(void *params)
{
	xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
  xSemaphoreGiveFromISR(otaSemaphore, pdFALSE);
}

void simple_ota_example_task(void *pvParameter)
{

	xSemaphoreTake(otaSemaphore, portMAX_DELAY);
    ESP_LOGI(TAG, "Starting OTA example");

    esp_http_client_config_t config = {
		.url = "https://github.com/kajlia/asd/blob/main/simple_ota.bin?raw=true",
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
    };

//    esp_err_t ret = esp_https_ota(&config);
    const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(NULL);
	esp_app_desc_t updatePartitionDesc;
	esp_ota_get_partition_description(updatePartition, &updatePartitionDesc);
	//printf("current firmware version: %s\n", updatePartitionDesc.version);
	printf("%s\t%s\n", runningPartitionDesc.version, updatePartitionDesc.version);
	if(strcmp(runningPartitionDesc.version, updatePartitionDesc.version)!=0){
		esp_err_t ret = esp_https_ota(&config);
		if (ret == ESP_OK) {
			esp_restart();
		} else {
			ESP_LOGE(TAG, "Firmware upgrade failed");
		}
	}else{
		printf("same versions\n");
		vTaskDelete( NULL );
	}
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
	const esp_partition_t *runningPartition = esp_ota_get_running_partition();
	esp_ota_get_partition_description(runningPartition, &runningPartitionDesc);
	printf("current firmware version: %s\n", runningPartitionDesc.version);

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_CONNECT_WIFI
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

	 gpio_config_t gpioConfig = {
	      .pin_bit_mask = 1ULL << GPIO_NUM_0,
	      .mode = GPIO_MODE_DEF_INPUT,
	      .pull_up_en = GPIO_PULLUP_ENABLE,
	      .pull_down_en = GPIO_PULLUP_DISABLE,
	      .intr_type = GPIO_INTR_NEGEDGE};
	  gpio_config(&gpioConfig);
	  gpio_install_isr_service(0);
	  gpio_isr_handler_add(GPIO_NUM_0, on_button_pushed, NULL);

	otaSemaphore = xSemaphoreCreateBinary();

//    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
}
