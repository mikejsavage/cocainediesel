#pragma once

#include "qcommon/types.h"

struct RagdollConfig {
	u8 pelvis;
	u8 spine;
	u8 neck;
	u8 left_shoulder;
	u8 left_elbow;
	u8 right_shoulder;
	u8 right_elbow;
	u8 left_hip;
	u8 left_knee;
	u8 left_ankle;
	u8 right_hip;
	u8 right_knee;
	u8 right_ankle;

	float lower_back_radius;
	float upper_back_radius;
	float head_half_height;
	float head_radius;
	float left_upper_arm_radius;
	float left_forearm_radius;
	float right_upper_arm_radius;
	float right_forearm_radius;
	float left_thigh_radius;
	float left_lower_leg_radius;
	float left_foot_radius;
	float right_thigh_radius;
	float right_lower_leg_radius;
	float right_foot_radius;
};

void InitRagdollEditor();
void ResetRagdollEditor();
void DrawRagdollEditor();
