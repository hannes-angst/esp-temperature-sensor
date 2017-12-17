#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include <mem.h>
#include <os_type.h>
#include "user_config.h"
#include "mqtt.h"
#include "wifi.h"
#include "dht.h"
#include "info.h"

MQTT_Client mqttClient;
uint8 ttl = 0;
const enum DHTType dhtType = DHT_TYPE;
struct dht_sensor_data* measure;


#ifdef NO_SLEEP
static ETSTimer call_timer;
#endif

static void ICACHE_FLASH_ATTR gotoSleep() {
#ifndef NO_SLEEP
	if (ttl <= 0) {
		MQTT_Disconnect(&mqttClient);
	}
#endif
}

static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
	if (status == STATION_GOT_IP) {
		MQTT_Connect(&mqttClient);
	} else if (status != STATION_IDLE && status != STATION_CONNECTING) {
		WARN("WIFI Connection failed. Shutting down\r\n");
		ttl = 0;
		gotoSleep();
	}
}


static void ICACHE_FLASH_ATTR read_dht() {
	measure = DHTRead();
	if (!measure->success) {
		WARN("Error reading temperature and humidity.\n");
	}
}

static void ICACHE_FLASH_ATTR publish_dht22() {
	//Submit data
	char *topicBuf = (char*) os_zalloc(128);
	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	os_sprintf(topicBuf, "%s/%s/%s", MQTT_TOPIC_BASE, id, MQTT_CLIENT_TYPE);
	char *dataBuf = (char*) os_zalloc(256);
	int8_t len = 0;
	if (measure->success) {
		float lastTemp, lastHum;
		lastTemp = measure->temperature;
		lastHum = measure->humidity;
		len += os_sprintf(dataBuf + len, "{\"status\":\"OK\"");
		if (lastTemp < 0) {
			lastTemp *= -1;
			len += os_sprintf(dataBuf + len, ",\"temperature\":-%d.%d", (int) (lastTemp), (int) ((lastTemp - (int) lastTemp) * 100));
		} else {
			len += os_sprintf(dataBuf + len, ",\"temperature\":%d.%d", (int) (lastTemp), (int) ((lastTemp - (int) lastTemp) * 100));
		}
		if (dhtType != DS18B20) {
			len += os_sprintf(dataBuf + len, ",\"humidity\":%d.%d", (int) (lastHum), (int) ((lastHum - (int) lastHum) * 100));
		}
	} else {
		len += os_sprintf(dataBuf + len, "{\"status\":\"FAILED\"");
		len += os_sprintf(dataBuf + len, ",\"temperature\":0.0");
		if (dhtType != DS18B20) {
			len += os_sprintf(dataBuf + len, ",\"humidity\":0.0");
		}
	}
	len += os_sprintf(dataBuf + len, "}");
	dataBuf[len] = '\0';
	INFO("%s\r\n", dataBuf);
	ttl++;
	MQTT_Publish(&mqttClient, topicBuf, dataBuf, len, 0, 0);
	os_free(id);
	os_free(topicBuf);
	os_free(dataBuf);
}

#ifdef NO_SLEEP
static void ICACHE_FLASH_ATTR publish_dht22_cb() {
	GPIO_OUTPUT_SET(LED_PIN, 0);
	publish_dht22();
	GPIO_OUTPUT_SET(LED_PIN, 1);
}
#endif

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) {
	INFO("MQTT: Connected\r\n");
	mqttClient = *(MQTT_Client*) args;

#ifdef NO_SLEEP
	INFO("Starting timer with %d ms intervals.\r\n",REPORT_INTERVAL);
	os_timer_disarm(&call_timer);
	os_timer_setfn(&call_timer, (os_timer_func_t *) publish_dht22_cb, NULL);
	os_timer_arm(&call_timer, REPORT_INTERVAL, 1);
	publish_dht22_cb();
#else
	publish_dht22();
#endif
}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	DEBUG("MQTT: Disconnected\r\n");
#ifdef NO_SLEEP
	os_timer_disarm(&call_timer);
#else
	INFO("Going to deep sleep for %d seconds.\r\n", (DEEP_SLEEP/1000000));
	system_deep_sleep_set_option(1);
	system_deep_sleep(DEEP_SLEEP);
#endif
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	DEBUG("MQTT: Published\r\n");
	ttl--;
	gotoSleep();
}

static void ICACHE_FLASH_ATTR mqtt_init(void) {
	DEBUG("INIT MQTT\r\n");
	//If WIFI is connected, MQTT gets connected (see wifiConnectCb)
	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, SEC_NONSSL);

	char *clientId = (char*) os_zalloc(64);
	os_sprintf(clientId, "%s%08X", MQTT_CLIENT_ID, system_get_chip_id());

	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	//id will be copied by MQTT_InitLWT
	if (!MQTT_InitClient(&mqttClient, clientId, id, id, MQTT_KEEPALIVE, MQTT_CLEAN_SESSION)) {
		ERROR("Could not initialize MQTT client");
	}
	os_free(id);
	os_free(clientId);

	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);

}

static void ICACHE_FLASH_ATTR app_init(void) {
	print_info();
#ifdef NO_SLEEP
	INFO("Mode: No sleep\r\n");
	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);
#else
	INFO("Mode: Low power consumption\r\n");
#endif
	DHTInit(dhtType);
	read_dht();
	mqtt_init();

	struct ip_info info;
	IP4_ADDR(&info.ip, 192, 168, 13, (system_get_chip_id() % 72) + 2);
	IP4_ADDR(&info.gw, 192, 168, 13, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);

	WIFI_Connect_IP(STA_SSID, STA_PASS, &info, wifiConnectCb);
}

void user_init(void) {
	system_init_done_cb(app_init);
}

