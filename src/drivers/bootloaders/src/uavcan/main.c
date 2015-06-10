/****************************************************************************
 *
 *   Copyright (c) 2015 PX4 Development Team. All rights reserved.
 *       Author: Ben Dyer <ben_dyer@mac.com>
 *               Pavel Kirienko <pavel.kirienko@zubax.com>
 *               David Sidrane <david_s5@nscdg.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include "boot_config.h"

#include <nuttx/arch.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "chip.h"
#include "stm32.h"
#include "nvic.h"

#include "board.h"
#include "flash.h"
#include "timer.h"
#include "can.h"
#include "uavcan.h"
#include "crc.h"
#include "boot_app_shared.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef volatile struct bootloader_t {
	can_speed_t bus_speed;
	volatile uint8_t node_id;
	volatile uint8_t status_code;
	volatile bool app_valid;
	volatile uint32_t uptime;
	volatile app_descriptor_t *fw_image_descriptor;
	volatile uint32_t *fw_image;
	bool  wait_for_getnodeinfo;
	bool  app_bl_request;
	bool sent_node_info_response;
	union {
		uint32_t l;
		uint8_t b[sizeof(uint32_t)];
	} fw_word0;

} bootloader_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

bootloader_t bootloader;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: start_the_watch_dog
 *
 * Description:
 *   This function will start the hardware watchdog. Once stated the code must
 *   kick it before it time out a reboot will occur.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static inline void start_the_watch_dog(void)
{
#ifdef OPT_ENABLE_WD
#endif
}
/****************************************************************************
 * Name: kick_the_watch_dog
 *
 * Description:
 *   This function will reset the watchdog timeout
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static inline void kick_the_watch_dog(void)
{
#ifdef OPT_ENABLE_WD
#endif
}

/****************************************************************************
 * Name: uptime_process
 *
 * Description:
 *   Run as a timer callback off the system tick ISR. This function counts
 *   Seconds of up time.
 *
 * Input Parameters:
 *   Not Used.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void uptime_process(bl_timer_id id, void *context)
{
	bootloader.uptime++;
}


/****************************************************************************
 * Name: node_info_process
 *
 * Description:
 *   Run as a timer callback off the system tick ISR.
 *   Once we have a node id, this process is started and handles the
 *   GetNodeInfo replies.  This function uses the fifoGetNodeInfo and
 *   MBGetNodeInfo of the CAN so it can coexist regardless of
 *   application state and what is it doing.
 *
 * Input Parameters:
 *   Not Used.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void node_info_process(bl_timer_id id, void *context)
{

	uavcan_getnodeinfo_response_t response;
	uavcan_nodestatus_t node_status;
	uavcan_frame_id_t frame_id;
	size_t frame_len;
	uint32_t rx_message_id;
	uint8_t frame_payload[8];

	node_status.uptime_sec = bootloader.uptime;
	node_status.status_code = bootloader.status_code;
	node_status.vendor_specific_status_code = 0u;
	uavcan_pack_nodestatus(response.nodestatus, &node_status);

	board_get_hardware_version(&response.hardware_version);
	response.name_length = board_get_product_name(response.name, sizeof(response.name));
	memset(&response.software_version, 0, sizeof(response.software_version));

	if (bootloader.app_valid) {
		response.software_version.major =
			bootloader.fw_image_descriptor->major_version;
		response.software_version.minor =
			bootloader.fw_image_descriptor->minor_version;
		response.software_version.optional_field_mask = 3u;
		bootloader.fw_image_descriptor->minor_version;
		response.software_version.vcs_commit =
			bootloader.fw_image_descriptor->vcs_commit;
		response.software_version.image_crc =
			bootloader.fw_image_descriptor->image_crc;
	}

	rx_message_id = 0u;

	if (can_rx(&rx_message_id, &frame_len, frame_payload, fifoGetNodeInfo) &&
	    uavcan_parse_frame_id(&frame_id, rx_message_id, UAVCAN_GETNODEINFO_DTID) &&
	    frame_payload[0] == bootloader.node_id &&
	    frame_id.last_frame) {
		uavcan_tx_getnodeinfo_response(bootloader.node_id, &response,
					       frame_id.source_node_id,
					       frame_id.transfer_id);

		bootloader.sent_node_info_response = true;
	}
}

/****************************************************************************
 * Name: node_status_process
 *
 * Description:
 *   Run as a timer callback off the system tick ISR.
 *   Once we have a node id, this process is started and sends the
 *   NodeStatus messages.  This function uses the MBNodeStatus
 *   of the CAN so it can coexist regardless of application state ant
 *   what is it doing.
 *
 * Input Parameters:
 *   Not Used.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void node_status_process(bl_timer_id id, void *context)
{
	uavcan_tx_nodestatus(bootloader.node_id, bootloader.uptime, bootloader.status_code);
}

/****************************************************************************
 * Name: find_descriptor
 *
 * Description:
 *   This functions looks through the application image in flash on 8 byte
 *   aligned boundaries to find the Application firmware descriptor.
 *   Once it is found the bootloader.fw_image_descriptor is set to point to
 *   it.
 *
 *
 * Input Parameters:
 *   None
 *
 * Returned State:
 *   If found bootloader.fw_image_descriptor points to the app_descriptor_t
 *   of the application firmware image.
 *   If not found bootloader.fw_image_descriptor = NULL
 *
 ****************************************************************************/

