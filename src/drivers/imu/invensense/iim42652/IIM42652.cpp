/****************************************************************************
 *
 *   Copyright (c) 2022-2023 PX4 Development Team. All rights reserved.
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

#include "IIM42652.hpp"

#define PRINT_REGISTER(REG) printf("%s (0x%02hhX) = 0x%02hhX \n", #REG, static_cast<uint8_t>(REG), RegisterRead(REG))

using namespace time_literals;

static constexpr int16_t combine(uint8_t msb, uint8_t lsb)
{
	return (msb << 8u) | lsb;
}

IIM42652::IIM42652(const I2CSPIDriverConfig &config) :
	SPI(config),
	I2CSPIDriver(config),
	_drdy_gpio(config.drdy_gpio),
	_px4_accel(get_device_id(), config.rotation),
	_px4_gyro(get_device_id(), config.rotation)
{
	if (config.drdy_gpio != 0) {
		_drdy_missed_perf = perf_alloc(PC_COUNT, MODULE_NAME": DRDY missed");
	}

	if (config.custom1 != 0) {
		_enable_clock_input = true;
		_input_clock_freq = config.custom1;

	} else {
		_enable_clock_input = false;
	}

	ConfigureSampleRate(_px4_gyro.get_max_rate_hz());
}

IIM42652::~IIM42652()
{
	perf_free(_reset_perf);
	perf_free(_bad_register_perf);
	perf_free(_bad_transfer_perf);
	perf_free(_fifo_empty_perf);
	perf_free(_fifo_overflow_perf);
	perf_free(_fifo_reset_perf);
	perf_free(_fifo_timestamp_error_perf);
	perf_free(_drdy_missed_perf);
}

int IIM42652::init()
{
	int ret = SPI::init();

	if (ret != PX4_OK) {
		DEVICE_DEBUG("SPI::init failed (%i)", ret);
		return ret;
	}

	return Reset() ? 0 : -1;
}

bool IIM42652::Reset()
{
	_state = STATE::RESET;
	DataReadyInterruptDisable();
	ScheduleClear();
	ScheduleNow();
	return true;
}

void IIM42652::exit_and_cleanup()
{
	DataReadyInterruptDisable();
	I2CSPIDriverBase::exit_and_cleanup();
}

void IIM42652::print_status()
{
	I2CSPIDriverBase::print_status();

	PX4_INFO_RAW("FIFO empty interval: %d us (%.1f Hz)\n", _fifo_empty_interval_us, 1e6 / _fifo_empty_interval_us);

	if (_enable_clock_input) {
		PX4_INFO_RAW("Clock input: %.1f Hz\n", (double)_input_clock_freq);
	}

	perf_print_counter(_reset_perf);
	perf_print_counter(_bad_register_perf);
	perf_print_counter(_bad_transfer_perf);
	perf_print_counter(_fifo_empty_perf);
	perf_print_counter(_fifo_overflow_perf);
	perf_print_counter(_fifo_reset_perf);
	perf_print_counter(_fifo_timestamp_error_perf);
	perf_print_counter(_drdy_missed_perf);
}

int IIM42652::probe()
{
	// 3 retries
	for (int retry = 0; retry < 3; retry++) {
		uint8_t whoami = RegisterRead(Register::BANK_0::WHO_AM_I);

		if (whoami == WHOAMI) {
			return PX4_OK;

		} else {
			DEVICE_DEBUG("unexpected WHO_AM_I 0x%02x", whoami);

			uint8_t reg_bank_sel = RegisterRead(Register::BANK_0::REG_BANK_SEL);
			int bank = reg_bank_sel >> 4;

			if (bank >= 1 && bank <= 3) {
				DEVICE_DEBUG("incorrect register bank for WHO_AM_I REG_BANK_SEL:0x%02x, bank:%d", reg_bank_sel, bank);
				// force bank selection and retry
				SelectRegisterBank(REG_BANK_SEL_BIT::BANK_SEL_0, true);
			}
		}
	}

	return PX4_ERROR;
}

void IIM42652::RunImpl()
{
	const hrt_abstime now = hrt_absolute_time();

	switch (_state) {
	case STATE::RESET:
		perf_count(_reset_perf);
		// DEVICE_CONFIG: Software reset configuration
		RegisterWrite(Register::BANK_0::DEVICE_CONFIG, DEVICE_CONFIG_BIT::SOFT_RESET_CONFIG);
		_reset_timestamp = now;
		_failure_count = 0;
		_state = STATE::WAIT_FOR_RESET;

		ScheduleDelayed(2_ms); // wait at least 1 ms for soft reset to be effective
		break;

	case STATE::WAIT_FOR_RESET:
		if ((RegisterRead(Register::BANK_0::WHO_AM_I) == WHOAMI)
		    && (RegisterRead(Register::BANK_0::DEVICE_CONFIG) == 0x00)
		    && (RegisterRead(Register::BANK_0::INT_STATUS) & INT_STATUS_BIT::RESET_DONE_INT)) {

			// Wakeup accel and gyro and schedule remaining configuration
			RegisterWrite(Register::BANK_0::PWR_MGMT0, PWR_MGMT0_BIT::GYRO_MODE_LOW_NOISE | PWR_MGMT0_BIT::ACCEL_MODE_LOW_NOISE);
			_state = STATE::CONFIGURE;
			ScheduleDelayed(30_ms); // 30 ms gyro startup time, 10 ms accel from sleep to valid data

		} else {
			// RESET not complete
			if (hrt_elapsed_time(&_reset_timestamp) > 1000_ms) {
				PX4_DEBUG("Reset failed, retrying");
				_state = STATE::RESET;
				ScheduleDelayed(100_ms);

			} else {
				PX4_DEBUG("Reset not complete, check again in 100 ms");
				ScheduleDelayed(100_ms);
			}
		}

		break;

	case STATE::CONFIGURE:
		if (Configure()) {
			// if configure succeeded then reset the FIFO
			_state = STATE::FIFO_RESET;
			ScheduleDelayed(1_ms);

		} else {
			// CONFIGURE not complete
			if (hrt_elapsed_time(&_reset_timestamp) > 1000_ms) {
				PX4_DEBUG("Configure failed, resetting");
				_state = STATE::RESET;

			} else {
				PX4_DEBUG("Configure failed, retrying");
			}

			ScheduleDelayed(100_ms);
		}

		break;

	case STATE::FIFO_RESET:

		_state = STATE::FIFO_READ;
		FIFOReset();

		if (DataReadyInterruptConfigure()) {
			_data_ready_interrupt_enabled = true;

			// backup schedule as a watchdog timeout
			ScheduleDelayed(100_ms);

		} else {
			_data_ready_interrupt_enabled = false;
			ScheduleOnInterval(_fifo_empty_interval_us, _fifo_empty_interval_us);
		}

		break;

	case STATE::FIFO_READ: {
			hrt_abstime timestamp_sample = now;
			uint8_t samples = 0;

			if (_data_ready_interrupt_enabled) {
				// scheduled from interrupt if _drdy_timestamp_sample was set as expected
				const hrt_abstime drdy_timestamp_sample = _drdy_timestamp_sample.fetch_and(0);

				if (now < drdy_timestamp_sample + _fifo_empty_interval_us) {
					timestamp_sample = drdy_timestamp_sample;
					samples = _fifo_gyro_samples;

				} else {
					perf_count(_drdy_missed_perf);
				}

				// push backup schedule back
				ScheduleDelayed(_fifo_empty_interval_us * 2);
			}

			bool success = false;

			if (samples == 0) {
				// check current FIFO count
				const uint16_t fifo_count = FIFOReadCount();

				if (fifo_count >= FIFO::SIZE) {
					FIFOReset();
					perf_count(_fifo_overflow_perf);

				} else if (fifo_count == 0) {
					perf_count(_fifo_empty_perf);

				} else {
					// FIFO count (size in bytes) should be a multiple of the FIFO::DATA structure
					samples = fifo_count / sizeof(FIFO::DATA);

					if (samples > _fifo_gyro_samples) {
						// grab desired number of samples, but reschedule next cycle sooner
						const int extra_samples = samples - _fifo_gyro_samples;
						samples = _fifo_gyro_samples;

						if (_fifo_gyro_samples > extra_samples) {
							// reschedule to run when a total of _fifo_gyro_samples should be available in the FIFO
							const uint32_t reschedule_delay_us = (_fifo_gyro_samples - extra_samples) * static_cast<int>(FIFO_SAMPLE_DT);
							ScheduleOnInterval(_fifo_empty_interval_us, reschedule_delay_us);

						} else {
							// otherwise reschedule to run immediately
							ScheduleOnInterval(_fifo_empty_interval_us);
						}

					} else if (samples < _fifo_gyro_samples) {
						// reschedule next cycle to catch the desired number of samples
						ScheduleOnInterval(_fifo_empty_interval_us, (_fifo_gyro_samples - samples) * static_cast<int>(FIFO_SAMPLE_DT));
					}

					if (samples == _fifo_gyro_samples) {
						if (FIFORead(now, samples)) {
							success = true;

							if (_failure_count > 0) {
								_failure_count--;
							}
						}
					}
				}
			}

			if (samples == _fifo_gyro_samples) {
				if (FIFORead(timestamp_sample, samples)) {
					success = true;

					if (_failure_count > 0) {
						_failure_count--;
					}
				}
			}

			if (!success) {
				_failure_count++;

				// full reset if things are failing consistently
				if (_failure_count > 10) {
					PX4_DEBUG("Full reset because things are failing consistently");
					Reset();
					return;
				}
			}

			// check configuration registers periodically or immediately following any failure
			if (!success || hrt_elapsed_time(&_last_config_check_timestamp) > 100_ms) {
				if (RegisterCheck(_register_bank0_cfg[_checked_register_bank0])
				    && RegisterCheck(_register_bank1_cfg[_checked_register_bank1])
				    && RegisterCheck(_register_bank2_cfg[_checked_register_bank2])
				   ) {
					_last_config_check_timestamp = now;
					_checked_register_bank0 = (_checked_register_bank0 + 1) % size_register_bank0_cfg;
					_checked_register_bank1 = (_checked_register_bank1 + 1) % size_register_bank1_cfg;
					_checked_register_bank2 = (_checked_register_bank2 + 1) % size_register_bank2_cfg;

				} else {
					// register check failed, force reset
					perf_count(_bad_register_perf);
					Reset();
				}
			}
		}

		break;
	}
}

void IIM42652::ConfigureSampleRate(int sample_rate)
{
	// round down to nearest FIFO sample dt
	const float min_interval = FIFO_SAMPLE_DT;
	_fifo_empty_interval_us = math::max(roundf((1e6f / (float)sample_rate) / min_interval) * min_interval, min_interval);

	_fifo_gyro_samples = roundf(math::min((float)_fifo_empty_interval_us / (1e6f / RATE), (float)FIFO_MAX_SAMPLES));

	// recompute FIFO empty interval (us) with actual gyro sample limit
	_fifo_empty_interval_us = _fifo_gyro_samples * (1e6f / RATE);

	ConfigureFIFOWatermark(_fifo_gyro_samples);
}

void IIM42652::ConfigureFIFOWatermark(uint8_t samples)
{
	// FIFO watermark threshold in number of bytes
	const uint16_t fifo_watermark_threshold = samples * sizeof(FIFO::DATA);

	for (auto &r : _register_bank0_cfg) {
		switch (r.reg) {
		case Register::BANK_0::FIFO_CONFIG2:
			// FIFO_WM[7:0]  FIFO_CONFIG2
			r.set_bits = fifo_watermark_threshold & 0xFF;
			r.clear_bits = ~r.set_bits;
			break;

		case Register::BANK_0::FIFO_CONFIG3:
			// FIFO_WM[11:8] FIFO_CONFIG3
			r.set_bits = (fifo_watermark_threshold >> 8) & 0x0F;
			r.clear_bits = ~r.set_bits;
			break;

		default:
			break;
		}
	}
}

void IIM42652::ConfigureCLKIN()
{
	for (auto &r0 : _register_bank0_cfg) {
		if (r0.reg == Register::BANK_0::INTF_CONFIG1) {
			if (_enable_clock_input) {
				// RTC_MODE: 1: RTC clock input is required
				r0.set_bits = INTF_CONFIG1_BIT::RTC_MODE;
				r0.clear_bits = 0;

			} else {
				// RTC_MODE: 0: No input RTC clock is required
				r0.set_bits = 0;
				r0.clear_bits = INTF_CONFIG1_BIT::RTC_MODE;
			}
		}
	}

	for (auto &r1 : _register_bank1_cfg) {
		if (r1.reg == Register::BANK_1::INTF_CONFIG5) {
			if (_enable_clock_input) {
				// PIN9_FUNCTION CLKIN
				r1.set_bits   = INTF_CONFIG5_BIT::PIN9_FUNCTION_CLKIN_SET;
				r1.clear_bits = INTF_CONFIG5_BIT::PIN9_FUNCTION_CLKIN_CLEAR;

			} else {
				// PIN9_FUNCTION reset/clear
				r1.set_bits   = INTF_CONFIG5_BIT::PIN9_FUNCTION_RESET_SET;
				r1.clear_bits = INTF_CONFIG5_BIT::PIN9_FUNCTION_RESET_CLEAR;
			}
		}
	}
}

void IIM42652::SelectRegisterBank(enum REG_BANK_SEL_BIT bank, bool force)
{
	if (bank != _last_register_bank || force) {
		// select BANK_0
		uint8_t cmd_bank_sel[2] {};
		cmd_bank_sel[0] = static_cast<uint8_t>(Register::BANK_0::REG_BANK_SEL);
		cmd_bank_sel[1] = bank;
		transfer(cmd_bank_sel, cmd_bank_sel, sizeof(cmd_bank_sel));

		_last_register_bank = bank;
	}
}

bool IIM42652::Configure()
{
	ConfigureCLKIN();

	// first set and clear all configured register bits
	for (const auto &reg_cfg : _register_bank0_cfg) {
		RegisterSetAndClearBits(reg_cfg.reg, reg_cfg.set_bits, reg_cfg.clear_bits);
	}

	for (const auto &reg_cfg : _register_bank1_cfg) {
		RegisterSetAndClearBits(reg_cfg.reg, reg_cfg.set_bits, reg_cfg.clear_bits);
	}

	for (const auto &reg_cfg : _register_bank2_cfg) {
		RegisterSetAndClearBits(reg_cfg.reg, reg_cfg.set_bits, reg_cfg.clear_bits);
	}


	// now check that all are configured
	bool success = true;

	for (const auto &reg_cfg : _register_bank0_cfg) {
		if (!RegisterCheck(reg_cfg)) {
			success = false;
		}
	}

	for (const auto &reg_cfg : _register_bank1_cfg) {
		if (!RegisterCheck(reg_cfg)) {
			success = false;
		}
	}

	for (const auto &reg_cfg : _register_bank2_cfg) {
		if (!RegisterCheck(reg_cfg)) {
			success = false;
		}
	}

	return success;
}

int IIM42652::DataReadyInterruptCallback(int irq, void *context, void *arg)
{
	static_cast<IIM42652 *>(arg)->DataReady();
	return 0;
}

void IIM42652::DataReady()
{
	_drdy_timestamp_sample.store(hrt_absolute_time());
	ScheduleNow();
}

bool IIM42652::DataReadyInterruptConfigure()
{
	if (_drdy_gpio == 0) {
		return false;
	}

	// Setup data ready on falling edge
	return (px4_arch_gpiosetevent(_drdy_gpio, false, true, false, &DataReadyInterruptCallback, this) == 0);
}

bool IIM42652::DataReadyInterruptDisable()
{
	if (_drdy_gpio == 0) {
		return false;
	}

	return (px4_arch_gpiosetevent(_drdy_gpio, false, false, false, nullptr, nullptr) == 0);
}

template <typename T>
bool IIM42652::RegisterCheck(const T &reg_cfg)
{
	bool success = true;

	const uint8_t reg_value = RegisterRead(reg_cfg.reg);

	if (reg_cfg.set_bits && ((reg_value & reg_cfg.set_bits) != reg_cfg.set_bits)) {
		PX4_DEBUG("0x%02hhX: 0x%02hhX (0x%02hhX not set)", (uint8_t)reg_cfg.reg, reg_value, reg_cfg.set_bits);
		success = false;
	}

	if (reg_cfg.clear_bits && ((reg_value & reg_cfg.clear_bits) != 0)) {
		PX4_DEBUG("0x%02hhX: 0x%02hhX (0x%02hhX not cleared)", (uint8_t)reg_cfg.reg, reg_value, reg_cfg.clear_bits);
		success = false;
	}

	return success;
}

template <typename T>
uint8_t IIM42652::RegisterRead(T reg)
{
	uint8_t cmd[2] {};
	cmd[0] = static_cast<uint8_t>(reg) | DIR_READ;
	SelectRegisterBank(reg);
	transfer(cmd, cmd, sizeof(cmd));
	return cmd[1];
}

template <typename T>
void IIM42652::RegisterWrite(T reg, uint8_t value)
{
	uint8_t cmd[2] { (uint8_t)reg, value };
	SelectRegisterBank(reg);
	transfer(cmd, cmd, sizeof(cmd));
}

template <typename T>
void IIM42652::RegisterSetAndClearBits(T reg, uint8_t setbits, uint8_t clearbits)
{
	const uint8_t orig_val = RegisterRead(reg);

	uint8_t val = (orig_val & ~clearbits) | setbits;

	if (orig_val != val) {
		RegisterWrite(reg, val);
	}
}

uint16_t IIM42652::FIFOReadCount()
{
	// read FIFO count
	SelectRegisterBank(REG_BANK_SEL_BIT::BANK_SEL_0);

	// transfer buffer
	struct TransferBuffer {
		uint8_t cmd{static_cast<uint8_t>(Register::BANK_0::FIFO_COUNTH) | DIR_READ};
		uint8_t FIFO_COUNTH{0};
		uint8_t FIFO_COUNTL{0};
	} buffer{};

	// read FIFO count
	if (transfer((uint8_t *)&buffer, (uint8_t *)&buffer, sizeof(buffer)) != PX4_OK) {
		perf_count(_bad_transfer_perf);
		return 0;
	}

	return (buffer.FIFO_COUNTH << 8) + buffer.FIFO_COUNTL;
}

static constexpr int32_t reassemble_20bit(const uint32_t a, const uint32_t b, const uint32_t c)
{
	// 0xXXXAABBC
	uint32_t high   = ((a << 12) & 0x000FF000);
	uint32_t low    = ((b << 4)  & 0x00000FF0);
	uint32_t lowest = (c         & 0x0000000F);

	uint32_t x = high | low | lowest;

	if (a & Bit7) {
		// sign extend
		x |= 0xFFF00000u;
	}

	return static_cast<int32_t>(x);
}

bool IIM42652::FIFORead(const hrt_abstime &timestamp_sample, uint8_t samples)
{
	// FIFO transfer buffer
	struct FIFOTransferBuffer {
		uint8_t cmd{static_cast<uint8_t>(Register::BANK_0::INT_STATUS) | DIR_READ};
		uint8_t INT_STATUS{0};
		uint8_t FIFO_COUNTH{0};
		uint8_t FIFO_COUNTL{0};
		FIFO::DATA f[FIFO_MAX_SAMPLES] {};
	} buffer{};

	// cmd + INT_STATUS + FIFO_COUNTH + FIFO_COUNTL + samples (FIFO::DATA)
	const size_t transfer_size = 4 + math::min(samples * sizeof(FIFO::DATA), FIFO::SIZE);

	SelectRegisterBank(REG_BANK_SEL_BIT::BANK_SEL_0);

	if (transfer((uint8_t *)&buffer, (uint8_t *)&buffer, transfer_size) != PX4_OK) {
		perf_count(_bad_transfer_perf);
		return false;
	}

	if (buffer.INT_STATUS & INT_STATUS_BIT::FIFO_FULL_INT) {
		perf_count(_fifo_overflow_perf);
		FIFOReset();
		return false;
	}

	const uint16_t fifo_count_bytes = (buffer.FIFO_COUNTH << 8) + buffer.FIFO_COUNTL;

	if (fifo_count_bytes >= FIFO::SIZE) {
		perf_count(_fifo_overflow_perf);
		FIFOReset();
		return false;
	}

	const uint8_t fifo_count_samples = fifo_count_bytes / sizeof(FIFO::DATA);

	if (fifo_count_samples == 0) {
		perf_count(_fifo_empty_perf);
		return false;
	}

	sensor_accel_fifo_s sensor_accel_fifo{};
	sensor_accel_fifo.timestamp_sample = timestamp_sample;
	sensor_accel_fifo.device_id = get_device_id();
	sensor_accel_fifo.dt = FIFO_SAMPLE_DT;
	sensor_accel_fifo.scale = CONSTANTS_ONE_G / 8192.f; // highres accel data 8192 LSB/g

	sensor_gyro_fifo_s sensor_gyro_fifo{};
	sensor_gyro_fifo.timestamp_sample = timestamp_sample;
	sensor_gyro_fifo.device_id = get_device_id();
	sensor_gyro_fifo.dt = FIFO_SAMPLE_DT;
	sensor_gyro_fifo.scale = math::radians(1.f / 131.f); // highres gyro data 131 LSB/dps

	// check FIFO header in every sample
	uint8_t valid_samples = 0;
	float temperature_sum = 0;
	float timestamp_interval_sum = 0;

	bool accel_scale_16bit = false; // 18-bits of accelerometer data
	bool gyro_scale_16bit = false; // 20-bits of gyroscope data

	for (int i = 0; i < math::min(samples, fifo_count_samples); i++) {
		const FIFO::DATA &fifo = buffer.f[i];

		// With FIFO_ACCEL_EN and FIFO_GYRO_EN header should be 8’b_0110_10xx
		const uint8_t FIFO_HEADER = fifo.FIFO_Header;

		const bool HEADER_MSG       = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_MSG; // FIFO is empty
		const bool HEADER_ACCEL     = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_ACCEL;
		const bool HEADER_GYRO      = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_GYRO;
		const bool HEADER_20        = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_20; // valid sample of extended 20-bit data
		// 3:2 HEADER_TIMESTAMP_FSYNC
		const bool HEADER_ODR_ACCEL = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_ODR_ACCEL; // ODR for accel is different
		const bool HEADER_ODR_GYRO  = FIFO_HEADER & FIFO::FIFO_HEADER_BIT::HEADER_ODR_GYRO;  // ODR for gyro is different

		if (!HEADER_MSG && HEADER_ACCEL && HEADER_GYRO && HEADER_20 && !HEADER_ODR_ACCEL && !HEADER_ODR_GYRO) {

			if (!accel_scale_16bit) {
				// Accel: 20 bit hires mode, Accel data is 18 bit
				// Sign extension + Accel [19:12] + Accel [11:4] + Accel [3:2] (20 bit extension byte)
				int32_t accel_x = reassemble_20bit(fifo.ACCEL_DATA_X1, fifo.ACCEL_DATA_X0, fifo.Ext_Accel_X_Gyro_X & 0xF0 >> 4);
				int32_t accel_y = reassemble_20bit(fifo.ACCEL_DATA_Y1, fifo.ACCEL_DATA_Y0, fifo.Ext_Accel_Y_Gyro_Y & 0xF0 >> 4);
				int32_t accel_z = reassemble_20bit(fifo.ACCEL_DATA_Z1, fifo.ACCEL_DATA_Z0, fifo.Ext_Accel_Z_Gyro_Z & 0xF0 >> 4);

				// sample invalid if -524288
				if (accel_x == -524288 || accel_y == -524288 || accel_z == -524288) {
					return false;
				}

				// shift by 2 (2 least significant bits are always 0)
				accel_x = accel_x / 4;
				accel_y = accel_y / 4;
				accel_z = accel_z / 4;

				// check if any values are going to exceed int16 limits
				if ((accel_x >= INT16_MAX || accel_x <= INT16_MIN)
				    || (accel_y >= INT16_MAX || accel_y <= INT16_MIN)
				    || (accel_z >= INT16_MAX || accel_z <= INT16_MIN)) {

					accel_scale_16bit = true;

					// 20 bit data scaled to 16 bit
					sensor_accel_fifo.scale = CONSTANTS_ONE_G / 2048.f;

					// rescale any existing data
					for (int j = 0; j < valid_samples + 1; j++) {
						sensor_accel_fifo.x[valid_samples] = combine(buffer.f[j].ACCEL_DATA_X1, buffer.f[j].ACCEL_DATA_X0);
						sensor_accel_fifo.y[valid_samples] = combine(buffer.f[j].ACCEL_DATA_Y1, buffer.f[j].ACCEL_DATA_Y0);
						sensor_accel_fifo.z[valid_samples] = combine(buffer.f[j].ACCEL_DATA_Z1, buffer.f[j].ACCEL_DATA_Z0);
					}

				} else {
					sensor_accel_fifo.x[valid_samples] = accel_x;
					sensor_accel_fifo.y[valid_samples] = accel_y;
					sensor_accel_fifo.z[valid_samples] = accel_z;
				}

			} else {
				sensor_accel_fifo.x[valid_samples] = combine(fifo.ACCEL_DATA_X1, fifo.ACCEL_DATA_X0);
				sensor_accel_fifo.y[valid_samples] = combine(fifo.ACCEL_DATA_Y1, fifo.ACCEL_DATA_Y0);
				sensor_accel_fifo.z[valid_samples] = combine(fifo.ACCEL_DATA_Z1, fifo.ACCEL_DATA_Z0);
			}


			if (!gyro_scale_16bit) {
				// Gyro: 20 bit hires mode
				// Gyro [19:12] + Gyro [11:4] + Gyro [3:0] (bottom 4 bits of 20 bit extension byte)
				int32_t gyro_x = reassemble_20bit(fifo.GYRO_DATA_X1, fifo.GYRO_DATA_X0, fifo.Ext_Accel_X_Gyro_X & 0x0F);
				int32_t gyro_y = reassemble_20bit(fifo.GYRO_DATA_Y1, fifo.GYRO_DATA_Y0, fifo.Ext_Accel_Y_Gyro_Y & 0x0F);
				int32_t gyro_z = reassemble_20bit(fifo.GYRO_DATA_Z1, fifo.GYRO_DATA_Z0, fifo.Ext_Accel_Z_Gyro_Z & 0x0F);

				// shift by 1 (least significant bit is always 0)
				gyro_x = gyro_x / 2;
				gyro_y = gyro_y / 2;
				gyro_z = gyro_z / 2;

				// check if any gyro values are going to exceed int16 limits
				if ((gyro_x >= INT16_MAX || gyro_x <= INT16_MIN)
				    || (gyro_y >= INT16_MAX || gyro_y <= INT16_MIN)
				    || (gyro_z >= INT16_MAX || gyro_z <= INT16_MIN)) {

					gyro_scale_16bit = true;

					// 20 bit data scaled to 16 bit
					sensor_gyro_fifo.scale = math::radians(2000.f / 32768.f);

					// rescale any existing data
					for (int j = 0; j < valid_samples + 1; j++) {
						sensor_gyro_fifo.x[j] = combine(buffer.f[j].GYRO_DATA_X1, buffer.f[j].GYRO_DATA_X0);
						sensor_gyro_fifo.y[j] = combine(buffer.f[j].GYRO_DATA_Y1, buffer.f[j].GYRO_DATA_Y0);
						sensor_gyro_fifo.z[j] = combine(buffer.f[j].GYRO_DATA_Z1, buffer.f[j].GYRO_DATA_Z0);
					}

				} else {
					sensor_gyro_fifo.x[valid_samples] = gyro_x;
					sensor_gyro_fifo.y[valid_samples] = gyro_y;
					sensor_gyro_fifo.z[valid_samples] = gyro_z;
				}

			} else {
				sensor_gyro_fifo.x[i] = combine(fifo.GYRO_DATA_X1, fifo.GYRO_DATA_X0);
				sensor_gyro_fifo.y[i] = combine(fifo.GYRO_DATA_Y1, fifo.GYRO_DATA_Y0);
				sensor_gyro_fifo.z[i] = combine(fifo.GYRO_DATA_Z1, fifo.GYRO_DATA_Z0);
			}


			// temperature
			const int16_t TEMP_DATA = combine(fifo.TEMP_DATA1, fifo.TEMP_DATA0);

			// sample invalid if -32768
			if (TEMP_DATA == -32768) {
				return false;
			}

			temperature_sum += TEMP_DATA;


			// HEADER_TIMESTAMP_FSYNC - 0b10: Packet contains ODR Timestamp
			if (!(FIFO_HEADER & Bit3)) {
				PX4_DEBUG("header timestamp missing");
				return false;
			}


			const uint16_t time_delta = (fifo.TimeStamp_h << 8) + fifo.TimeStamp_l;

			if (_enable_clock_input) {
				// RTC_MODE=1, TMST_RES=0 timestamp interval reported in FIFO requires scaling by a factor of 32.768/RTC Frequency.
				const float kFifoIntervalScale = static_cast<float>(32768) / _input_clock_freq;
				timestamp_interval_sum += (float)time_delta * kFifoIntervalScale;

			} else {
				// RTC_MODE=0, TMST_RES=0 - timestamp interval reported in FIFO requires scaling by a factor of 32/30
				static constexpr float kFifoIntervalScale = 32.f / 30.f;
				timestamp_interval_sum += (float)time_delta * kFifoIntervalScale;
			}

			valid_samples++;
		}
	}

	if (valid_samples > 0) {

		sensor_accel_fifo.samples = valid_samples;
		sensor_gyro_fifo.samples = valid_samples;

		for (int i = 0; i < valid_samples; i++) {
			// sensor's frame is +x forward, +y left, +z up
			//  flip y & z to publish right handed with z down (x forward, y right, z down)

			// sensor_accel_fifo.x[i] no change
			sensor_accel_fifo.y[i] = math::negate(sensor_accel_fifo.y[i]);
			sensor_accel_fifo.z[i] = math::negate(sensor_accel_fifo.z[i]);
			//rotate_3i(_rotation, sensor_accel_fifo.x[i], sensor_accel_fifo.y[i], sensor_accel_fifo.z[i]);

			// sensor_gyro_fifo.x[i] no change
			sensor_gyro_fifo.y[i] = math::negate(sensor_gyro_fifo.y[i]);
			sensor_gyro_fifo.z[i] = math::negate(sensor_gyro_fifo.z[i]);
			//rotate_3i(_rotation, sensor_gyro_fifo.x[i], sensor_gyro_fifo.y[i], sensor_gyro_fifo.z[i]);
		}

		const float temperature_avg = temperature_sum / valid_samples;

		// use average temperature reading
		const float TEMP_degC = (temperature_avg / TEMPERATURE_SENSITIVITY) + TEMPERATURE_OFFSET;

		if (PX4_ISFINITE(TEMP_degC)) {
			_px4_accel.set_temperature(TEMP_degC);
			_px4_gyro.set_temperature(TEMP_degC);

		} else {
			perf_count(_bad_transfer_perf);
			return false;
		}

		if (timestamp_interval_sum > 0) {
			const float dt_avg = (timestamp_interval_sum / valid_samples);

			// check dt is within +=1% of expected value
			if ((dt_avg < (FIFO_SAMPLE_DT * 0.99f)) || (dt_avg > (FIFO_SAMPLE_DT * 1.01f))) {
				perf_count(_fifo_timestamp_error_perf);

			} else {
				sensor_accel_fifo.dt = dt_avg;
				sensor_gyro_fifo.dt = dt_avg;
			}
		}

		const uint32_t error_count = perf_event_count(_bad_register_perf) + perf_event_count(_bad_transfer_perf) +
					     perf_event_count(_fifo_empty_perf) + perf_event_count(_fifo_overflow_perf);

		_px4_accel.set_error_count(error_count);
		_px4_gyro.set_error_count(error_count);

		_px4_accel.set_scale(sensor_accel_fifo.scale);
		_px4_gyro.set_scale(sensor_gyro_fifo.scale);

		// publish accel
		sensor_accel_fifo.timestamp = hrt_absolute_time();
		_px4_accel.updateFIFO(sensor_accel_fifo);

		// publish gyro
		sensor_accel_fifo.timestamp = hrt_absolute_time();
		_px4_gyro.updateFIFO(sensor_gyro_fifo);

		return true;
	}

	return false;
}

void IIM42652::FIFOReset()
{
	perf_count(_fifo_reset_perf);

	// SIGNAL_PATH_RESET: FIFO flush
	RegisterSetBits(Register::BANK_0::SIGNAL_PATH_RESET, SIGNAL_PATH_RESET_BIT::FIFO_FLUSH);

	// reset while FIFO is disabled
	_drdy_timestamp_sample.store(0);
}
