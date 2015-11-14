#include "BlockAttEst.hpp"
#include <px4_posix.h>

BlockAttEst::BlockAttEst(SuperBlock * parent, const char * name) :
	SuperBlock(parent, name),
	// Publications (meta, priority, list)
	// Subscriptions (meta, interval, instance, list)
	_subSensor(ORB_ID(sensor_combined), 0, 0, &getSubscriptions()),
	_subParam(ORB_ID(parameter_update), 0, 0, &getSubscriptions()),
	_subControlMode(ORB_ID(vehicle_control_mode), 0, 0, &getSubscriptions()),
	_timestamp(0),
	_timestampGyro(0),
	_timestampMag(0),
	_timestampAccel(0)
{
}

void BlockAttEst::update() {

	// poll
	px4_pollfd_struct_t fds[1];
	fds[0].fd = _subSensor.getHandle();
	fds[0].events = POLLIN;
	px4_poll(&fds[0], sizeof(fds) / sizeof(fds[0]), 100);

	// check timing
	uint64_t now = hrt_absolute_time();
	setDt((now - _timestamp)/1.0e6f);
	if (getDt() > 1e-1f) {
		_timestamp = now;
		return;
	} else if (getDt() < 1e-3f) {
		return;
	}
	printf("dt: %g\n", double(getDt()));
	_timestamp = now;
	handleSensor();
	handleParam();
}

/**
 * Subscription handlers
 */

void BlockAttEst::handleSensor() {
	if (!_subSensor.updated()) return;
	_subSensor.update();

	// new available data
	bool gyroUpdate = false;
	bool accelUpdate = false;
	bool magUpdate = false;

	// check for sensor updates
	uint64_t nowGyro = _subSensor.get().gyro_timestamp[0];
	if (_timestampGyro != nowGyro) {
		//printf("dt gyro: %g\n", double((nowGyro - _timestampGyro)/1.0e6f));
		_timestampGyro = nowGyro;
		gyroUpdate = true;
	}
	uint64_t nowMag = _subSensor.get().magnetometer_timestamp[0];
	if (_timestampMag != nowMag) {
		//printf("dt baro: %g\n", double((nowMag - _timestampMag)/1.0e6f));
		_timestampMag = nowMag;
		magUpdate = true;
	}
	uint64_t nowAccel = _subSensor.get().accelerometer_timestamp[0];
	if (_timestampAccel != nowAccel) {
		//printf("dt accel: %g\n", double((nowAccel - _timestampAccel)/1.0e6f));
		_timestampAccel = nowAccel;
		accelUpdate = true;
	}

	// fire off correct/ predict steps
	if (gyroUpdate || accelUpdate) {
		SMat6 R;
		R.setIdentity();
		Vec6 y;
		y(0) = 0;
		y(1) = 0;
		y(2) = 0;
		y(3) = 0;
		y(4) = 0;
		y(5) = 0;
		predict(y, R);
	}
	if (magUpdate) {
		SMat6 R;
		R.setIdentity();
		Vec6 y;
		y(0) = 0;
		y(1) = 0;
		y(2) = 0;
		y(3) = 0;
		y(4) = 0;
		y(5) = 0;
		correctMagAccel(y, R);
	}
}

void BlockAttEst::handleParam() {
	if (!_subParam.updated()) return;
	_subParam.update();
	updateParams();
}