static void find_descriptor(void)
{
	uint64_t *p = (uint64_t *)APPLICATION_LOAD_ADDRESS;
	app_descriptor_t *descriptor = NULL;
	union {
		uint64_t ull;
		char text[sizeof(uint64_t)];
	} sig = {
		.text = {APP_DESCRIPTOR_SIGNATURE}
	};

	do {
		if (*p == sig.ull) {
			descriptor = (app_descriptor_t *)p;
			break;
		}
	} while (++p < APPLICATION_LAST_64BIT_ADDRRESS);

	bootloader.fw_image_descriptor = (volatile app_descriptor_t *)descriptor;
}

/****************************************************************************
 * Name: is_app_valid
 *
 * Description:
 *   This functions validates the applications image based on the validity of
 *   the Application firmware descriptor's crc and the value of the first word
 *   in the FLASH image.
 *
 *
 * Input Parameters:
 *   first_word - the value read from the first word of the Application's
 *   in FLASH image.
 *
 * Returned Value:
 *   true if the application in flash is valid., false otherwise.
 *
 ****************************************************************************/

static bool is_app_valid(uint32_t first_word)
{
	uint64_t crc;
	size_t i, length, crc_offset;
	uint32_t word;

	find_descriptor();

	if (!bootloader.fw_image_descriptor || first_word == 0xFFFFFFFFu) {
		return false;
	}

	length = bootloader.fw_image_descriptor->image_size;

	if (length > APPLICATION_SIZE) {
		return false;
	}

	crc_offset = (size_t)(&bootloader.fw_image_descriptor->image_crc) -
		     (size_t) bootloader.fw_image;
	crc_offset >>= 2u;
	length >>= 2u;

	crc = crc64_add_word(CRC64_INITIAL, first_word);

	for (i = 1u; i < length; i++) {
		if (i == crc_offset || i == crc_offset + 1u) {
			/* Zero out the CRC field while computing the CRC */
			word = 0u;

		} else {
			word = bootloader.fw_image[i];
		}

		crc = crc64_add_word(crc, word);
	}

	crc ^= CRC64_OUTPUT_XOR;

	return crc == bootloader.fw_image_descriptor->image_crc;
}

/****************************************************************************
 * Name: get_dynamic_node_id
 *
 * Description:
 *   This functions performs the client side of the uavcan specification
 *   for dynamic node allocation.
 *
 * Input Parameters:
 *   tboot             - The id of the timer to use at the tBoot timeout.
 *   allocated_node_id - A pointer to the location that should receive the
 *                       allocated node id.
 * Returned Value:
 *   CAN_OK            - Indicates the a node id has been allocated to this
 *                       node.
 *   CAN_BOOT_TIMEOUT - Indicates that tboot expired before an allocation
 *                      was done.
 *
 ****************************************************************************/

