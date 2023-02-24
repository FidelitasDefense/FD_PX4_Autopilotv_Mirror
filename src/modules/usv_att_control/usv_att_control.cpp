/****************************************************************************
 *
 *   Copyright (c) 2020 PX4 Development Team. All rights reserved.
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
 *
 * This module is a modification of the rover attitide control module and is designed for the
 * TUHH hippocampus.
 *
 * All the acknowledgments and credits for the fw wing app are reported in those files.
 * TODO: Update comment
 *
 * @author Norbert Szulc <>
 */

#include "usv_att_control.hpp"


#define ACTUATOR_PUBLISH_PERIOD_MS 4


/**
 * UUV attitude control app start / stop handling function
 *
 * @ingroup apps
 */
extern "C" __EXPORT int usv_att_control_main(int argc, char *argv[]);


USVAttitudeControl::USVAttitudeControl():
	ModuleParams(nullptr),
	WorkItem(MODULE_NAME, px4::wq_configurations::nav_and_controllers),
	/* performance counters */
	_loop_perf(perf_alloc(PC_ELAPSED, MODULE_NAME": cycle"))
{
}

USVAttitudeControl::~USVAttitudeControl()
{
	perf_free(_loop_perf);
}

bool USVAttitudeControl::init()
{
	if (!_vehicle_attitude_sub.registerCallback()) {
		PX4_ERR("callback registration failed");
		return false;
	}

	return true;
}

void USVAttitudeControl::parameters_update(bool force)
{
	// check for parameter updates
	if (_parameter_update_sub.updated() || force) {
		// clear update
		parameter_update_s pupdate;
		_parameter_update_sub.copy(&pupdate);

		// update parameters from storage
		updateParams();
	}
}

// This function seems like invention of UUV controller
void USVAttitudeControl::constrain_actuator_commands(float roll_u, float pitch_u, float yaw_u,
		float thrust_x, float thrust_y, float thrust_z)
{
	float max = 1.0f;
	float scaling = _param_max_thrust_ac.get();
	if (PX4_ISFINITE(roll_u)) {
		roll_u = math::constrain(roll_u, -max, max);
		_actuators.control[actuator_controls_s::INDEX_ROLL] = roll_u;

	} else {
		_actuators.control[actuator_controls_s::INDEX_ROLL] = 0.0f;
	}

	if (PX4_ISFINITE(pitch_u)) {
		pitch_u = math::constrain(pitch_u, -max, max);
		_actuators.control[actuator_controls_s::INDEX_PITCH] = pitch_u;

	} else {
		_actuators.control[actuator_controls_s::INDEX_PITCH] = 0.0f;
	}

	if (PX4_ISFINITE(yaw_u)) {
		yaw_u = math::constrain(yaw_u, -max, max);
		_actuators.control[actuator_controls_s::INDEX_YAW] = yaw_u * scaling;

	} else {
		_actuators.control[actuator_controls_s::INDEX_YAW] = 0.0f;
	}

	if (PX4_ISFINITE(thrust_x)) {
		thrust_x = math::constrain(thrust_x, -max, max);
		_actuators.control[actuator_controls_s::INDEX_THROTTLE] = thrust_x * scaling;

	} else {
		_actuators.control[actuator_controls_s::INDEX_THROTTLE] = 0.0f;
	}

	if (PX4_ISFINITE(thrust_y)) {
		thrust_y = math::constrain(thrust_y, -max, max);
		_actuators.control[actuator_controls_s::INDEX_FLAPS] = thrust_y * scaling;

	} else {
		_actuators.control[actuator_controls_s::INDEX_FLAPS] = 0.0f;
	}

	if (PX4_ISFINITE(thrust_z)) {
		thrust_z = math::constrain(thrust_z, -max, max);
		_actuators.control[actuator_controls_s::INDEX_SPOILERS] = thrust_z;

	} else {
		_actuators.control[actuator_controls_s::INDEX_SPOILERS] = 0.0f;
	}
}

