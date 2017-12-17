/*
 Driver for the temperature and humidity sensor DHT11 and DHT22
 Official repository: https://github.com/CHERTS/esp8266-dht11_22

 Copyright (C) 2014 Mikhail Grigorev (CHERTS)

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <ets_sys.h>
#include <osapi.h>
#include <c_types.h>
#include <user_interface.h>
#include <gpio.h>

#include "user_config.h"
#include "dht.h"

// list of commands DS18B20:


static struct dht_sensor_data reading = { .success = 0 };

enum DHTType sensor_type;


/***
 * CRC Table and code from Maxim AN 162:
 *  http://www.maximintegrated.com/en/app-notes/index.mvp/id/162
 */
unsigned const char dscrc_table[] = { 0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65, 157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220, 35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98, 190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255, 70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7, 219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154, 101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36, 248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185, 140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205, 17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80, 175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238, 50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115, 202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139, 87, 9, 235, 181, 54, 104, 138, 212,
		149, 203, 41, 119, 244, 170, 72, 22, 233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168, 116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53 };

unsigned char dowcrc = 0;

unsigned char ow_crc(unsigned char x) {
	dowcrc = dscrc_table[dowcrc ^ x];
	return dowcrc;
}

//End of Maxim AN162 code

unsigned char ROM_NO[8];
uint8_t LastDiscrepancy;
uint8_t LastFamilyDiscrepancy;
uint8_t LastDeviceFlag;

void write_bit(int bit) {
	os_delay_us(1);
	//Let bus get pulled high
	GPIO_DIS_OUTPUT(DHT_PIN);

	if (bit) {
		//5us low pulse
		GPIO_OUTPUT_SET(DHT_PIN, 0);
		os_delay_us(5);
		//let bus go high again (device should read about 30us later)
		GPIO_DIS_OUTPUT(DHT_PIN);// was OW_RLS	// input
		os_delay_us(55);
	} else {
		//55us low
		GPIO_OUTPUT_SET(DHT_PIN, 0);
		os_delay_us(55);
		//5us high
		GPIO_DIS_OUTPUT(DHT_PIN);// was OW_RLS	// input
		os_delay_us(5);
	}
}

//#####################################################################

int read_bit() {
	int bit = 0;
	os_delay_us(1);
	//Pull bus low for 15us
	GPIO_OUTPUT_SET(DHT_PIN, 0);
	os_delay_us(15);

	//Allow device to control bu
	GPIO_DIS_OUTPUT(DHT_PIN);
	os_delay_us(5);

	//Read value
	if (GPIO_INPUT_GET(DHT_PIN)) {		 //was if (OW_IN)
		bit = 1;
	}
	//Wait for end of bit
	os_delay_us(40);
	return bit;
}

//#####################################################################

void write_byte(uint8_t byte) {
	int i;
	for (i = 0; i < 8; i++) {
		write_bit(byte & 1);
		byte >>= 1;
	}
}

//#####################################################################

uint8_t read_byte() {
	unsigned int i;
	uint8_t byte = 0;
	for (i = 0; i < 8; i++) {
		byte >>= 1;
		if (read_bit())
			byte |= 0x80;
	}
	return byte;
}

int reset(void) {
	//Hold the bus low for 500us
	GPIO_OUTPUT_SET(DHT_PIN, 0); //was OW_LO
	os_delay_us(500);

	//Give up control of the bus and wait for a device to reply
	GPIO_DIS_OUTPUT(DHT_PIN);//set to input
	os_delay_us(80);

	//The bus should be low if the device is present:
	if (GPIO_INPUT_GET(DHT_PIN)) {
		ERROR("No device found.\r\n");
		//No device
		return 1;
	}

	//The device should have stopped pulling the bus now:
	os_delay_us(300);
	if (!GPIO_INPUT_GET(DHT_PIN)) {
		ERROR("Invalid state. Bus should be high.\r\n");
		//Something's wrong, the bus should be high
		return 2;
	}

	//All good.
	return 0;
}

struct dht_sensor_data* readDS18B20(void) {
	if(reset() != 0) {
		ERROR("Reset #1 failed\r\n");
		reading.success = 0;
		return &reading;
	}