static int get_dynamic_node_id(bl_timer_id tboot, uint32_t *allocated_node_id)
{
	uavcan_hardwareversion_t hw_version;
	uavcan_frame_id_t rx_frame_id;

	uint32_t rx_message_id;
	uint8_t  rx_payload[8];
	size_t   rx_len;
	uint16_t rx_crc;

	struct {
		uint8_t node_id;
		uint8_t transfer_id;
		uint8_t allocated_node_id;
		uint8_t frame_index;
	} server;

	*allocated_node_id     = 0;

	uint8_t unique_id_matched;

	size_t i;
	size_t offset;
	uint16_t expected_crc;

	board_get_hardware_version(&hw_version);

	memset(&server, 0, sizeof(server));
	rx_crc = 0u;
	unique_id_matched = 0u;

	/*
	Rule A: on initialization, the client subscribes to
	uavcan.protocol.dynamic_node_id.Allocation and starts a Request Timer with
	interval of Trequestinterval = 1 second
	*/

	bl_timer_id trequest = timer_allocate(modeTimeout | modeStarted, 1000, 0);

	do {
		/*
		Rule B. On expiration of Request Timer:
		1) Request Timer restarts.
		2) The client broadcasts a first-stage Allocation request message,
		   where the fields are assigned following values:
		        node_id                 - preferred node ID, or zero if the
		                                  client doesn't have any preference
		        first_part_of_unique_id - true
		        unique_id               - first 7 bytes of unique ID
		*/
		if (timer_expired(trequest)) {
			uavcan_tx_allocation_message(*allocated_node_id, 16u, hw_version.unique_id, 0u);
			timer_start(trequest);
		}

		if (can_rx(&rx_message_id, &rx_len, rx_payload, fifoAll) &&
		    uavcan_parse_frame_id(&rx_frame_id, rx_message_id, UAVCAN_DYNAMICNODEIDALLOCATION_DTID)) {

			/*
			Rule C. On any Allocation message, even if other rules also match:
			1) Request Timer restarts.
			*/

			timer_start(trequest);

			/*
			Skip this message if it's anonymous (from another client), or if it's
			from a different server to the one we're listening to at the moment
			*/
			if (!rx_frame_id.source_node_id ||
			    rx_frame_id.priority == PRIORITY_SERVICE ||
			    (server.node_id && rx_frame_id.source_node_id != server.node_id)) {
				continue;

			} else if (!server.node_id ||
				   rx_frame_id.transfer_id != server.transfer_id ||
				   rx_frame_id.frame_index == 0) {
				/* First (only?) frame of the transfer */
				if (rx_frame_id.last_frame) {
					offset = 1u;

				} else {
					rx_crc = (uint16_t)(rx_payload[0] | (rx_payload[1] << 8u));
					server.allocated_node_id = rx_payload[2];
					offset = 3u;
				}

				server.node_id = rx_frame_id.source_node_id;
				server.transfer_id = rx_frame_id.transfer_id;
				unique_id_matched = 0u;
				server.frame_index = 1u;

			} else if (rx_frame_id.frame_index != server.frame_index) {
				/* Abort if the frame index is wrong */
				server.node_id = 0u;
				continue;

			} else {
				offset = 0u;
				server.frame_index++;
			}

			/*
			Rule D. On an Allocation message WHERE (source node ID is
			non-anonymous) AND (client's unique ID starts with the bytes available
			in the field unique_id) AND (unique_id is less than 16 bytes long):
			1) The client broadcasts a second-stage Allocation request message,
			   where the fields are assigned following values:
			        node_id                 - same value as in the first-stage
			        first_part_of_unique_id - false
			        unique_id               - at most 7 bytes of local unique ID
			                                  with an offset equal to number of
			                                  bytes in the received unique ID

			Rule E. On an Allocation message WHERE (source node ID is
			non-anonymous) AND (unique_id fully matches client's unique ID) AND
			(node_id in the received message is not zero):
			1) Request Timer stops.
			2) The client initializes its node_id with the received value.
			3) The client terminates subscription to Allocation messages.
			4) Exit.
			*/

			/* Count the number of unique ID bytes matched */
			for (i = offset; i < rx_len && unique_id_matched < 16u &&
			     hw_version.unique_id[unique_id_matched] == rx_payload[i];
			     unique_id_matched++, i++);

			if (i < rx_len) {
				/* Abort if we didn't match the whole unique ID */
				server.node_id = 0u;

			} else if (rx_frame_id.last_frame && unique_id_matched < 16u) {
				/* Case D */
				uavcan_tx_allocation_message(*allocated_node_id, 16u, hw_version.unique_id,
							     unique_id_matched);

			} else if (rx_frame_id.last_frame) {
				/* Validate CRC */
				expected_crc = UAVCAN_ALLOCATION_CRC;
				expected_crc = crc16_add(expected_crc, server.allocated_node_id);
				expected_crc = crc16_signature(expected_crc, 16u,
							       hw_version.unique_id);

				/* Case E */
				if (rx_crc == expected_crc) {
					*allocated_node_id = server.allocated_node_id >> 1u;
					break;
				}
			}
		}
	} while (!timer_expired(tboot));

	timer_free(trequest);
	return *allocated_node_id == 0 ? CAN_BOOT_TIMEOUT : CAN_OK;
}

/****************************************************************************
 * Name: wait_for_beginfirmwareupdate
 *
 * Description:
 *   This functions performs the client side of the uavcan specification
 *   for begin firmware update.
 *
 * Input Parameters:
 *   tboot              - The id of the timer to use at the tBoot timeout.
 *   fw_path            - A pointer to the location that should receive the
 *                        path of the firmware file to read.
 *   fw_path_length    -  A pointer to return the path length in.
 *   fw_source_node_id -  A pointer to return the node id of the node to
 *                        read the file from.
 * Returned Value:
 *   CAN_OK            - Indicates the a beginfirmwareupdate was received and
 *                       processed successful.
 *   CAN_BOOT_TIMEOUT - Indicates that tboot expired before a
 *                      beginfirmwareupdate was received.
 *
 ****************************************************************************/

