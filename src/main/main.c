/*
 * NexaSense - firmware EDUCACIONAL (prototipo) da NexaHome Technologies.
 *
 * ATENCAO: este firmware contem fragilidades PROPOSITAIS para fins de
 * avaliacao de seguranca em laboratorio (UniFECAF - IoT Security).
 * Nao utilizar em producao.
 *
 * Funcoes:
 *  - le "sensor" de temperatura/umidade (simulado no QEMU; DHT22 no Wokwi)
 *  - publica telemetria via MQTT (1883, texto claro)
 *  - recebe comandos MQTT (reboot, interval, get_config, ota)
 *  - servidor HTTP local (/status, /config)
 *  - console de diagnostico na UART0 (sem autenticacao)
 *  - atualizacao OTA via HTTP sem verificacao de assinatura
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_flash_encrypt.h"
#include "esp_secure_boot.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mqtt_client.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "secrets.h"

#define FW_VERSION  "1.0.0-proto"
static const char *TAG = "NEXASENSE";

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_host[64];
    int32_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[64];
    char api_token[64];
    char ota_url[128];
    int32_t interval_s;
} nexa_cfg_t;

static nexa_cfg_t g_cfg;
static char g_dev_id[20];
static esp_mqtt_client_handle_t g_mqtt;
static char g_ip[16] = "0.0.0.0";
static volatile bool g_net_up = false;

/* ------------------------------------------------------------------ */
/* Configuracao em NVS (sem criptografia)                              */
/* ------------------------------------------------------------------ */
static void cfg_load_or_provision(void)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("nexacfg", NVS_READWRITE, &h));
    size_t len = sizeof(g_cfg.wifi_ssid);
    if (nvs_get_str(h, "wifi_ssid", g_cfg.wifi_ssid, &len) != ESP_OK) {
        ESP_LOGW(TAG, "NVS vazia -> gravando configuracao de fabrica");
        nvs_set_str(h, "wifi_ssid", NEXA_WIFI_SSID);
        nvs_set_str(h, "wifi_pass", NEXA_WIFI_PASS);
        nvs_set_str(h, "mqtt_host", NEXA_MQTT_HOST);
        nvs_set_i32(h, "mqtt_port", NEXA_MQTT_PORT);
        nvs_set_str(h, "mqtt_user", NEXA_MQTT_USER);
        nvs_set_str(h, "mqtt_pass", NEXA_MQTT_PASS);
        nvs_set_str(h, "api_token", NEXA_API_TOKEN);
        nvs_set_str(h, "ota_url", NEXA_OTA_DEFAULT_URL);
        nvs_set_i32(h, "interval", 10);
        nvs_commit(h);
    }
#define GETS(k, f) do { size_t l = sizeof(g_cfg.f); nvs_get_str(h, k, g_cfg.f, &l); } while (0)
    GETS("wifi_ssid", wifi_ssid);
    GETS("wifi_pass", wifi_pass);
    GETS("mqtt_host", mqtt_host);
    GETS("mqtt_user", mqtt_user);
    GETS("mqtt_pass", mqtt_pass);
    GETS("api_token", api_token);
    GETS("ota_url", ota_url);
    nvs_get_i32(h, "mqtt_port", &g_cfg.mqtt_port);
    nvs_get_i32(h, "interval", &g_cfg.interval_s);
    nvs_close(h);
}

static void cfg_set_interval(int v)
{
    nvs_handle_t h;
    if (nvs_open("nexacfg", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, "interval", v);
        nvs_commit(h);
        nvs_close(h);
    }
    g_cfg.interval_s = v;
}

static int cfg_to_json(char *buf, size_t n)
{
    return snprintf(buf, n,
        "{\"device\":\"%s\",\"fw\":\"%s\",\"wifi_ssid\":\"%s\",\"wifi_pass\":\"%s\","
        "\"mqtt\":\"%s:%ld\",\"mqtt_user\":\"%s\",\"mqtt_pass\":\"%s\","
        "\"api_token\":\"%s\",\"cloud\":\"%s\",\"ota_url\":\"%s\",\"interval\":%ld}",
        g_dev_id, FW_VERSION, g_cfg.wifi_ssid, g_cfg.wifi_pass, g_cfg.mqtt_host,
        (long)g_cfg.mqtt_port, g_cfg.mqtt_user, g_cfg.mqtt_pass, g_cfg.api_token,
        NEXA_CLOUD_ENDPOINT, g_cfg.ota_url, (long)g_cfg.interval_s);
}