	write_byte(DS1820_SKIP_ROM);  // skip ROM command
	write_byte(DS1820_CONVERT_T); // convert T command

	os_delay_us(750);
	if(reset() != 0) {
		ERROR("Reset #2 failed\r\n");
		reading.success = 0;
		return &reading;
	}
	write_byte(DS1820_SKIP_ROM);		// skip ROM command
	write_byte(DS1820_READ_SCRATCHPAD); // read scratchpad command

	uint8_t get[10];
	uint8_t k = 0;

	get[0] = read_byte();
	get[1] = read_byte();
	get[2] = read_byte();
	get[3] = read_byte();
	get[4] = read_byte();
	get[5] = read_byte();
	get[6] = read_byte();
	get[7] = read_byte();
	get[8] = read_byte();
	get[9] = read_byte();

	DEBUG("ScratchPAD DATA = %X %X %X %X %X %X %X %X %X\r\n",get[8],get[7],get[6],get[5],get[4],get[3],get[2],get[1],get[0]);

	dowcrc = 0;
	ow_crc(get[0]);
	ow_crc(get[1]);
	ow_crc(get[2]);
	ow_crc(get[3]);
	ow_crc(get[4]);
	ow_crc(get[5]);
	ow_crc(get[6]);
	ow_crc(get[7]);

	if (get[8] != dowcrc) {
		ERROR("CRC check failed: %02X %02X", get[8], dowcrc);
		reading.success = 0;
		return &reading;
	}
	uint8_t temp_msb = get[1]; // Sign byte + lsbit
	uint8_t temp_lsb = get[0]; // Temp data plus lsb

	uint16_t temp = temp_msb << 8 | temp_lsb;

	reading.success = 1;
	reading.temperature = (temp * 625.0) / 10000;

	INFO("Got a DS18B20 Reading: %d.%d\r\n", (int) reading.temperature, (int) (reading.temperature - (int) reading.temperature) * 100);

	return &reading;

}

void reset_search() {
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = FALSE;
	LastFamilyDiscrepancy = 0;

	ROM_NO[7] = 0;
	ROM_NO[6] = 0;
	ROM_NO[5] = 0;
	ROM_NO[4] = 0;
	ROM_NO[3] = 0;
	ROM_NO[2] = 0;
	ROM_NO[1] = 0;
	ROM_NO[0] = 0;
}
//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
uint8_t search(uint8_t *newAddr) {
	uint8_t id_bit_number;
	uint8_t last_zero, rom_byte_number, search_result;
	uint8_t id_bit, cmp_id_bit;
	uint8_t ii = 0;
	unsigned char ROM_NO[8];
	unsigned char rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;

	// if the last call was not the last one
	if (!LastDeviceFlag) {
		// 1-Wire reset
		ii = reset();
		if (ii) // ii>0
		{
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = FALSE;
			LastFamilyDiscrepancy = 0;
			return ii;	// Pass back the reset error status  gf***
		}
		// issue the search command

		write_byte(DS1820_SEARCHROM);

		// loop to do the search
		do {
			// read a bit and its complement
			id_bit = read_bit();
			cmp_id_bit = read_bit();

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1))
				break;
			else {
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
					search_direction = id_bit;  // bit write value for search
				else {
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy)
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					else
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);

					// if 0 was picked then record its position in LastZero
					if (search_direction == 0) {
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
							LastFamilyDiscrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;

				// serial number search direction write bit
				write_bit(search_direction);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0) {
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		} while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!(id_bit_number < 65)) {
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = TRUE;

			search_result = TRUE;	// All OK status GF***
		}
	}

	// if no device found then reset counters so next 'search' will be like a first
	//if (search_result || !ROM_NO[0])
	//if (!ROM_NO[0])
	if (!search_result || !ROM_NO[0]) {
		LastDiscrepancy = 0;
		LastDeviceFlag = FALSE;
		LastFamilyDiscrepancy = 0;
		search_result = FALSE;
	}
	newAddr[0] = ROM_NO[0];
	newAddr[1] = ROM_NO[1];
	newAddr[2] = ROM_NO[2];
	newAddr[3] = ROM_NO[3];
	newAddr[4] = ROM_NO[4];
	newAddr[5] = ROM_NO[5];
	newAddr[6] = ROM_NO[6];
	newAddr[7] = ROM_NO[7];
	return search_result;
}