static int wait_for_beginfirmwareupdate(bl_timer_id tboot, uint8_t *fw_path,
					uint8_t *fw_path_length,
					uint8_t *fw_source_node_id)
{
	uavcan_beginfirmwareupdate_request_t request;
	uavcan_frame_id_t frame_id;
	size_t i;
	uint8_t frame_payload[8];
	can_error_t status;

	status = CAN_ERROR;
	fw_path[0] = 0;
	*fw_source_node_id = 0;

	while (status != CAN_OK) {
		if (timer_expired(tboot)) {
			return CAN_BOOT_TIMEOUT;
		}

		status = uavcan_rx_beginfirmwareupdate_request(bootloader.node_id,
				&request, &frame_id);
	}

	if (status == CAN_OK) {
		/* UAVCANBootloader_v0.3 #22.2: Resp580.BeginFirmwareUpdate.uavcan */

		/* Send an ERROR_OK response */
		frame_payload[0] = frame_id.source_node_id;
		frame_payload[1] = 0u;

		frame_id.last_frame = 1u;
		frame_id.frame_index = 0u;
		frame_id.source_node_id = bootloader.node_id;
		frame_id.request_not_response = 0u;

		can_tx(uavcan_make_service_frame_id(&frame_id), 2u, frame_payload, MBAll);

		/* UAVCANBootloader_v0.3 #22.3: fwPath = image_file_remote_path */
		for (i = 0u; i < request.path_length; i++) {
			fw_path[i] = request.path[i];
		}

		*fw_path_length = request.path_length;
		/* UAVCANBootloader_v0.3 #22.4: fwSourceNodeID = source_node_id */
		*fw_source_node_id = request.source_node_id;
	}

	return status;
}

/****************************************************************************
 * Name: file_getinfo
 *
 * Description:
 *   This functions performs the client side of the uavcan specification
 *   for getinfo (for a file).
 *
 * Input Parameters:
 *   fw_image_size     - A pointer to the location that should receive the
 *                       firmware image size.
 *   fw_image_crc      - A pointer to the location that should receive the
 *                       firmware image crc.
 *   fw_path           - A pointer to the path of the firmware file that's
 *                       info is being requested.
 *   fw_path_length    - The path length of the firmware file that's
 *                       info is being requested.
 *   fw_source_node_id - The node id of the node to get the info from.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void file_getinfo(size_t *fw_image_size, uint64_t *fw_image_crc,
			 const uint8_t *fw_path, uint8_t fw_path_length,
			 uint8_t fw_source_node_id)
{
	uavcan_getinfo_request_t request;
	uavcan_getinfo_response_t response;
	uint8_t transfer_id, retries, i;
	can_error_t status;

	for (i = 0; i < fw_path_length; i++) {
		request.path[i] = fw_path[i];
	}

	request.path_length = fw_path_length;

	/* UAVCANBootloader_v0.3 #25: SetRetryAndTimeout(3,1000MS) */
	retries = UAVCAN_SERVICE_RETRIES;
	transfer_id = 0;

	*fw_image_size = 0;
	*fw_image_crc = 0;

	while (retries) {
		/* UAVCANBootloader_v0.3 #26: 585.GetInfo.uavcan(path) */
		uavcan_tx_getinfo_request(bootloader.node_id, &request,
					  fw_source_node_id, transfer_id);

		/* UAVCANBootloader_v0.3 #28:
		 * 585.GetInfo.uavcan(fw_path,fw_crc,fw_size...), */
		status =
			uavcan_rx_getinfo_response(bootloader.node_id, &response,
						   fw_source_node_id, transfer_id,
						   UAVCAN_SERVICE_TIMEOUT_MS);

		transfer_id++;

		/* UAVCANBootloader_v0.3 #27: validateFileInfo(file_info, &errorcode) */
		if (status == CAN_OK && response.error == UAVCAN_FILE_ERROR_OK &&
		    (response.entry_type & UAVCAN_GETINFO_ENTRY_TYPE_FLAG_FILE) &&
		    (response.entry_type & UAVCAN_GETINFO_ENTRY_TYPE_FLAG_READABLE) &&
		    response.size > 0u && response.size < OPT_APPLICATION_IMAGE_LENGTH) {
			/* UAVCANBootloader_v0.3 #28.4: save(file_info) */
			*fw_image_size = response.size;
			*fw_image_crc = response.crc64;
			break;

		} else {
			retries--;
		}
	}
}