/* ------------------------------------------------------------------ */
/* Sensor (simulado)                                                   */
/* ------------------------------------------------------------------ */
static void sensor_read(float *t, float *hum)
{
    double s = esp_timer_get_time() / 1e6;
    *t = 24.0f + 2.5f * sinf(s / 60.0f) + (esp_random() % 10) / 20.0f;
    *hum = 55.0f + 8.0f * cosf(s / 90.0f) + (esp_random() % 10) / 10.0f;
}

/* ------------------------------------------------------------------ */
/* OTA via HTTP - SEM verificacao de assinatura/autenticidade          */
/* ------------------------------------------------------------------ */
static void ota_task(void *arg)
{
    char *url = (char *)arg;
    ESP_LOGW(TAG, "OTA: baixando imagem de %s (HTTP, sem TLS, sem assinatura)", url);
    esp_http_client_config_t hc = { .url = url, .timeout_ms = 10000 };
    esp_http_client_handle_t cli = esp_http_client_init(&hc);
    if (esp_http_client_open(cli, 0) != ESP_OK) { ESP_LOGE(TAG, "OTA: falha ao conectar"); goto out; }
    int total = esp_http_client_fetch_headers(cli);
    const esp_partition_t *dst = esp_ota_get_next_update_partition(NULL);
    esp_ota_handle_t oh;
    if (esp_ota_begin(dst, OTA_SIZE_UNKNOWN, &oh) != ESP_OK) { ESP_LOGE(TAG, "OTA: begin falhou"); goto out; }
    static char buf[1024];
    int rd, got = 0;
    while ((rd = esp_http_client_read(cli, buf, sizeof(buf))) > 0) {
        esp_ota_write(oh, buf, rd);
        got += rd;
    }
    ESP_LOGI(TAG, "OTA: %d bytes recebidos (content-length=%d) -> particao %s", got, total, dst->label);
    if (esp_ota_end(oh) != ESP_OK) { ESP_LOGE(TAG, "OTA: imagem invalida (checksum)"); goto out; }
    esp_ota_set_boot_partition(dst);
    ESP_LOGW(TAG, "OTA: imagem aceita SEM verificar assinatura. Reiniciando...");
    esp_http_client_cleanup(cli);
    free(url);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
out:
    esp_http_client_cleanup(cli);
    free(url);
    vTaskDelete(NULL);
}

static void start_ota(const char *url)
{
    xTaskCreate(ota_task, "ota", 8192, strdup(url), 5, NULL);
}

/* ------------------------------------------------------------------ */
/* MQTT                                                                */
/* ------------------------------------------------------------------ */
static void handle_cmd(const char *data, int len)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "%.*s", len, data);
    ESP_LOGI(TAG, "MQTT cmd recebido: %s", msg);
    char resp_topic[80];
    snprintf(resp_topic, sizeof(resp_topic), "nexahome/sense/%s/resp", g_dev_id);
    if (strstr(msg, "\"reboot\"")) {
        esp_mqtt_client_publish(g_mqtt, resp_topic, "{\"ok\":\"rebooting\"}", 0, 1, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    } else if (strstr(msg, "\"get_config\"")) {
        char out[640];
        cfg_to_json(out, sizeof(out));
        esp_mqtt_client_publish(g_mqtt, resp_topic, out, 0, 1, 0);
    } else if (strstr(msg, "\"interval\"")) {
        char *p = strstr(msg, "\"value\"");
        if (p && (p = strchr(p, ':'))) {
            cfg_set_interval(atoi(p + 1));
            esp_mqtt_client_publish(g_mqtt, resp_topic, "{\"ok\":\"interval\"}", 0, 1, 0);
        }
    } else if (strstr(msg, "\"ota\"")) {
        char *p = strstr(msg, "\"url\"");
        char url[128] = {0};
        if (p && (p = strchr(p + 5, '"'))) {
            char *e = strchr(p + 1, '"');
            if (e) snprintf(url, sizeof(url), "%.*s", (int)(e - p - 1), p + 1);
        }
        esp_mqtt_client_publish(g_mqtt, resp_topic, "{\"ok\":\"ota_started\"}", 0, 1, 0);
        start_ota(url[0] ? url : g_cfg.ota_url);
    }
}