void USVAttitudeControl::control_attitude_yaw_only(const vehicle_attitude_s &attitude,
		const vehicle_attitude_setpoint_s &attitude_setpoint, const vehicle_angular_velocity_s &angular_velocity,
		const vehicle_rates_setpoint_s &rates_setpoint)
{
	Eulerf euler_angles(matrix::Quatf(attitude.q));

	// float roll_u;
	// float pitch_u;
	float yaw_u;
	float thrust_x;
	float thrust_y;
	float thrust_z;

	// float roll_body = attitude_setpoint.roll_body;
	// float pitch_body = attitude_setpoint.pitch_body;
	float yaw_body = attitude_setpoint.yaw_body;

	// float roll_rate_desired = rates_setpoint.roll;
	// float pitch_rate_desired = rates_setpoint.pitch;
	float yaw_rate_desired = rates_setpoint.yaw;

	/* get attitude setpoint rotational matrix */
	Dcmf rot_des = Eulerf(0.0f, 0.0f, yaw_body);

	/* get current rotation matrix from control state quaternions */
	Quatf q_att(attitude.q);
	Matrix3f rot_att =  matrix::Dcm<float>(q_att);

	Vector3f e_R_vec;
	Vector3f torques;

	/* Compute matrix: attitude error */
	Matrix3f e_R = (rot_des.transpose() * rot_att - rot_att.transpose() * rot_des) * 0.5;

	/* vee-map the error to get a vector instead of matrix e_R */
	e_R_vec(0) = e_R(2, 1);  /**< Roll  */
	e_R_vec(1) = e_R(0, 2);  /**< Pitch */
	e_R_vec(2) = e_R(1, 0);  /**< Yaw   */

	Vector3f omega{angular_velocity.xyz};
	// omega(0) -= roll_rate_desired;
	// omega(1) -= pitch_rate_desired;
	omega(2) -= yaw_rate_desired;

	/**< P-Control */
	// torques(0) = - e_R_vec(0) * _param_roll_p.get();	/**< Roll  */
	// torques(1) = - e_R_vec(1) * _param_pitch_p.get();	/**< Pitch */
	torques(2) = - e_R_vec(2) * _param_yaw_p.get();		/**< Yaw   */

	/**< PD-Control */
	// torques(0) = torques(0) - omega(0) * _param_roll_d.get();  /**< Roll  */
	// torques(1) = torques(1) - omega(1) * _param_pitch_d.get(); /**< Pitch */
	torques(2) = torques(2) - omega(2) * _param_yaw_d.get();   /**< Yaw   */

	// roll_u = torques(0);
	// pitch_u = torques(1);
	yaw_u = torques(2);

	// take thrust as is
	thrust_x = attitude_setpoint.thrust_body[0];
	thrust_y = attitude_setpoint.thrust_body[1];
	thrust_z = attitude_setpoint.thrust_body[2];


	constrain_actuator_commands(0.0f, 0.0f, yaw_u, thrust_x, thrust_y, thrust_z);
	/* Geometric Controller END*/
}

