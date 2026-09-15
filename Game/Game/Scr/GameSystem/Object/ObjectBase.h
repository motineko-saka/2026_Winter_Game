#pragma once
#include <DxLib.h>

#include "ObjcetState.h"
#include "../../Common/Common.h"

class ObjectBase
{
public:
	ObjectBase();
    virtual ~ObjectBase() = default;

    virtual void Init() = 0;        // 初期化
    virtual void Load() = 0;        // 読み込み
    virtual void LoadEnd() = 0;     // 読み込み後
    virtual void Draw() = 0;        // 描画
    virtual void Delete() = 0;      // 削除

    virtual void Update();          // 更新

    // 状態遷移
    void SetState(State newState) { curState_ = newState; }
    void SetMoveState(MoveState newState) { curMoveState_ = newState; }
    void SetAttackState(AttackState newState) { curAttackState_ = newState; }

    // 状態を返却
    State GetState() const { return curState_; };
    MoveState GetMoveState() const { return curMoveState_; };
    AttackState GetAttackState() const { return curAttackState_; };

protected:
    // 移動は常に呼ぶ(純粋仮想関数)
    virtual void MoveController() = 0;  // 行動時の最初に呼ばれる
    virtual void MoveEnd() = 0;         // 行動後に呼ばれる

protected:
    State curState_;
    MoveState curMoveState_;
    AttackState curAttackState_;

    // 状態のテーブル（派生クラスでセットする）
    StateFunction stateTable_[STATE_MAX];
    StateFunction moveStateTable_[MOVE_STATE_MAX];
    StateFunction attackStateTable_[ATTACK_STATE_MAX];
};
