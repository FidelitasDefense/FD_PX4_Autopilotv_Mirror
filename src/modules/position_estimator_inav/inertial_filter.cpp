/*
 * inertial_filter.c
 *
 *   Copyright (C) 2013 Anton Babushkin. All rights reserved.
 *   Author: 	Anton Babushkin	<rk3dov@gmail.com>
 */

#include <math.h>

#include "inertial_filter.h"

void inertial_filter_predict(float dt, float x[2], float acc)
{
	if (isfinite(dt)) {
		if (!isfinite(acc)) {
			acc = 0.0f;
		}

		/*
		 * x[2]={pos,vel}
		 * �ڵ�λʱ�����޵�����£�����x[0]���ϵ�λ�����������ٶ�x[1]�ͼ��ٶ�acc�������֣�
		 * ���ٶ�x[1]Ҳ���ϵ�λ�����������ٶ�acc*dt��
		 * ��Ȼacc�����޵���������acc���ã���acc��0�����޼��ٶ������
 		 */
		x[0] += x[1] * dt + acc * dt * dt / 2.0f; 
		x[1] += acc * dt;
	}
}

void inertial_filter_correct(float e, float dt, float x[2], int i, float w)
{
	if (isfinite(e) && isfinite(w) && isfinite(dt)) {
		float ewdt = e * w * dt;
		/*
		 * �����п��Խ�e��������w��Ȩ�أ�λ��������e,w,dt�������ٶ�������e,w2,dt����
		 */
		x[i] += ewdt;

		if (i == 0) {
			x[1] += w * ewdt;
		}
	}
}
