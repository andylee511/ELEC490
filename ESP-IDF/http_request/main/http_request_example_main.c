/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_tls.h"
#include "esp_crc.h"

#include "esp_http_client.h"
#include "cJSON.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG1 = "HTTP_CLIENT";

#define EXAMPLE_ESP_WIFI_SSID "217 Bathurst"
#define EXAMPLE_ESP_WIFI_PASS "umRLv&q29eXpjf3xV75_zsu&tknA_b&tf+j$j3W-en=s&+R-J+R-rRkgSy8@KPQ"
#define EXAMPLE_ESP_MAXIMUM_RETRY 100

static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG1, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG1, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG1, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG1, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG1, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */

            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG1, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG1, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG1, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                if (output_buffer != NULL) {
                    free(output_buffer);
                    output_buffer = NULL;
                }
                output_len = 0;
                ESP_LOGI(TAG1, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG1, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
} 


static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int s_retry_num = 0;
    // wifi事件状态为 start 时连接
    if (event_base == WIFI_EVENT )
    {   
        if (event_id == WIFI_EVENT_STA_START)       // 事件id为STA开始
        {
            ESP_LOGI("WIFI_EVENT ", "connecting to wifi\n");
            esp_wifi_connect();            
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)   // 事件id为STA断开，重新连接直到最大重连数
        {
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
            {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI("WIFI_EVENT：", "reconnecting to AP");
            }
            else
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI("WIFI_EVENT：", "AP connection failed");
        }

    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)     // 系统事件为ip地址事件，且事件id为成功获取ip地址
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("IP_EVENT ", "GOT IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
void NVS_Init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("esp_err_t类型返回值：", " %d\n", ret);
}

void wifi_init_sta(void)
{
    
    s_wifi_event_group = xEventGroupCreate();                   // 创建事件标志组，返回值为事件标志组句柄,根据WIFI的连接状况来做出相应动作。

    ESP_ERROR_CHECK(esp_netif_init());                          // 初始化底层TCP/IP堆栈
    ESP_ERROR_CHECK(esp_event_loop_create_default());           // 创建默认事件循环
    esp_netif_create_default_wifi_sta();                        // 创建一个默认的WIFI-STA网络接口
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();        // 默认的wifi配置参数
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                       // 根据 cfg 初始化 wifi
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));    // 配置存储类型设置为 RAM

    // 将事件处理程序注册到系统默认事件循环，分别是WiFi事件和IP地址事件
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // 配置WiFi连接的 ssid password 参数
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));                              // 工作模式设置为 STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));            // 根据 wifi_config 进行设置
    ESP_ERROR_CHECK(esp_wifi_start());                                              // 启动wifi连接

    /* 等待标志组。条件到达最大重试次数 WIFI_FAIL_BIT ，直到连接建立 WIFI_CONNECTED_BIT 或连接失败。位由event_handler()设置(参见上面) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,                      // 需要等待的事件标志组的句柄 
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,      // 需要等待的事件位
                                           pdFALSE,                                 // 为pdFALSE时，在退出此函数之前所设置的这些事件位不变，为pdFALSE则清零
                                           pdFALSE,                                 // 为pdFALSE时，设置的这些事件位任意一个置1就会返回，为pdFALSE则需全为1才返回
                                           portMAX_DELAY);                          // 设置为最长阻塞等待时间，单位为时钟节拍

    /* xEventGroupWaitBits()返回调用的位，根据事件标志组等待函数的返回值获取WiFi连接状态 */
    if (bits & WIFI_CONNECTED_BIT)                                                  // WiFi连接成功事件
    {
        ESP_LOGI("bits status:", "connected successfully\n ssid:%s\n password:%s\n",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);                     
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI("bits status：", "connected failed\n 名称:%s\n 密码:%s\n",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE("bits status：", "incident happen");
    }

    // 注销事件
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);                                            // 删除WiFi连接事件标志组
}

 void http_post_demo(void)
 {
     cJSON *root  = cJSON_CreateObject();
    cJSON_AddItemToObject(root , "api_key",   cJSON_CreateString("DaoBanMoJie_JiaIT"));
    cJSON_AddItemToObject(root , "id",        cJSON_CreateString("2"));
    cJSON_AddItemToObject(root , "Temperature",      cJSON_CreateString("temperatire is -20"));//(buf->payload[0]
    cJSON_AddItemToObject(root , "Humidity",      cJSON_CreateString("Humidity is 99"));//buf->payload[1]
   // we can send the motion cJSON_AddItemToObject(root , "value1",    cJSON_CreateNumber(1230));
    const char *post_data = cJSON_Print(root);

        esp_http_client_config_t config = {
        .method = HTTP_METHOD_POST,  
        .url = "http://cay.bal.mybluehost.me/post-esp-data.php",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t t1 = esp_http_client_set_post_field(client, post_data, strlen(post_data));//set post data
    if(t1 == ESP_OK)
    {
        printf("post data create successfully!, string posted is %s \n", post_data);
    }
    //printf("pst data : %s\n", post_data);
    esp_err_t err = esp_http_client_perform(client);//perform the transfer
    if (err == ESP_OK) {
        ESP_LOGI(
            
            
            
            
            TAG1, "HTTP POST Status = %d, content_length = %d",esp_http_client_get_status_code(client),esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG1, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);



 }

static void http_test_task(void *pvParameters)
{
   
    http_post_demo();
    ESP_LOGI(TAG1, "http 示例结束");
    vTaskDelete(NULL);
}

void app_main(void)
{
    
    NVS_Init();
    wifi_init_sta();
    xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
}