/****************************************************************************
 * Name: file_read_and_program
 *
 * Description:
 *   This functions performs the client side of the uavcan specification
 *   for file read and programs the flash.
 *
 * Input Parameters:
 *   fw_source_node_id - The node id of the node to read the file from.
 *   fw_path_length    - The path length of the firmware file that is
 *                       being read.
 *   fw_path           - A pointer to the path of the firmware file that
 *                       is being read.
 *   fw_image_size     - The size the fw image file should be.
 *
 * Returned Value:
 *   FLASH_OK          - Indicates that the correct amount of data has
 *                       been programmed to the FLASH.
 *                       processed successful.
 *   FLASH_ERROR       - Indicates that an error occurred
 *
 * From Read 218.Read.uavcan updated 5/16/2015
 *
 *      There are two possible outcomes of a successful service call:
 *        1. Data array size equals its capacity. This means that the
 *           end of the file is not reached yet.
 *        2. Data array size is less than its capacity, possibly zero. This
 *           means that the end of file is reached.
 *
 *       Thus, if the client needs to fetch the entire file, it should
 *       repeatedly call this service while increasing the offset,
 *       until incomplete data is returned.
 *
 *       If the object pointed by 'path' cannot be read (e.g. it is a
 *       directory or it does not exist), appropriate error code
 *       will be returned, and data array will be empty.
 *
 ****************************************************************************/

static flash_error_t file_read_and_program(uint8_t fw_source_node_id,
		uint8_t fw_path_length,
		const uint8_t *fw_path,
		size_t fw_image_size)
{
	uavcan_read_request_t request;
	uavcan_read_response_t response;
	can_error_t can_status;
	flash_error_t flash_status;
	uint8_t transfer_id;
	uint8_t retries;
	uint8_t *data;
	uint32_t flash_address = (uint32_t) bootloader.fw_image;


	/* Set up the read request */
	memcpy(&request.path, fw_path, fw_path_length);

	request.path_length = fw_path_length;
	request.offset = 0;

	transfer_id = 0;

	/*
	 * Rate limiting on read requests:
	 *
	 *  2/sec (500  ms) on a 125 Kbaud bus Speed = 1 1000/2
	 *  4/sec (250  ms) on a 250 Kbaud bus Speed = 2 1000/4
	 *  8/sec (125  ms) on a 500 Kbaud bus Speed = 3 1000/8
	 * 16/sec (26.5 ms) on a 1 Mbaud bus   Speed = 4 1000/16
	 */

	uint32_t read_ms = 1000 >> bootloader.bus_speed;

	bl_timer_id tread = timer_allocate(modeTimeout | modeStarted, read_ms, 0);

	do {
		/* reset the rate limit */
		retries = UAVCAN_SERVICE_RETRIES;
		can_status = CAN_ERROR;

		while (retries && can_status != CAN_OK) {
			timer_restart(tread, read_ms);

			uavcan_tx_read_request(bootloader.node_id, &request,
					       fw_source_node_id, transfer_id);

			can_status = uavcan_rx_read_response(bootloader.node_id,
							     &response, fw_source_node_id,
							     transfer_id,
							     UAVCAN_SERVICE_TIMEOUT_MS);

			transfer_id++;

			if (can_status != CAN_OK || response.error != UAVCAN_FILE_ERROR_OK) {
				can_status = CAN_ERROR;

				retries--;

				board_indicate(fw_update_invalid_response);

				uavcan_tx_log_message(bootloader.node_id,
						      UAVCAN_LOGMESSAGE_LEVEL_ERROR,
						      UAVCAN_LOGMESSAGE_STAGE_PROGRAM,
						      UAVCAN_LOGMESSAGE_RESULT_FAIL);

			}
		}

		/* Exhausted retries */

		if (can_status != CAN_OK) {
			board_indicate(fw_update_timeout);
			break;
		}

		data = response.data;


		/*
		 * STM32 flash addresses  must be word aligned If the packet is the
		 * last and an odd length add an 0xff but do not count it in length
		 * The is OK to do because the uavcan Read will fill all data payloads
		 * until the last one
		 */

		if (response.data_length & 1) {
			data[response.data_length] = 0xff;
		}

		/* Save the first word off */

		if (request.offset == 0u) {
			bootloader.fw_word0.l = *(uint32_t *)data;
			*(uint32_t *)data = 0xffffffff;
		}

		flash_status = bl_flash_write(flash_address + request.offset ,
					      data,
					      response.data_length +
					      (response.data_length & 1));

		request.offset  += response.data_length;

		/* rate limit */

		while (!timer_expired(tread)) {
			;
		}

	} while (request.offset < fw_image_size &&
		 response.data_length == sizeof(response.data)  &&
		 flash_status == FLASH_OK);

	timer_free(tread);

	/*
	 * Return success if the last read succeeded, the last write succeeded, the
	 * correct number of bytes were written, and the length of the last response
	 * was not. */
	if (can_status == CAN_OK &&
	    flash_status == FLASH_OK &&
	    request.offset == fw_image_size &&
	    response.data_length != 0) {
		return FLASH_OK;

	} else {
		return FLASH_ERROR;
	}
}

