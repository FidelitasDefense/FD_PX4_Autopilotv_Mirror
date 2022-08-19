#include <px4_platform_common/px4_config.h>
#include <syslog.h>

#include <sys/types.h>
#include <stdbool.h>
#include <float.h>
#include <string.h>
#include <math.h>

#include <drivers/drv_pwm_output.h>
#include <drivers/drv_hrt.h>

#include "msp_defines.h"
#include "MspV1.hpp"

MspV1::MspV1(int fd)
:
	_fd(fd)
{
}

int MspV1::GetMessageSize(int message_type)
{
	return 0;
}

struct msp_message_descriptor_t
{
	uint8_t message_id;
	bool fixed_size;
	uint8_t message_size;
};

#define MSP_DESCRIPTOR_COUNT 9
const msp_message_descriptor_t msp_message_descriptors[MSP_DESCRIPTOR_COUNT] =
{
	{MSP_OSD_CONFIG, true, sizeof(msp_osd_config_t)},
	{MSP_NAME, true, sizeof(msp_name_t)},
	{MSP_ANALOG, true, sizeof(msp_analog_t)},
	{MSP_STATUS, true, sizeof(msp_status_BF_t)},
	{MSP_BATTERY_STATE, true, sizeof(msp_battery_state_t)},
	{MSP_RAW_GPS, true, sizeof(msp_raw_gps_t)},
	{MSP_ATTITUDE, true, sizeof(msp_attitude_t)},
	{MSP_ALTITUDE, true, sizeof(msp_altitude_t)},
	{MSP_COMP_GPS, true, sizeof(msp_comp_gps_t)}
};

#define MSP_FRAME_START_SIZE 5
#define MSP_CRC_SIZE 1
bool MspV1::Send(const uint8_t message_id, const void* payload)
{
	uint32_t payload_size = 0;

	msp_message_descriptor_t* desc = NULL;
	
	for (int i = 0; i < MSP_DESCRIPTOR_COUNT; i++)
	{
		if (message_id == msp_message_descriptors[i].message_id)
		{
			desc = (msp_message_descriptor_t*)&msp_message_descriptors[i];
			break;
		}
	}
	
	if (!desc)
	{
		return false;
	}
	
	if (!desc->fixed_size)
	{
		return false;
	}
	
	payload_size = desc->message_size;
	
	uint8_t packet[MSP_FRAME_START_SIZE + payload_size + MSP_CRC_SIZE];
	uint8_t crc;
	
	packet[0] = '$';
	packet[1] = 'M';
	packet[2] = '<';
	packet[3] = payload_size;
	packet[4] = message_id;
	
	crc = payload_size ^ message_id;
	
	memcpy(packet + MSP_FRAME_START_SIZE, payload, payload_size);
	
	for (uint32_t i = 0; i < payload_size; i ++)
	{
		crc ^= packet[MSP_FRAME_START_SIZE + i];
	}
	
	packet[MSP_FRAME_START_SIZE + payload_size] = crc;

	int packet_size =  MSP_FRAME_START_SIZE + payload_size + MSP_CRC_SIZE;
	return  write(_fd, packet, packet_size) == packet_size;
}