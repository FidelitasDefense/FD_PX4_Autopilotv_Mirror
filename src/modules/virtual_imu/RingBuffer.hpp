/****************************************************************************
 *
 *   Copyright (C) 2022 PX4 Development Team. All rights reserved.
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

/**
 * @file RingBuffer.hpp
 * Template RingBuffer
 */

#include <inttypes.h>
#include <cstdio>
#include <cstring>

template <typename T, size_t SIZE>
class RingBuffer
{
public:
	void push(const T &sample)
	{
		size_t head_new = _head;

		if (!_first_write) {
			head_new = (_head + 1) % SIZE;
		}

		_buffer[head_new] = sample;
		_head = head_new;

		// move tail if we overwrite it
		if (_head == _tail && !_first_write) {
			_tail = (_tail + 1) % SIZE;

		} else {
			_first_write = false;
		}
	}

	size_t get_length() const { return SIZE; }

	T &operator[](const size_t index) { return _buffer[index]; }

	const T &get_newest() const { return _buffer[_head]; }
	const T &get_oldest() const { return _buffer[_tail]; }

	size_t get_oldest_index() const { return _tail; }

	bool peak_first_older_than(const uint64_t &timestamp, T *sample)
	{
		// start looking from newest observation data
		for (size_t i = 0; i < SIZE; i++) {
			int index = (_head - i);
			index = index < 0 ? SIZE + index : index;

			if (timestamp >= _buffer[index].time_us && timestamp < _buffer[index].time_us + (uint64_t)100'000) {
				*sample = _buffer[index];
				return true;
			}

			if (index == _tail) {
				// we have reached the tail and haven't got a
				// match
				return false;
			}
		}

		return false;
	}

	// Peak the closest sample to timestamp between timestamp_oldest and timestamp_newest
	// bool peak_closest_between(const uint64_t &timestamp, const uint64_t &timestamp_oldest, const uint64_t &timestamp_newest, T *sample) {

	// 	if (entries() == 0) {
	// 		return false;
	// 	}

	// 	if (timestamp_oldest >= timestamp_newest) {
	// 		return false;
	// 	}

	// 	uint64_t closest_time = 0;
	// 	size_t closest_index = 0;

	// 	for (size_t i = 0; i < SIZE; i++) {

	// 		size_t index = (_tail + i) % SIZE;

	// 		if (_buffer[index].time_us >= timestamp_oldest && _buffer[index].time_us <= timestamp_newest) {
	// 			if (closest_time == 0 || abs((int64_t)timestamp - (int64_t)_buffer[index].time_us) < abs((int64_t)timestamp - (int64_t)closest_time)) {
	// 				closest_time = _buffer[index].time_us;
	// 				closest_index = index;
	// 			}
	// 		}
	// 	}

	// 	if (closest_time != 0) {
	// 		*sample = _buffer[closest_index];
	// 		return true;
	// 	}

	// 	return false;
	// }

	bool pop_first_older_than(const uint64_t &timestamp, T *sample)
	{
		// start looking from newest observation data
		for (size_t i = 0; i < SIZE; i++) {
			int index = (_head - i);
			index = index < 0 ? SIZE + index : index;

			if (timestamp >= _buffer[index].time_us && timestamp < _buffer[index].time_us + (uint64_t)100'000) {
				*sample = _buffer[index];

				// Now we can set the tail to the item which
				// comes after the one we removed since we don't
				// want to have any older data in the buffer
				if (index == _head) {
					_tail = _head;
					_first_write = true;

				} else {
					_tail = (index + 1) % SIZE;
				}

				_buffer[index].time_us = 0;

				return true;
			}

			if (index == _tail) {
				// we have reached the tail and haven't got a
				// match
				return false;
			}
		}

		return false;
	}

	bool pop_oldest(const uint64_t &timestamp_oldest, const uint64_t &timestamp_newest, T *sample)
	{
		if (timestamp_oldest >= timestamp_newest) {
			return false;
		}

		for (size_t i = 0; i < SIZE; i++) {

			size_t index = (_tail + i) % SIZE;

			if (_buffer[index].time_us >= timestamp_oldest && _buffer[index].time_us <= timestamp_newest) {
				*sample = _buffer[index];

				// Now we can set the tail to the item which
				// comes after the one we removed since we don't
				// want to have any older data in the buffer
				if (index == _head) {
					_tail = _head;
					_first_write = true;

				} else {
					_tail = (index + 1) % SIZE;
				}

				return true;
			}

			// If tail equals head we can stop
			if (index == _head) {
				return false;
			}

			// If the sample timetamp is newer than timestamp_newest we can stop
			if (_buffer[index].time_us > timestamp_newest) {
				return false;
			}
		}

		return false;
	}

	bool peek_oldest(const uint64_t &timestamp_oldest, const uint64_t &timestamp_newest, T *sample)
	{
		if (_tail == _head) {
			return false;
		}

		if (timestamp_oldest >= timestamp_newest) {
			return false;
		}

		for (size_t i = 0; i < SIZE; i++) {

			size_t index = (_tail + i) % SIZE;

			if (_buffer[index].time_us >= timestamp_oldest && _buffer[index].time_us <= timestamp_newest) {
				*sample = _buffer[index];
				return true;
			}

			if (index == _head) {
				// we have reached the head and haven't got a
				// match
				return false;
			}
		}

		return false;
	}

	bool pop_oldest(T *sample)
	{
		if (_tail == _head) {
			return false;
		}

		*sample = _buffer[_tail];
		_buffer[_tail].time_us = 0;

		_tail = (_tail + 1) % SIZE;

		return true;
	}

	bool pop_newest(T *sample)
	{
		if (_tail == _head) {
			return false;
		}

		*sample = _buffer[_head];
		_buffer[_head].time_us = 0;

		_head = (_head - 1) % SIZE;

		return true;
	}

	int get_total_size() const { return sizeof(*this) + sizeof(T) * SIZE; }

	size_t entries() const
	{
		int count = 0;

		if (_tail == _head) {
			return count;
		}

		if (_tail < _head) {
			count = SIZE - _head + _tail;

		} else {
			count = _tail - _head;
		}

		// for (size_t i = 0; i < SIZE; i++) {
		// 	if (_buffer[i].time_us != 0) {
		// 		count++;
		// 	}
		// }

		return count;
	}

private:
	T _buffer[SIZE] {};

	size_t _head{0};
	size_t _tail{0};

	bool _first_write{true};
};