void USVAttitudeControl::Run()
{
	if (should_exit()) {
		// TODO: unregister callbacks
		_vehicle_attitude_sub.unregisterCallback();
		exit_and_cleanup();
		return;
	}

	perf_begin(_loop_perf);

	/* check vehicle control mode for changes to publication state */
	_vehicle_control_mode_sub.update(&_vehicle_control_mode);

	/* update parameters from storage */
	parameters_update();

	vehicle_attitude_s attitude;

	/* only run controller if attitude changed */
	if (_vehicle_attitude_sub.update(&attitude)) {
		vehicle_angular_velocity_s angular_velocity {};
		_angular_velocity_sub.copy(&angular_velocity);

		/* Run geometric attitude controllers if NOT manual mode*/
		if (!_vehicle_control_mode.flag_control_manual_enabled
		    && _vehicle_control_mode.flag_control_attitude_enabled
		    && _vehicle_control_mode.flag_control_rates_enabled) {

			int input_mode = _param_input_mode.get();

			_vehicle_attitude_setpoint_sub.update(&_attitude_setpoint);
			_vehicle_rates_setpoint_sub.update(&_rates_setpoint);

			// PX4_INFO("got att setpoint?");
			if (input_mode == 1) { // process manual data
				// TODO: NOTE: this is ignored for some reason
				_attitude_setpoint.roll_body = _param_direct_pitch.get();
				_attitude_setpoint.pitch_body = _param_direct_pitch.get();
				_attitude_setpoint.yaw_body = _param_direct_yaw.get();
				_attitude_setpoint.thrust_body[0] = _param_direct_thrust.get();
				_attitude_setpoint.thrust_body[1] = _param_direct_thrust.get();
				_attitude_setpoint.thrust_body[2] = 0.f;
			}

			/* Geometric Control*/
			int skip_controller = _param_skip_ctrl.get();

			if (skip_controller) {
				constrain_actuator_commands(_rates_setpoint.roll, _rates_setpoint.pitch, _rates_setpoint.yaw,
							    _rates_setpoint.thrust_body[0], _rates_setpoint.thrust_body[1], _rates_setpoint.thrust_body[2]);

			} else {
				// TODO: This proppably could be simplified
				control_attitude_yaw_only(attitude, _attitude_setpoint, angular_velocity, _rates_setpoint);
			}
		}
	}

	/* Manual Control mode (e.g. gamepad,...) - raw feedthrough no assistance */
	if (_manual_control_setpoint_sub.update(&_manual_control_setpoint)) {
		// This should be copied even if not in manual mode. Otherwise, the poll(...) call will keep
		// returning immediately and this loop will eat up resources.
		if (_vehicle_control_mode.flag_control_manual_enabled && !_vehicle_control_mode.flag_control_rates_enabled) {
			/* manual/direct control */
			constrain_actuator_commands(0.0f, 0.0f,
						    _manual_control_setpoint.yaw,
						    _manual_control_setpoint.throttle,
						    // TODO: really? Can I setup joystick in QGC to enable moving to the sides? Or is it the common practice
						    _manual_control_setpoint.roll,
						    0.0f);
		}

	}

	_actuators.timestamp = hrt_absolute_time();

	/* Only publish if any of the proper modes are enabled */
	if (_vehicle_control_mode.flag_control_manual_enabled ||
	    _vehicle_control_mode.flag_control_attitude_enabled) {
		/* publish the actuator controls */
		_actuator_controls_pub.publish(_actuators);
		publishTorqueSetpoint(0);
		publishThrustSetpoint(0);
	}

	perf_end(_loop_perf);
}

// TODO: timestamp_sample??
// TODO: do it like mc_rate_control
void USVAttitudeControl::publishTorqueSetpoint(const hrt_abstime &timestamp_sample)
{
	vehicle_torque_setpoint_s v_torque_sp = {};
	v_torque_sp.timestamp = hrt_absolute_time();
	v_torque_sp.timestamp_sample = timestamp_sample;
	v_torque_sp.xyz[0] = _actuators.control[actuator_controls_s::INDEX_ROLL];
	v_torque_sp.xyz[1] = _actuators.control[actuator_controls_s::INDEX_PITCH];
	v_torque_sp.xyz[2] = _actuators.control[actuator_controls_s::INDEX_YAW];

	_vehicle_torque_setpoint_pub.publish(v_torque_sp);
}

void USVAttitudeControl::publishThrustSetpoint(const hrt_abstime &timestamp_sample)
{
	vehicle_thrust_setpoint_s v_thrust_sp = {};
	v_thrust_sp.timestamp = hrt_absolute_time();
	v_thrust_sp.timestamp_sample = timestamp_sample;
	v_thrust_sp.xyz[0] = _actuators.control[actuator_controls_s::INDEX_THROTTLE];
	v_thrust_sp.xyz[1] = _actuators.control[actuator_controls_s::INDEX_FLAPS];
	v_thrust_sp.xyz[2] = 0.0f;

	_vehicle_thrust_setpoint_pub.publish(v_thrust_sp);
}


int USVAttitudeControl::task_spawn(int argc, char *argv[])
{
	USVAttitudeControl *instance = new USVAttitudeControl();

	if (instance) {
		_object.store(instance);
		_task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}

	} else {
		PX4_ERR("alloc failed");
	}

	delete instance;
	_object.store(nullptr);
	_task_id = -1;

	return PX4_ERROR;
}

int USVAttitudeControl::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}


int USVAttitudeControl::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Controls the attitude of an unmanned underwater vehicle (UUV).

Publishes `actuator_controls_0` messages at a constant 250Hz.

### Implementation
Currently, this implementation supports only a few modes:

 * Full manual: Roll, pitch, yaw, and throttle controls are passed directly through to the actuators
 * Auto mission: The usv runs missions

### Examples
CLI usage example:
$ usv_att_control start
$ usv_att_control status
$ usv_att_control stop

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("usv_att_control", "controller");
	PRINT_MODULE_USAGE_COMMAND("start")
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

int usv_att_control_main(int argc, char *argv[])
{
	return USVAttitudeControl::main(argc, argv);
}