/****************************************************************************
 * Name: do_jump
 *
 * Description:
 *   This functions begins the execution of the application firmware.
 *
 * Input Parameters:
 *   stacktop   - The value that should be loaded into the stack pointer.
 *   entrypoint - The address to jump to.
 *
 * Returned Value:
 *    Does not return.
 *
 ****************************************************************************/

static void do_jump(uint32_t stacktop, uint32_t entrypoint)
{
	asm volatile("msr msp, %[stacktop]    \n"
		     "bx     %[entrypoint]      \n"
		     :: [stacktop] "r"(stacktop), [entrypoint] "r"(entrypoint):);

	// just to keep noreturn happy
	for (;;);
}

/****************************************************************************
 * Name: application_run
 *
 * Description:
 *   This functions will test the application image is valid by
 *   checking the value of the first word for != 0xffffffff and the
 *   second word for an address that lies inside od the application
 *   fw image in flash.
 *
 * Input Parameters:
 *   fw_image_size   - The size the fw image is.
 *
 * Returned Value:
 *    If the Image is valid this code does not return, but runs the application
 *    via do_jump.
 *    If the image is invalid the function returns.
 *
 ****************************************************************************/

static void application_run(size_t fw_image_size, bootloader_app_shared_t *common)
{
	/*
	 * We refuse to program the first word of the app until the upload is marked
	 * complete by the host.  So if it's not 0xffffffff, we should try booting it.

	 * The second word of the app is the entrypoint; it must point within the
	 * flash area (or we have a bad flash).
	 */
	uint32_t fw_image[2] = {bootloader.fw_image[0], bootloader.fw_image[1]};

	if (fw_image[0] != 0xffffffff
	    && fw_image[1] > APPLICATION_LOAD_ADDRESS
	    && fw_image[1] < (APPLICATION_LOAD_ADDRESS + fw_image_size)) {

		irqdisable();

		stm32_boarddeinitialize();

		/* kill the systick interrupt */
		putreg32(0, NVIC_SYSTICK_CTRL);

		/* and set a specific LED pattern */
		board_indicate(jump_to_app);

		/* Update the shared memory and make it valid to tell the
		 * App are node ID and Can bit rate.
		 */

		if (common->crc.valid) {
			bootloader_app_shared_write(common, BootLoader);
		}


		/* the interface */

		/* switch exception handlers to the application */
		__asm volatile("dsb");
		__asm volatile("isb");
		putreg32(APPLICATION_LOAD_ADDRESS, NVIC_VECTAB);
		__asm volatile("dsb");
		/* extract the stack and entrypoint from the app vector table and go */
		do_jump(fw_image[0], fw_image[1]);
	}
}

/****************************************************************************
 * Name: autobaud_and_get_dynamic_node_id
 *
 * Description:
 *   This helper functions wraps the auto baud and node id allocation
 *
 * Input Parameters:
 *   tboot   - The id of the timer to use at the tBoot timeout.
 *   speed   - A pointer to the location to receive the speed detected by
 *             the auto baud function.
 *   node_id - A pointer to the location to receive the allocated node_id
 *             from the dynamic node allocation.
 *
 * Returned Value:
 *   CAN_OK - on Success or a CAN_BOOT_TIMEOUT
 *
 ****************************************************************************/

