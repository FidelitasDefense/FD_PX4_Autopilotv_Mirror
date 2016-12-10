/****************************************************************************
 *
 *   Copyright (c) 2013-2016 PX4 Development Team. All rights reserved.
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
 * @file mc_pos_control_params.c
 * Multicopter position controller parameters.
 * ������λ�ÿ��Ʋ���
 * @author Anton Babushkin <anton@px4.io>
 */

/**
 * Minimum thrust in auto thrust control
 * �Զ����������µ���С����������һ�����Ϊ���ŵı仯����
 * It's recommended to set it > 0 to avoid free fall with zero thrust.
 * �Ƽ�����ֵ����Ϊһ������0��ֵ�Ա�����û��������ʱ����������
 *
 * @unit norm
 * @min 0.05
 * @max 1.0
 * @decimal 2
 * @increment 0.01
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_THR_MIN, 0.12f);

/**
 * Hover thrust
 * ��ͣ����
 *
 * Vertical thrust required to hover.
 * This value is mapped to center stick for manual throttle control.
 * With this value set to the thrust required to hover, transition
 * from manual to ALTCTL mode while hovering will occur with the
 * throttle stick near center, which is then interpreted as (near)
 * zero demand for vertical speed.
 * ��Ӧ�ֶ�ģʽ������ֵ��������ʾ��ͣ��Ҫ��������
 * �ֶ�ģʽת����ģʽ�ɴ�����ͣ��
 *
 * @unit norm
 * @min 0.2
 * @max 0.8
 * @decimal 2
 * @increment 0.01
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_THR_HOVER, 0.5f);

/**
 * ALTCTL throttle curve breakpoint
 * ����ģʽ���������߶ϵ�
 *
 * Halfwidth of deadband or reduced sensitivity center portion of curve.
 * This is the halfwidth of the center region of the ALTCTL throttle
 * curve. It extends from center-dz to center+dz.
 * �����������ߵ���������İ������
 * ����������=���е�-�ϵ㣩~���е�+�ϵ㣩��
 * �������ڱ��ֶ��ߣ���ֱ������������
 *
 * @unit norm
 * @min 0.0
 * @max 0.2
 * @decimal 2
 * @increment 0.05
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_ALTCTL_DZ, 0.1f);

/**
 * ALTCTL throttle curve breakpoint height
 * ����ģʽ�������߶ϵ�ĸ߶�
 *
 * Controls the slope of the reduced sensitivity region.
 * This is the height of the ALTCTL throttle
 * curve at center-dz and center+dz.
 * ���ƽ��������������б�ʡ����Ƕ���ģʽ����������������dz��Χ�ڵĸ߶�����
 *
 * @min 0.0
 * @max 0.2
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_ALTCTL_DY, 0.0f);

/**
 * Maximum thrust in auto thrust control
 * �Զ��������Ƶ��������
 *
 * Limit max allowed thrust. Setting a value of one can put
 * the system into actuator saturation as no spread between
 * the motors is possible any more. A value of 0.8 - 0.9
 * is recommended.
 * ���ƿ�������������������ֵ����Ϊ1����ʹϵͳ�������ͣ����ڵ��֮�䲻���ܴ���spread
 * ���齫��ֵ����Ϊ0.8~0.9
 *
 * @unit norm
 * @min 0.0
 * @max 0.95
 * @decimal 2
 * @increment 0.01
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_THR_MAX, 0.9f);

/**
 * Minimum manual thrust
 * ��С���ֶ�����
 *
 * Minimum vertical thrust. It's recommended to set it > 0 to avoid free fall with zero thrust.
 * ��С�Ĵ�ֱ���������齫��ֵ���ó�һ������0��ֵ�Ա���������Ϊ0ʱ��������
 *
 * @unit norm
 * @min 0.0
 * @max 1.0
 * @decimal 2
 * @increment 0.01
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_MANTHR_MIN, 0.08f);

/**
 * Maximum manual thrust
 * �����ֶ�����
 *
 * Limit max allowed thrust. Setting a value of one can put
 * the system into actuator saturation as no spread between
 * the motors is possible any more. A value of 0.8 - 0.9
 * is recommended.
 * ����������������������ֵ����Ϊ1����ʹϵͳ�������ͣ����ڵ��֮�䲻���ܴ���spread
 * ���齫��ֵ����Ϊ0.8~0.9
 *
 * @unit norm
 * @min 0.0
 * @max 1.0
 * @decimal 2
 * @increment 0.01
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_MANTHR_MAX, 0.9f);

/**
 * Proportional gain for vertical position error
 * ��ֱλ�����ı������� 
 *
 * @min 0.0
 * @max 1.5
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_P, 1.0f);

/**
 * Proportional gain for vertical velocity error
 * ��ֱ�ٶ����ı�������
 * @min 0.1
 * @max 0.4
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_P, 0.2f);

/**
 * Integral gain for vertical velocity error
 * ��ֱ�ٶ����Ļ�������
 * Non zero value allows hovering thrust estimation on stabilized or autonomous takeoff.
 * ��ֵ����ʱ��������������Ȼ����Զ����ģʽ�µ���ͣ��������
 *
 * @min 0.01
 * @max 0.1
 * @decimal 3
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_I, 0.02f);

/**
 * Differential gain for vertical velocity error
 * ��ֱ�ٶ�����΢������
 *
 * @min 0.0
 * @max 0.1
 * @decimal 3
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_D, 0.0f);

/**
 * Maximum vertical ascent velocity
 * ���ֱ�����ٶ�
 * Maximum vertical velocity in AUTO mode and endpoint for stabilized modes (ALTCTRL, POSCTRL).
 * �Զ�ģʽ�µ����ֱ�ٶȣ�����ģʽ�µ���ֵ�����ߡ����㣩
 *
 * @unit m/s
 * @min 0.5
 * @max 8.0
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_MAX_UP, 3.0f);

/**
 * Maximum vertical descent velocity
 * ���ֱ�½��ٶ�
 * Maximum vertical velocity in AUTO mode and endpoint for stabilized modes (ALTCTRL, POSCTRL).
 * �Զ�ģʽ�µ����ֱ�ٶȣ�����ģʽ�µ���ֵ�����ߡ����㣩
 *
 * @unit m/s
 * @min 0.5
 * @max 4.0
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_MAX, 1.0f);

/**
 * Transitional support, do not change / use
 * ת��֧�֣���Ҫ�ı�/ʹ��
 *
 * @unit m/s
 * @min 0.5
 * @max 4.0
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_VEL_MAX_DN, 1.0f);

/**
 * Vertical velocity feed forward
 * ��ֱ�ٶ�ǰ��
 * Feed forward weight for altitude control in stabilized modes (ALTCTRL, POSCTRL). 
 * 0 will give slow responce and no overshot, 1 - fast responce and big overshot.
 * ����ģʽ�¶Ը߶Ƚ��п��Ƶ�ǰ��Ȩ�ء�
 * ��ֵΪ0ʱ��ʹ����Ӧ���޳�������ֵΪ1ʱ��Ӧ�쳬����
 *
 * @min 0.0
 * @max 1.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_Z_FF, 0.5f);

/**
 * Proportional gain for horizontal position error
 * ˮƽλ�����ı�������
 * @min 0.0
 * @max 2.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_P, 0.95f);

/**
 * Proportional gain for horizontal velocity error
 * ˮƽ�ٶ����ı�������
 * @min 0.06
 * @max 0.15
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_VEL_P, 0.09f);

/**
 * Integral gain for horizontal velocity error
 * ˮƽ�ٶ����Ļ�������
 * Non-zero value allows to resist wind.
 * ��ֵ��0ʱ�����𵽿��������
 *
 * @min 0.0
 * @max 0.1
 * @decimal 3
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_VEL_I, 0.02f);

/**
 * Differential gain for horizontal velocity error. Small values help reduce fast oscillations. 
 * If value is too big oscillations will appear again.
 * ˮƽ�ٶ�����΢�����档��ֵ��Сʱ�ɰ������Ϳ��ٵ��𵴣����ֵ̫���𵴻��ٴη���
 *
 * @min 0.005
 * @max 0.1
 * @decimal 3
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_VEL_D, 0.01f);

/**
 * Nominal horizontal velocity
 *
 * Normal horizontal velocity in AUTO modes (includes
 * also RTL / hold / etc.) and endpoint for
 * position stabilized mode (POSCTRL).
 * �Զ�ģʽ�µ�һ��ˮƽ�ٶȣ�����RTL/HOLD�ȵȣ�������ģʽ�µĵ���ֵ
 *
 * @unit m/s
 * @min 3.0
 * @max 20.0
 * @increment 1
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_CRUISE, 5.0f);

/**
 * Maximum horizontal velocity
 * ���ˮƽ�ٶ�
 * Maximum horizontal velocity in AUTO mode. If higher speeds
 * are commanded in a mission they will be capped to this velocity.
 *
 * @unit m/s
 * @min 0.0
 * @max 20.0
 * @increment 1
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_VEL_MAX, 8.0f);

/**
 * Horizontal velocity feed forward
 * ˮƽ�ٶ�ǰ��
 * Feed forward weight for position control in position control mode (POSCTRL). 
 * 0 will give slow responce and no overshot, 1 - fast responce and big overshot.
 *
 * @min 0.0
 * @max 1.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_XY_FF, 0.5f);

/**
 * Maximum tilt angle in air
 * ����ʱ�������б��
 * Limits maximum tilt in AUTO and POSCTRL modes during flight.
 * ���Ʒ���ʱ���Զ�ģʽ�Լ�����ģʽ�µ������б��
 *
 * @unit deg
 * @min 0.0
 * @max 90.0
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_TILTMAX_AIR, 45.0f);

/**
 * Maximum tilt during landing
 * ��½ʱ�������б��
 * Limits maximum tilt angle on landing.
 * 
 *
 * @unit deg
 * @min 0.0
 * @max 90.0
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_TILTMAX_LND, 12.0f);

/**
 * Landing descend rate
 * ����ʱ���½��ٶ�
 * @unit m/s
 * @min 0.2
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_LAND_SPEED, 0.5f);

/**
 * Takeoff climb rate
 * ���ʱ�������ٶ�
 * @unit m/s
 * @min 1
 * @max 5
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_TKO_SPEED, 1.5f);

/**
 * Max manual roll
 * �ֶ�ģʽ�µ������ 
 *
 * @unit deg
 * @min 0.0
 * @max 90.0
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_MAN_R_MAX, 35.0f);

/**
 * Max manual pitch
 * �ֶ�ģʽ�µ������
 *
 * @unit deg
 * @min 0.0
 * @max 90.0
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_MAN_P_MAX, 35.0f);

/**
 * Max manual yaw rate
 * �ֶ�ģʽ�µ����ƫ��
 *
 * @unit deg/s
 * @min 0.0
 * @max 400
 * @decimal 1
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_MAN_Y_MAX, 200.0f);

/**
 * Deadzone of X,Y sticks where position hold is enabled
 * ʹ�ܶ���ģʽʱX,Yң�˵����� 
 *
 * @min 0.0
 * @max 1.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_HOLD_XY_DZ, 0.1f);

/**
 * Maximum horizontal velocity for which position hold is enabled (use 0 to disable check)
 * ʹ��λ�ñ���(����)ģʽʱ�����ˮƽ�ٶȣ�����ֵ����Ϊ0�Խ��ü�⣩
 *
 * @unit m/s
 * @min 0.0
 * @max 3.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_HOLD_MAX_XY, 0.8f);

/**
 * Maximum vertical velocity for which position hold is enabled (use 0 to disable check)
 * ʹ��λ�ñ���(����)ģʽʱ�����ֱ�ٶȣ�����ֵ����Ϊ0�Խ��ü�⣩
 *
 * @unit m/s
 * @min 0.0
 * @max 3.0
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_HOLD_MAX_Z, 0.6f);

/**
 * Low pass filter cut freq. for numerical velocity derivative
 * ��ͨ�˲����Ľ�ֹƵ�ʣ������ٶ�΢��
 *
 * @unit Hz
 * @min 0.0
 * @max 10
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_VELD_LP, 5.0f);

/**
 * Maximum horizonal acceleration in velocity controlled modes
 * ���ٶȿ���ģʽ�µ����ˮƽ���ٶ�
 *
 * @unit m/s/s
 * @min 2.0
 * @max 15.0
 * @increment 1
 * @decimal 2
 * @group Multicopter Position Control
 */
PARAM_DEFINE_FLOAT(MPC_ACC_HOR_MAX, 5.0f);

/**
 * Altitude control mode, note mode 1 only tested with LPE
 * ����ģʽ����ע�⣬ģʽ1������LPE���� 
 *
 * @min 0
 * @max 1
 * @value 0 Altitude following
 * @value 1 Terrain following
 * @group Multicopter Position Control
 */
PARAM_DEFINE_INT32(MPC_ALT_MODE, 0);
