#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include <c_types.h>
#define USE_OPTIMIZE_PRINTF



#define DS1820_PIN 12

#define APP_NAME        "Remote Temperature Sensor"
#define APP_VER_MAJ		1
#define APP_VER_MIN		2
#define APP_VER_REV		0

#define MQTT_CLIENT_TYPE    	"env"

//#define DHT_TYPE		DS18B20
//#define DHT_TYPE		DHT11
#define DHT_TYPE		DHT22

#define DHT_MUX		PERIPHS_IO_MUX_GPIO4_U
#define DHT_FUNC		FUNC_GPIO4
#define DHT_PIN		GPIO_ID_PIN(4)

//#define NO_SLEEP


#ifdef NO_SLEEP
	#define REPORT_INTERVAL	600000

	#define LED_PIN       	2  // GPIO 2 = Green Led (0 = On, 1 = Off)
	#define LED_MUX       	PERIPHS_IO_MUX_GPIO2_U
	#define LED_FUNC      	FUNC_GPIO2

#else
	#define DEEP_SLEEP 600000000	/* microseconds, sleep for 10 minutes */
#endif



#define MQTT_HOST     			"192.168.13.100"
#define MQTT_PORT     			1883
#define MQTT_KEEPALIVE    		30  /*second*/
#define MQTT_RECONNECT_TIMEOUT  	10  /*second*/
#define MQTT_CLEAN_SESSION 		1
#define MQTT_BUF_SIZE   			1024
#define MQTT_CLIENT_ID    		"ESP"

#define PROTOCOL_NAMEv311

#define MQTT_TOPIC_BASE			"/angst/devices"




#define LOCAL_CONFIG_AVAILABLE

#ifndef LOCAL_CONFIG_AVAILABLE
#error Please copy user_config.sample.h to user_config.local.h and modify your configurations
#else
#include "user_config.local.h"
#endif


#ifdef ERROR_LEVEL
	#define ERROR( format, ... ) os_printf( "[ERROR] " format, ## __VA_ARGS__ )
#else
	#define ERROR( format, ... )
#endif

#ifdef WARN_LEVEL
	#define WARN( format, ... ) os_printf( "[WARN] " format, ## __VA_ARGS__ )
#else
	#define WARN( format, ... )
#endif

#ifdef INFO_LEVEL
	#define INFO( format, ... ) os_printf( "[INFO] " format, ## __VA_ARGS__ )
#else
	#define INFO( format, ... )
#endif

#ifdef DEBUG_LEVEL
	#define MQTT_DEBUG_ON
	#define DEBUG( format, ... ) os_printf( "[DEBUG] " format, ## __VA_ARGS__ )
#else
	#define DEBUG( format, ... )
#endif
#endif

