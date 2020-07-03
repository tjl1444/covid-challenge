/* COVID Challenge Task
 * Contacts the covid19 API via TLS v1.2 and reads a JSON
 * response. 
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "tcpip_adapter.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"

/* Constants for HTTPS GET requests */
#define WEB_SERVER "www.api.covid19api.com"
#define WEB_PORT "443"
#define WEB_URL "https://api.covid19api.com/country/south-africa/status/confirmed?from=2020-04-01T00:00:00Z&to=2020-04-05T00:00:00Z"

/* Constants for JSON parsing */
#define DATE_STRING_LENGTH 10
#define CASES_STRING_LENGTH 10
#define JSON_SEARCH_START_POINT 8
#define JSON_OFFSET 4

static const char* DATE_STRING = "Date";
static const char* CASES_STRING = "Cases";

static const char* TAG = "covid challenge";

static const char* REQUEST = "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";


// Root certificate for api.covid19.api.com which has been taken from server_root_cert.pem
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

// Function prototypes
void extract_date(char* date_str, char* cptr);
uint16_t extract_cases(char* cases_str, char* cptr);

static void https_get_task(void *pvParameters)
{
    
    char buf[24000];
    int32_t tls_return;
    int32_t buf_len = sizeof(buf);
    
    while(1) {
        int32_t read_count = 0;
        esp_tls_cfg_t cfg = {
            .cacert_pem_buf  = server_root_cert_pem_start,
            .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        };
        
        // Create blocking TLS/SSL connection 
        struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);
        
        // Check if the TLS/SSL connection established 
        if(tls != NULL) {
            ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            goto exit;
        }
        
        // Write GET request into the tls connection
        size_t written_bytes = 0;
        do {
            tls_return = esp_tls_conn_write(tls, REQUEST + written_bytes, strlen(REQUEST) - written_bytes);
            
            if (tls_return >= 0) {
                // tls write connection was successful
                ESP_LOGI(TAG, "%d bytes written", tls_return);
                written_bytes += tls_return;
            } else if (tls_return != MBEDTLS_ERR_SSL_WANT_READ  && tls_return != MBEDTLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", tls_return);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        // Fill the buffer with the full JSON response
        do
        {
            bzero(buf + read_count, buf_len - read_count);
            tls_return = esp_tls_conn_read(tls, (char*) (buf + read_count), buf_len - read_count - 1);
            
            if(tls_return == MBEDTLS_ERR_SSL_WANT_WRITE  || tls_return == MBEDTLS_ERR_SSL_WANT_READ)
                continue;
            
            if(tls_return < 0)
           {
                ESP_LOGI(TAG, "esp_tls_conn return < 0");
                ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -tls_return);
                break;
            }

            if(tls_return == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            read_count += tls_return;
            ESP_LOGI(TAG, "Bytes being read -> %d bytes read", tls_return);

        } while(1);

    exit:
        ESP_LOGI(TAG, "In EXIT Function HTTP TASK");
        esp_tls_conn_delete(tls); 
        
        // Print out JSON to stdout
        for (int32_t i = 0; i < read_count; i++){
            putchar(buf[i]);
        }
        putchar('\n'); // JSON output doesn't have a newline at end
       
        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);
        ESP_LOGI(TAG, "Read count is %d", read_count);

        if (read_count > 0 && request_count == 1) {
            ESP_LOGI(TAG, "Printing out dates from JSON");
            char* ptr = strstr(buf, DATE_STRING);
            while(ptr != NULL) {
             char date_str[DATE_STRING_LENGTH];
                extract_date(date_str, ptr);
                ptr = strstr(ptr + JSON_OFFSET, DATE_STRING);
            }
        }

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
    
}


void extract_date(char* date_str, char* cptr) {
    // Extracts the date from a JSON by parsing the JSON string starting the first char 'D' 
	date_str[DATE_STRING_LENGTH] = '\0';

	uint8_t i = 0;
	for (i = 0; i < DATE_STRING_LENGTH; i++) {
	    date_str[i] = *(cptr + JSON_SEARCH_START_POINT + i);
	}
	// Testing
	printf("\nDate with printf ---> %s\n", date_str);

}


uint16_t extract_cases(char* cases_str, char* cptr) {
    // Extract number of cases from a JSON string that starts from "Case"
    // Returns the number of covid cases
	cases_str[CASES_STRING_LENGTH] = '\0';

	uint8_t i;
	for (i = 0; i < CASES_STRING_LENGTH; i++) {
	    cases_str[i] = *(cptr + JSON_SEARCH_START_POINT + i);
	}
	uint16_t case_count = atoi(cases_str);

	// Testing
	printf("\nThe number of cases is ---> %d\n", case_count);
	return case_count;

}
    

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Establish WiFi connection using a basic blocking call 
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(&https_get_task, "https_get_task", 8192 * 4, NULL, 5, NULL);
}
