/****************************************************************************
 *
 *   Copyright (C) 2019 PX4 Development Team. All rights reserved.
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

#include <gtest/gtest.h>
#include <ControlMath.hpp>
#include <px4_platform_common/defines.h>

using namespace matrix;
using namespace ControlMath;


TEST(ControlMathTest, ThrottleAttitudeMapping)
{
	/* expected: zero roll, zero pitch, zero yaw, full thr mag
	 * reason: thrust pointing full upward */
	Vector3f thr{0.f, 0.f, -1.f};
	float yaw = 0.f;
	int omni_att_mode = 0;
	float omni_dfc_max_thrust = 0.0f;
	vehicle_attitude_setpoint_s att{};
	thrustToAttitude(thr, yaw, omni_att_mode, omni_dfc_max_thrust, att);
	EXPECT_FLOAT_EQ(att.roll_body, 0.f);
	EXPECT_FLOAT_EQ(att.pitch_body, 0.f);
	EXPECT_FLOAT_EQ(att.yaw_body, 0.f);
	EXPECT_FLOAT_EQ(att.thrust_body[2], -1.f);

	/* expected: same as before but with 90 yaw
	 * reason: only yaw changed */
	yaw = M_PI_2_F;
	thrustToAttitude(thr, yaw, omni_att_mode, omni_dfc_max_thrust, att);
	EXPECT_FLOAT_EQ(att.roll_body, 0.f);
	EXPECT_FLOAT_EQ(att.pitch_body, 0.f);
	EXPECT_FLOAT_EQ(att.yaw_body, M_PI_2_F);
	EXPECT_FLOAT_EQ(att.thrust_body[2], -1.f);

	/* expected: same as before but roll 180
	 * reason: thrust points straight down and order Euler
	 * order is: 1. roll, 2. pitch, 3. yaw */
	thr = Vector3f(0.f, 0.f, 1.f);
	thrustToAttitude(thr, yaw, omni_att_mode, omni_dfc_max_thrust, att);
	EXPECT_FLOAT_EQ(att.roll_body, -M_PI_F);
	EXPECT_FLOAT_EQ(att.pitch_body, 0.f);
	EXPECT_FLOAT_EQ(att.yaw_body, M_PI_2_F);
	EXPECT_FLOAT_EQ(att.thrust_body[2], -1.f);
}

TEST(ControlMathTest, ConstrainXYPriorities)
{
	const float max = 5.f;
	// v0 already at max
	Vector2f v0(max, 0);
	Vector2f v1(v0(1), -v0(0));

	Vector2f v_r = constrainXY(v0, v1, max);
	EXPECT_FLOAT_EQ(v_r(0), max);
	EXPECT_FLOAT_EQ(v_r(1), 0);

	// norm of v1 exceeds max but v0 is zero
	v0.zero();
	v_r = constrainXY(v0, v1, max);
	EXPECT_FLOAT_EQ(v_r(1), -max);
	EXPECT_FLOAT_EQ(v_r(0), 0.f);

	v0 = Vector2f(.5f, .5f);
	v1 = Vector2f(.5f, -.5f);
	v_r = constrainXY(v0, v1, max);
	const float diff = Vector2f(v_r - (v0 + v1)).length();
	EXPECT_FLOAT_EQ(diff, 0.f);

	// v0 and v1 exceed max and are perpendicular
	v0 = Vector2f(4.f, 0.f);
	v1 = Vector2f(0.f, -4.f);
	v_r = constrainXY(v0, v1, max);
	EXPECT_FLOAT_EQ(v_r(0), v0(0));
	EXPECT_GT(v_r(0), 0);
	const float remaining = sqrtf(max * max - (v0(0) * v0(0)));
	EXPECT_FLOAT_EQ(v_r(1), -remaining);
}

TEST(ControlMathTest, CrossSphereLine)
{
	/* Testing 9 positions (+) around waypoints (o):
	 *
	 * Far             +              +              +
	 *
	 * Near            +              +              +
	 * On trajectory --+----o---------+---------o----+--
	 *                    prev                curr
	 *
	 * Expected targets (1, 2, 3):
	 * Far             +              +              +
	 *
	 *
	 * On trajectory -------1---------2---------3-------
	 *
	 *
	 * Near            +              +              +
	 * On trajectory -------o---1---------2-----3-------
	 *
	 *
	 * On trajectory --+----o----1----+--------2/3---+-- */
	const Vector3f prev = Vector3f(0.f, 0.f, 0.f);
	const Vector3f curr = Vector3f(0.f, 0.f, 2.f);
	Vector3f res;
	bool retval = false;

	// on line, near, before previous waypoint
	retval = cross_sphere_line(Vector3f(0.f, 0.f, -.5f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 0.5f));

	// on line, near, before target waypoint
	retval = cross_sphere_line(Vector3f(0.f, 0.f, 1.f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 2.f));

	// on line, near, after target waypoint
	retval = cross_sphere_line(Vector3f(0.f, 0.f, 2.5f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 2.f));

	// near, before previous waypoint
	retval = cross_sphere_line(Vector3f(0.f, .5f, -.5f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, .366025388f));

	// near, before target waypoint
	retval = cross_sphere_line(Vector3f(0.f, .5f, 1.f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 1.866025448f));

	// near, after target waypoint
	retval = ControlMath::cross_sphere_line(matrix::Vector3f(0.f, .5f, 2.5f), 1.f, prev, curr, res);
	EXPECT_TRUE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 2.f));

	// far, before previous waypoint
	retval = ControlMath::cross_sphere_line(matrix::Vector3f(0.f, 2.f, -.5f), 1.f, prev, curr, res);
	EXPECT_FALSE(retval);
	EXPECT_EQ(res, Vector3f());

	// far, before target waypoint
	retval = ControlMath::cross_sphere_line(matrix::Vector3f(0.f, 2.f, 1.f), 1.f, prev, curr, res);
	EXPECT_FALSE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 1.f));

	// far, after target waypoint
	retval = ControlMath::cross_sphere_line(matrix::Vector3f(0.f, 2.f, 2.5f), 1.f, prev, curr, res);
	EXPECT_FALSE(retval);
	EXPECT_EQ(res, Vector3f(0.f, 0.f, 2.f));
}
