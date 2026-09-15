#pragma once

enum PlayerAnimation
{
	FRONT,		// ê≥ñ 
	IDLE,		// ë“ã@
	WALK_A,		// à⁄ìÆA
	WALK_B,		// à⁄ìÆB
	CLIMB_A,	// ìoÇÈA
	CLIMB_B,	// ìoÇÈB
	HIT,		// îÌÉ_ÉÅ4
	UP_A,		// è„è∏A
	UP_B,		// è„è∏B
	DOWN_A,		// â∫ç~A
	DOWN_B,		// â∫ç~B

	MAX,		// ç≈ëÂ
};

static constexpr float WALK_SPEED = 2.5f;
static constexpr float DUCK_SPEED = 0.5f;
static constexpr float RUN_SPEED = 4.0f;

static constexpr int WALK_ANIM_DELAY = 4;
static constexpr int RUN_ANIM_DELAY = 1;

static constexpr float JUMP_POWOR = 5;
static constexpr float JUMP_TIME = 30;
