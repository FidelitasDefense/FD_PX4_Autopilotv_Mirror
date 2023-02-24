/****************************************************************************
 *
 *   Copyright (c) 2013-2020 PX4 Development Team. All rights reserved.
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
 * AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
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
 * @file usv_att_control_params.c
 *
 * Parameters defined by the attitude control task for unmanned underwater vehicles (USVs)
 *
 * This is a modification of the fixed wing/ground rover params and it is designed for ground rovers.
 * It has been developed starting from the fw  module, simplified and improved with dedicated items.
 *
 * All the ackowledgments and credits for the fw wing/rover app are reported in those files.
 * TODO: Update this comment
 *
 * @author Norbert Szulc <>
 */

/*
 * Controller parameters, accessible via MAVLink
 *
 */

// Roll gains
/**
 * Roll proportional gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_ROLL_P, 4.0f);

/**
 * Roll differential gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_ROLL_D, 1.5f);


// Pitch gains
/**
 * Pitch proportional gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_PITCH_P, 4.0f);

/**
 * Pitch differential gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_PITCH_D, 2.0f);


// Yaw gains
/**
 * Yawh proportional gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_YAW_P, 4.0f);

/**
 * Yaw differential gain
 *
 * @group USV Attitude Control
 * @decimal 2
 */
PARAM_DEFINE_FLOAT(USV_YAW_D, 2.0f);


// Input Modes
/**
 * Select Input Mode
 *
 * @value 0 use Attitude Setpoints
 * @value 1 Direct Feedthrough
 * @group USV Attitude Control
 */
PARAM_DEFINE_INT32(USV_INPUT_MODE, 0);

/**
 * Skip the controller
 *
 * @value 0 use the module's controller
 * @value 1 skip the controller and feedthrough the setpoints
 */
PARAM_DEFINE_INT32(USV_SKIP_CTRL, 0);

/**
 * Thrust CAP
 *
 * @group USV Attitude Control
 */
PARAM_DEFINE_FLOAT(USV_DIRCT_ROLL, 0.5f);

/**
 * Direct pitch input
 *
 * @group USV Attitude Control
 */
PARAM_DEFINE_FLOAT(USV_DIRCT_PITCH, 0.0f);

/**
 * Direct yaw input
 *
 * @group USV Attitude Control
 */
PARAM_DEFINE_FLOAT(USV_DIRCT_YAW, 0.0f);

/**
 * Direct thrust input
 *
 * @group USV Attitude Control
 */
PARAM_DEFINE_FLOAT(USV_DIRCT_THRUST, 0.0f);

/**
 * Max thrust
 *
 * @group USV Attitude Control
 * @unit norm
 * @min 0.05
 * @max 1.0
 * @decimal 2
 * @increment 0.01
 */
PARAM_DEFINE_FLOAT(USV_THR_MAX_AC, 0.5f);