static void mqtt_event(void *a, esp_event_base_t b, int32_t id, void *data)
{
    esp_mqtt_event_handle_t ev = data;
    char topic[80];
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT conectado em mqtt://%s:%ld", g_cfg.mqtt_host, (long)g_cfg.mqtt_port);
        snprintf(topic, sizeof(topic), "nexahome/sense/%s/cmd", g_dev_id);
        esp_mqtt_client_subscribe(g_mqtt, topic, 1);
        ESP_LOGI(TAG, "Inscrito em %s", topic);
        break;
    case MQTT_EVENT_DATA:
        handle_cmd(ev->data, ev->data_len);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT desconectado");
        break;
    default:
        break;
    }
}

static void mqtt_start(void)
{
    char uri[96];
    snprintf(uri, sizeof(uri), "mqtt://%s:%ld", g_cfg.mqtt_host, (long)g_cfg.mqtt_port);
    esp_mqtt_client_config_t mc = {
        .broker.address.uri = uri,
        .credentials.username = g_cfg.mqtt_user,
        .credentials.client_id = g_dev_id,
        .credentials.authentication.password = g_cfg.mqtt_pass,
    };
    g_mqtt = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(g_mqtt, ESP_EVENT_ANY_ID, mqtt_event, NULL);
    esp_mqtt_client_start(g_mqtt);
}

static void telemetry_task(void *arg)
{
    char topic[80], payload[200];
    snprintf(topic, sizeof(topic), "nexahome/sense/%s/telemetry", g_dev_id);
    for (;;) {
        float t, h;
        sensor_read(&t, &h);
        snprintf(payload, sizeof(payload),
                 "{\"device\":\"%s\",\"fw\":\"%s\",\"temp\":%.2f,\"hum\":%.2f,\"token\":\"%s\",\"uptime\":%lld}",
                 g_dev_id, FW_VERSION, t, h, g_cfg.api_token, esp_timer_get_time() / 1000000);
        if (g_mqtt) esp_mqtt_client_publish(g_mqtt, topic, payload, 0, 0, 0);
        ESP_LOGD(TAG, "telemetria: %s", payload);
        vTaskDelay(pdMS_TO_TICKS(g_cfg.interval_s * 1000));
    }
}

/* ------------------------------------------------------------------ */
/* HTTP local (porta 80) - sem autenticacao                            */
/* ------------------------------------------------------------------ */
static esp_err_t h_status(httpd_req_t *r)
{
    char out[300];
    snprintf(out, sizeof(out),
             "{\"device\":\"%s\",\"fw\":\"%s\",\"idf\":\"%s\",\"ip\":\"%s\",\"uptime\":%lld,"
             "\"mqtt\":\"%s:%ld\",\"free_heap\":%lu}",
             g_dev_id, FW_VERSION, esp_get_idf_version(), g_ip, esp_timer_get_time() / 1000000,
             g_cfg.mqtt_host, (long)g_cfg.mqtt_port, (unsigned long)esp_get_free_heap_size());
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, out);
}

static esp_err_t h_config(httpd_req_t *r)
{
    char out[640];
    cfg_to_json(out, sizeof(out));
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, out);
}

static void http_start(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t s;
    if (httpd_start(&s, &c) == ESP_OK) {
        httpd_uri_t u1 = { .uri = "/status", .method = HTTP_GET, .handler = h_status };
        httpd_uri_t u2 = { .uri = "/config", .method = HTTP_GET, .handler = h_config };
        httpd_register_uri_handler(s, &u1);
        httpd_register_uri_handler(s, &u2);
        ESP_LOGI(TAG, "HTTP local ativo: GET /status, GET /config (sem autenticacao)");
    }
}

/* ------------------------------------------------------------------ */
/* Rede: no QEMU usamos OpenETH (emulado). No hardware/Wokwi: Wi-Fi.   */
/* ------------------------------------------------------------------ */
static void got_ip(void *a, esp_event_base_t b, int32_t id, void *data)
{
    ip_event_got_ip_t *e = data;
    snprintf(g_ip, sizeof(g_ip), IPSTR, IP2STR(&e->ip_info.ip));
    ESP_LOGI(TAG, "Rede OK - IP %s gw " IPSTR, g_ip, IP2STR(&e->ip_info.gw));
    g_net_up = true;
}