static inline float scale_humidity(int *data) {
	if (sensor_type == DHT11) {
		return data[0];
	} else {
		float humidity = data[0] * 256 + data[1];
		return humidity /= 10;
	}
}

static inline float scale_temperature(int *data) {
	if (sensor_type == DHT11) {
		return data[2];
	} else {
		float temperature = data[2] & 0x7f;
		temperature *= 256;
		temperature += data[3];
		temperature /= 10;
		if (data[2] & 0x80)
			temperature *= -1;
		return temperature;
	}
}

struct dht_sensor_data *ICACHE_FLASH_ATTR readDHT1122(void) {
	int counter = 0;
	int laststate = 1;
	int j = 0;
	int checksum = 0;
	int data[100];
	data[0] = data[1] = data[2] = data[3] = data[4] = 0;

	// Wake up device, 250ms of high
	GPIO_OUTPUT_SET(DHT_PIN, 1);
	os_delay_us((uint16_t)(250*1000));

	// Hold low for 20ms
	GPIO_OUTPUT_SET(DHT_PIN, 0);
	os_delay_us((uint16_t)(20*1000));

	// High for 40ns
	GPIO_OUTPUT_SET(DHT_PIN, 1);
	os_delay_us(40);

	// Set DHT_PIN pin as an input
	GPIO_DIS_OUTPUT(DHT_PIN);

	// wait for pin to drop?
	int i = 0;
	while (GPIO_INPUT_GET(DHT_PIN) == 1 && i < DHT_MAXCOUNT) {
		os_delay_us(1);
		i++;
	}

	if (i == DHT_MAXCOUNT) {
		reading.success = 0;
		INFO("Failed to get reading, dying\r\n");
		return &reading;
	}

	// read data
	for (i = 0; i < DHT_MAXTIMINGS; i++) {
		// Count high time (in approx us)
		counter = 0;
		while (GPIO_INPUT_GET(DHT_PIN) == laststate) {
			counter++;
			os_delay_us(1);
			if (counter == 1000)
				break;
		}
		laststate = GPIO_INPUT_GET(DHT_PIN);
		if (counter == 1000)
			break;
		// store data after 3 reads
		if ((i > 3) && (i % 2 == 0)) {
			// shove each bit into the storage bytes
			data[j / 8] <<= 1;
			if (counter > DHT_BREAKTIME)
				data[j / 8] |= 1;
			j++;
		}
	}

	if (j >= 39) {
		checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
		INFO("DHT: %02x %02x %02x %02x [%02x] CS: %02x\r\n", data[0], data[1], data[2], data[3], data[4], checksum);
		if (data[4] == checksum) {
			// checksum is valid
			reading.temperature = scale_temperature(data);
			reading.humidity = scale_humidity(data);
			reading.success = 1;
		} else {
			INFO("Checksum was incorrect after %d bits. Expected %d but got %d\r\n", j, data[4], checksum);
			reading.success = 0;
		}
	} else {
		INFO("Got too few bits: %d should be at least 40\r\n", j);
		reading.success = 0;
	}
	return &reading;
}

struct dht_sensor_data *ICACHE_FLASH_ATTR DHTRead(void) {
	if (sensor_type == DS18B20) {
		DEBUG("Reading DS18B20\r\n");
		return readDS18B20();
	} else {
		DEBUG("Reading DHT 11/22\r\n");
		return readDHT1122();
	}
}

void ICACHE_FLASH_ATTR DHTInit(enum DHTType dht_type) {
	DEBUG("INIT DHT\r\n");
	sensor_type = dht_type;

	if(dht_type == DHT11) {
		INFO("DHT setup for type DHT11\r\n");
	} else if(dht_type == DHT22) {
		INFO("DHT setup for type DHT22\r\n");
	} else if(dht_type == DS18B20) {
		INFO("DHT setup for type DS18B20\r\n");
	} else {
		INFO("DHT setup for unknown type\r\n");
	}
	PIN_FUNC_SELECT(DHT_MUX, DHT_FUNC);

	if (dht_type != DS18B20) {
		//DEBUG("Enabled pullup\r\n");
		PIN_PULLUP_EN(DHT_MUX);
	}
}

