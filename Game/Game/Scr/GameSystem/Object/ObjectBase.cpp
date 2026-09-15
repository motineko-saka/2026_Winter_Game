#include "ObjectBase.h"

ObjectBase::ObjectBase()
{
    // 状態テーブルは派生クラスで初期化する
    curState_ = STATE_NOMAL;
    curMoveState_ = MOVE_STATE_IDLE;
}

void ObjectBase::Update()
{
    MoveController();

    // nullチェック
    if (moveStateTable_[curMoveState_])
    {
        moveStateTable_[curMoveState_](*this);
    }

    // nullチェック
    if (stateTable_[curState_])
    {
        stateTable_[curState_](*this);
    }

    MoveEnd();
}