static void net_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Conectando ao Wi-Fi SSID='%s' senha='%s'", g_cfg.wifi_ssid, g_cfg.wifi_pass);
    ESP_LOGI(TAG, "(ambiente QEMU: enlace Wi-Fi substituido por OpenETH emulado)");
    esp_netif_config_t nc = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&nc);
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.autonego_timeout_ms = 100;
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
    esp_eth_config_t ec = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eh;
    ESP_ERROR_CHECK(esp_eth_driver_install(&ec, &eh));
    esp_netif_attach(netif, esp_eth_new_netif_glue(eh));
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip, NULL);
    ESP_ERROR_CHECK(esp_eth_start(eh));
}

/* ------------------------------------------------------------------ */
/* Console de diagnostico na UART0 - SEM autenticacao                  */
/* ------------------------------------------------------------------ */
static void console_task(void *arg)
{
    char line[128];
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    uart_vfs_dev_use_driver(UART_NUM_0);
    uart_vfs_dev_port_set_rx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CR);
    setvbuf(stdin, NULL, _IONBF, 0);
    printf("\n[diag] console de diagnostico pronto. Digite 'help'.\n");
    for (;;) {
        printf("nexa> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0]) continue;
        if (!strcmp(line, "help")) {
            printf("comandos: help | info | config | nvs | ota <url> | interval <s> | reboot | factory\n");
        } else if (!strcmp(line, "info")) {
            const esp_partition_t *run = esp_ota_get_running_partition();
            printf("device=%s fw=%s idf=%s part=%s ip=%s\n", g_dev_id, FW_VERSION,
                   esp_get_idf_version(), run->label, g_ip);
            printf("secure_boot=%s flash_encryption=%s\n",
                   esp_secure_boot_enabled() ? "ON" : "OFF",
                   esp_flash_encryption_enabled() ? "ON" : "OFF");
        } else if (!strcmp(line, "config") || !strcmp(line, "nvs")) {
            char out[640];
            cfg_to_json(out, sizeof(out));
            printf("%s\n", out);
        } else if (!strncmp(line, "ota ", 4)) {
            start_ota(line + 4);
        } else if (!strncmp(line, "interval ", 9)) {
            cfg_set_interval(atoi(line + 9));
            printf("interval=%ld\n", (long)g_cfg.interval_s);
        } else if (!strcmp(line, "reboot")) {
            esp_restart();
        } else if (!strcmp(line, "factory")) {
            nvs_flash_erase();
            printf("NVS apagada. Reiniciando...\n");
            esp_restart();
        } else {
            printf("comando desconhecido\n");
        }
    }
}

void app_main(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(g_dev_id, sizeof(g_dev_id), "NXS-%02X%02X%02X", mac[3], mac[4], mac[5]);

    printf("\n==============================================\n");
    printf(" NexaSense %s  (build %s %s)\n", FW_VERSION, d->date, d->time);
    printf(" ESP-IDF %s | projeto %s\n", d->idf_ver, d->project_name);
    printf(" DEBUG BUILD - diagnostico habilitado\n");
    printf("==============================================\n");

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    cfg_load_or_provision();
    ESP_LOGI(TAG, "Device ID: %s  MAC: %02x:%02x:%02x:%02x:%02x:%02x", g_dev_id,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "MQTT broker=%s:%ld user=%s pass=%s", g_cfg.mqtt_host, (long)g_cfg.mqtt_port,
             g_cfg.mqtt_user, g_cfg.mqtt_pass);
    ESP_LOGI(TAG, "API token=%s endpoint=%s", g_cfg.api_token, NEXA_CLOUD_ENDPOINT);
    ESP_LOGI(TAG, "OTA url=%s | PIN de servico=%s", g_cfg.ota_url, NEXA_SERVICE_PIN);

    xTaskCreate(console_task, "console", 4096, NULL, 3, NULL);
    net_start();
    while (!g_net_up) vTaskDelay(pdMS_TO_TICKS(100));
    http_start();
    mqtt_start();
    xTaskCreate(telemetry_task, "telemetry", 4096, NULL, 4, NULL);
}