static int autobaud_and_get_dynamic_node_id(bl_timer_id tboot,
		can_speed_t *speed,
		uint32_t *node_id)
{

	board_indicate(autobaud_start);

	int rv = can_autobaud(speed, tboot);

	if (rv != CAN_BOOT_TIMEOUT) {
		board_indicate(autobaud_end);
		board_indicate(allocation_start);
		rv = get_dynamic_node_id(tboot, node_id);

		if (rv != CAN_BOOT_TIMEOUT) {
			board_indicate(allocation_end);
		}
	}

	return rv;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 *
 * Description:
 *   Called by the os_start code
 *
 * Input Parameters:
 *   Not used.
 *
 * Returned Value:
 *   Does not return.
 *
 ****************************************************************************/

__EXPORT int main(int argc, char *argv[])
{

	uint64_t fw_image_crc;
	size_t fw_image_size = 0;
	uint8_t fw_path[200];
	uint8_t fw_path_length;
	uint8_t fw_source_node_id;
	uint8_t error_log_stage;
	flash_error_t status;
	bootloader_app_shared_t common;

	start_the_watch_dog();

	/* Begin with all data zeroed */

	memset((void *)&bootloader, 0, sizeof(bootloader));

	bootloader.status_code = UAVCAN_NODESTATUS_STATUS_INITIALIZING;

	error_log_stage = UAVCAN_LOGMESSAGE_STAGE_INIT;

	bootloader.fw_image = (volatile uint32_t *)(APPLICATION_LOAD_ADDRESS);

	/*
	 *  This Option is set to 1 ensure a provider of firmware has an
	 *  opportunity update the node's firmware.
	 *  This Option is the default policy and can be overridden by
	 *  a jumper
	 *  When this Policy is set, the node will ignore tboot and
	 *  wait indefinitely for a GetNodeInfo request before booting.
	 *
	 *  OPT_WAIT_FOR_GETNODEINFO_JUMPER_GPIO_INVERT is used to allow
	 *  the polarity of the jumper to be True Active
	 *
	 *  wait  OPT_WAIT_FOR_GETNODEINFO  OPT_WAIT_FOR_GETNODEINFO_JUMPER_GPIO
	 *                                                 Jumper
	 *   yes           1                       0         x
	 *   yes           1                       1       Active
	 *   no            1                       1       Not Active
	 *   no            0                       0         X
	 *   yes           0                       1       Active
	 *   no            0                       1       Not Active
	 */
	bootloader.wait_for_getnodeinfo = OPT_WAIT_FOR_GETNODEINFO;

	/*
	 *  This Option allows the compile timed option to be overridden
	 *  by the state of the jumper
	 *
	 */
#if defined(OPT_WAIT_FOR_GETNODEINFO_JUMPER_GPIO)
	bootloader.wait_for_getnodeinfo = (stm32_gpioread(GPIO_GETNODEINFO_JUMPER) ^
					   OPT_WAIT_FOR_GETNODEINFO_JUMPER_GPIO_INVERT);
#endif

	/* Is the memory in the Application space occupied by a valid application? */

	bootloader.app_valid = is_app_valid(bootloader.fw_image[0]);

	board_indicate(reset);

	/* Was this boot a result of the Application being told it has a FW update ? */

	bootloader.app_bl_request = (OK == bootloader_app_shared_read(&common, App)) &&
				    common.bus_speed && common.node_id;

	/*
	 * Mark CRC to say this is not from
	 * auto baud and Node Allocation
	 */
	common.crc.valid = false;

	/* Either way prevent Deja vu by invalidating the struct*/

	bootloader_app_shared_invalidate();

	/* Set up the Timers */

	bl_timer_cb_t p = null_cb;
	p.cb = uptime_process;

	/* Uptime is always on*/

	timer_allocate(modeRepeating | modeStarted, 1000, &p);

	/*
	 * NodeInfo is a controlled process that will be run once a node Id is
	 * established to process the received NodeInfo response.
	 */

	p.cb = node_info_process;
	bl_timer_id tinfo = timer_allocate(modeRepeating, OPT_NODE_INFO_RATE_MS, &p);

	/*
	 * NodeStatus is a controlled process that will sent the requisite NodeStatus
	 * once a node Id is established
	 */

	p.cb = node_status_process;
	bl_timer_id tstatus = timer_allocate(modeRepeating, OPT_NODE_STATUS_RATE_MS, &p);


	/*
	 * tBoot is a controlled timer, that is run to allow the application, if
	 * valid to be booted. tBoot is gated on by the application being valid
	 * and the OPT_WAIT_FOR_GETNODEINFOxxxx settings
	 *
	 */

	bl_timer_id tboot = timer_allocate(modeTimeout, OPT_TBOOT_MS, 0);

	/*
	 * If the Application is Valid and we are not in a reboot from the
	 * running Application requesting to Bootload or we are not configured
	 * to wait for NodeStatus then start tBoot
	 */
	if (bootloader.app_valid && !bootloader.wait_for_getnodeinfo && !bootloader.app_bl_request) {
		timer_start(tboot);
	}

	/*
	 * If this is a reboot from the running Application requesting to Bootload
	 * then use the CAN parameters supplied by the running Application to init
	 * the CAN device.
	 */

	if (bootloader.app_bl_request) {

		bootloader.bus_speed = common.bus_speed;
		bootloader.node_id = (uint8_t) common.node_id;
		can_init(can_freq2speed(common.bus_speed), CAN_Mode_Normal);

	} else {

		/*
		 * It is a regular boot, So we need to autobaud and get a node ID
		 * If the tBoot was started, we will boot normal if the auto baud
		 * or the Node allocation runs longer the tBoot
		 */


		if (CAN_OK != autobaud_and_get_dynamic_node_id(tboot, (can_speed_t *)&bootloader.bus_speed, &common.node_id)) {
			goto boot;
		}

		/* We have autobauded and got a Node ID. So reset uptime
		 * and save the speed and node_id in both the common and
		 * and bootloader data sets
		 */

		bootloader.uptime = 0;
		common.bus_speed = can_speed2freq(bootloader.bus_speed);
		bootloader.node_id = (uint8_t) common.node_id;

		/*
		 * Mark CRC to say this is from
		 * auto baud and Node Allocation
		 */
		common.crc.valid = true;

	}

	/* Now start the processes that were defendant on a node ID */

	timer_start(tinfo);
	timer_start(tstatus);

	/*
	 * Now since we are sending NodeStatus messages the Node Status monitor on the
	 * bus will see us as a new node or one whose uptime has gone backwards
	 * So we wait for the NodeInfoRequest and our response to be sent of if
	 * tBoot is running a time tBoot out.
	 */

	while (!bootloader.sent_node_info_response) {
		if (timer_expired(tboot)) {
			goto boot;
		}
	}

	/*
	 * Now we have seen and responded to the NodeInfoRequest
	 * If we have a Valid App and we are processing and the running App's
	 * re-boot to bootload request or we are configured to wait
	 * we start the tBoot time out.
	 *
	 * This effectively extends the time the FirmwareServer has to tell us
	 * to Begin a FW update. To tBoot from NodeInfoRequest as opposed to
	 * tBoot from Reset.
	 */


	if (bootloader.app_valid &&
	    (bootloader.wait_for_getnodeinfo ||
	     bootloader.app_bl_request)) {

		timer_start(tboot);
	}

	/*
	 * Now We wait up to tBoot for a begin firmware update from the FirmwareServer
	 * on the bus. tBoot will only be running if the app is valid.
	 */

	do {
		if (CAN_BOOT_TIMEOUT == wait_for_beginfirmwareupdate(tboot, fw_path,
				&fw_path_length,
				&fw_source_node_id)) {
			goto boot;
		}
	} while (!fw_source_node_id);


	/* We received a begin firmware update */

	timer_stop(tboot);
	board_indicate(fw_update_start);

	file_getinfo(&fw_image_size, &fw_image_crc, fw_path, fw_path_length,
		     fw_source_node_id);

	//todo:Check this
	if (fw_image_size < sizeof(app_descriptor_t) || !fw_image_crc) {
		error_log_stage = UAVCAN_LOGMESSAGE_STAGE_GET_INFO;
		goto failure;
	}

	/* LogMessage the Erase  */

	uavcan_tx_log_message(bootloader.node_id,
			      UAVCAN_LOGMESSAGE_LEVEL_INFO,
			      UAVCAN_LOGMESSAGE_STAGE_ERASE,
			      UAVCAN_LOGMESSAGE_RESULT_START);


	/* Need to signal that the app is no longer valid  if Node Info Request are done */

	bootloader.app_valid = false;

	status = bl_flash_erase(APPLICATION_LOAD_ADDRESS, APPLICATION_SIZE);

	if (status != FLASH_OK) {
		/* UAVCANBootloader_v0.3 #28.8: [Erase
		 * Failed]:INDICATE_FW_UPDATE_ERASE_FAIL */
		board_indicate(fw_update_erase_fail);

		error_log_stage = UAVCAN_LOGMESSAGE_STAGE_ERASE;
		goto failure;
	}

	status = file_read_and_program(fw_source_node_id, fw_path_length, fw_path,
				       fw_image_size);

	if (status != FLASH_OK) {
		error_log_stage = UAVCAN_LOGMESSAGE_STAGE_PROGRAM;
		goto failure;
	}

	/* Did we program a valid imange ?*/

	if (!is_app_valid(bootloader.fw_word0.l)) {
		bootloader.app_valid = 0u;

		board_indicate(fw_update_invalid_crc);

		error_log_stage = UAVCAN_LOGMESSAGE_STAGE_VALIDATE;
		goto failure;
	}

	/* Yes Commit the first word to location 0 of the Application image in flash */

	status = bl_flash_write((uint32_t) bootloader.fw_image, (uint8_t *) &bootloader.fw_word0.b[0],
				sizeof(bootloader.fw_word0.b));

	if (status != FLASH_OK) {
		error_log_stage = UAVCAN_LOGMESSAGE_STAGE_FINALIZE;
		goto failure;
	}


	/* Send a completion log message */
	uavcan_tx_log_message(bootloader.node_id,
			      UAVCAN_LOGMESSAGE_LEVEL_INFO,
			      UAVCAN_LOGMESSAGE_STAGE_FINALIZE,
			      UAVCAN_LOGMESSAGE_RESULT_OK);



	/* Boot the application */

boot:

	kick_the_watch_dog();

	application_run(bootloader.fw_image_descriptor->image_size, &common);

	/* We will fall thru if the Image is bad */

failure:

	uavcan_tx_log_message(bootloader.node_id,
			      UAVCAN_LOGMESSAGE_LEVEL_ERROR,
			      error_log_stage, UAVCAN_LOGMESSAGE_RESULT_FAIL);


	bootloader.status_code = UAVCAN_NODESTATUS_STATUS_CRITICAL;

	bl_timer_id tmr = timer_allocate(modeTimeout | modeStarted , OPT_RESTART_TIMEOUT_MS, 0);

	while (!timer_expired(tmr)) {
		;
	}

	timer_free(tmr);
	up_systemreset();
}
