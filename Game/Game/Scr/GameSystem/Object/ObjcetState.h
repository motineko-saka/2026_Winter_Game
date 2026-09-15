#pragma once

class ObjectBase; // 前方宣言

// 状態関数型
typedef void (*StateFunction)(ObjectBase&);

// 状態
enum State
{
    STATE_NOMAL,   // 通常
    //STATE_JUMP,    // ジャンプ
    STATE_DAMEGE,  // ダメージ
    STATE_DEAD,    // 死亡

    STATE_MAX,
};

// 移動状態
enum MoveState
{
    MOVE_STATE_IDLE,    // 待機
    MOVE_STATE_WALK,    // 歩く
    MOVE_STATE_RUN,     // 走る
    MOVE_STATE_DUCK,    // しゃがみ
	MOVE_STATE_UP,      // 上昇
	MOVE_STATE_DOWN,    // 下降

    MOVE_STATE_MAX,     // 全状態
};

// 攻撃状態
enum AttackState
{
    ATTACK_STATE_NON,      // 攻撃なし
    ATTACK_STATE_ATTACK,   // 攻撃

    ATTACK_STATE_MAX,      // 全状態
};

// AI状態
enum AIState
{
    AI_STATE_NON,      // 何もしない
    AI_STATE_SEARCH,   // 索敵
    AI_STATE_CHASE,    // 追跡
    AI_STATE_RETREAT,  // 後退
    AI_STATE_RETURN,   // 戻る
    
    AI_STATE_MAX,      // 全状態
};
