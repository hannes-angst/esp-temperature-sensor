#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include "user_config.h"

const char *ICACHE_FLASH_ATTR flashSizeMap() {
	switch (system_get_flash_size_map()) {
	case FLASH_SIZE_4M_MAP_256_256:
		return "Flash size : 4Mbits. Map : 256KBytes + 256KBytes";
	case FLASH_SIZE_2M:
		return "Flash size : 2Mbits. Map : 256KBytes";
	case FLASH_SIZE_8M_MAP_512_512:
		return "Flash size : 8Mbits. Map : 512KBytes + 512KBytes";
	case FLASH_SIZE_16M_MAP_512_512:
		return "Flash size : 16Mbits. Map : 512KBytes + 512KBytes";
	case FLASH_SIZE_32M_MAP_512_512:
		return "Flash size : 32Mbits. Map : 512KBytes + 512KBytes";
	case FLASH_SIZE_16M_MAP_1024_1024:
		return "Flash size : 16Mbits. Map : 1024KBytes + 1024KBytes";
	case FLASH_SIZE_32M_MAP_1024_1024:
		return "attention: don't support now ,just compatible for nodemcu";
	case FLASH_SIZE_32M_MAP_2048_2048:
		return "Flash size : 32Mbits. Map : 2048KBytes + 2048KBytes";
	case FLASH_SIZE_64M_MAP_1024_1024:
		return "Flash size : 64Mbits. Map : 1024KBytes + 1024KBytes";
	case FLASH_SIZE_128M_MAP_1024_1024:
		return "Flash size : 128Mbits. Map : 1024KBytes + 1024KBytes";
	default:
		return "Unknown";
	}

}

void ICACHE_FLASH_ATTR print_info() {
	INFO("\r\n\r\n");
	INFO("BOOTUP...\r\n");
	INFO("SDK version:%s rom %d\r\n", system_get_sdk_version(), system_upgrade_userbin_check());
	INFO("Time = %ld\n", system_get_time());
	INFO("Chip ID: %08X\r\n", system_get_chip_id());
	INFO("CPU freq: %d MHz\r\n", system_get_cpu_freq());
	INFO("Flash size map [%d]: %s\r\n", system_get_flash_size_map(), flashSizeMap());
	INFO("Free heap size: %d\r\n", system_get_free_heap_size());
	INFO("Memory info:\r\n");
	system_print_meminfo();
	INFO("-------------------------------------------\r\n");
	INFO("%s v.%d.%d.%d \r\n", APP_NAME, APP_VER_MAJ, APP_VER_MIN, APP_VER_REV);
	INFO("compile time:%s %s\r\n", __DATE__, __TIME__);
#ifdef FOTA_ENABLED
	INFO("Update revision path: http://%s:%d%s\r\n", FOTA_HOST, FOTA_PORT, FOTA_PATH);
#endif
	INFO("-------------------------------------------\r\n");
}
