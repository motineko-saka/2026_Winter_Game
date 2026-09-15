#include "Player.h"

#include <string>
#include <DxLib.h>

#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../../Utility/Utility.h"
#include "../../Scene/SceneManager.h"
#include "../../Scene/GameScene/GameScene.h"
#include "../Stage/Stage.h"

#include "../../../Common/Debug.h"

Player::Player() : ObjectBase()
{
	// 状態の登録
	stateTable_[STATE_NOMAL] = StateNormal;

	// 移動状態の登録
	moveStateTable_[MOVE_STATE_IDLE] = MoveStateIdle;
	moveStateTable_[MOVE_STATE_WALK] = MoveStateWalk;
	moveStateTable_[MOVE_STATE_RUN] = MoveStateRun;
	moveStateTable_[MOVE_STATE_UP] = MoveStateUp;
	moveStateTable_[MOVE_STATE_DOWN] = MoveStateDown;

	// 初期向きのデフォルト
	initialReverseFlg_ = false;
}

void Player::Init(void)
{
	curState_ = STATE_NOMAL;			// 状態の初期化
	curMoveState_ = MOVE_STATE_IDLE;	// 移動状態の初期化

	image_.pos.x = 50;
	image_.pos.y = 300;
	image_.scale = 0.5f;


	curPlayerAnim_ = IDLE;
	moveAnimDelay_ = 0;
	reverseFlg_ = false;

	// 初期向きを記憶
	reverseFlg_ = initialReverseFlg_;
}

void Player::Load(void)
{
	animHandle_[IDLE]	 = LoadGraph("Data/Image/Player/Idle.png");
	animHandle_[WALK_A]	 = LoadGraph("Data/Image/Player/Walk1.png");
	animHandle_[WALK_B]	 = LoadGraph("Data/Image/Player/Walk2.png");
	animHandle_[UP_A]	 = LoadGraph("Data/Image/Player/Back1.png");
	animHandle_[UP_B]	 = LoadGraph("Data/Image/Player/Back2.png");
	animHandle_[DOWN_A] = LoadGraph("Data/Image/Player/Flont1.png");
	animHandle_[DOWN_B] = LoadGraph("Data/Image/Player/Flont2.png");
}

void Player::LoadEnd(void)
{
}

void Player::Draw(void)
{
	DrawRotaGraph(
		image_.pos.x,
		image_.pos.y,
		image_.scale,
		image_.angle,
		animHandle_[curPlayerAnim_],
		true,
		reverseFlg_
	);

	DebugDraw();
}

void Player::Delete(void)
{
	// 画像の削除
	for (int i = 0; i < MAX; i++)
	{
		DeleteGraph(animHandle_[i]);
	}
}

void Player::StateNormal(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
}

void Player::MoveStateIdle(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
	player.image_.pos.x += player.movedPos_.x;

	// Idleに戻るときは必ず最初の向きに戻る
	player.reverseFlg_ = player.initialReverseFlg_;

	player.curPlayerAnim_ = IDLE;
	player.moveAnimDelay_ = 0;
}

void Player::MoveStateWalk(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
	player.image_.pos.x += player.movedPos_.x;
	
	// 歩きアニメ―ションの切り替え
	if (player.moveAnimDelay_++ > WALK_ANIM_DELAY)
	{
		player.curPlayerAnim_ == WALK_A ? player.curPlayerAnim_ = WALK_B : player.curPlayerAnim_ = WALK_A;
		player.moveAnimDelay_ = 0;
	}
}

void Player::MoveStateRun(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
	player.image_.pos.x += player.movedPos_.x;

	// 動いてないならアニメーションしない
	if (player.movedPos_.x != 0)
	{
		// 歩きアニメ―ションの切り替え
		if (player.moveAnimDelay_++ > RUN_ANIM_DELAY)
		{
			player.curPlayerAnim_ == WALK_A ? player.curPlayerAnim_ = WALK_B : player.curPlayerAnim_ = WALK_A;
			player.moveAnimDelay_ = 0;
		}
	}
}

void Player::MoveStateUp(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
	player.image_.pos.y += player.movedPos_.y;

	// アニメーションを上昇に
	if (player.curPlayerAnim_ != UP_A && player.curPlayerAnim_ != UP_B)
	{
		player.curPlayerAnim_ = UP_A;
		player.moveAnimDelay_ = 0;
	}
	else
	{
		// 歩きアニメ―ションの切り替え
		if (player.moveAnimDelay_++ > WALK_ANIM_DELAY)
		{
			player.curPlayerAnim_ == UP_A ? player.curPlayerAnim_ = UP_B : player.curPlayerAnim_ = UP_A;
			player.moveAnimDelay_ = 0;
		}
	}
}

void Player::MoveStateDown(ObjectBase& obj)
{
	Player& player = static_cast<Player&>(obj);
	player.image_.pos.y += player.movedPos_.y;
	// アニメーションを下降に
	if (player.curPlayerAnim_ != DOWN_A && player.curPlayerAnim_ != DOWN_B)
	{
		player.curPlayerAnim_ = DOWN_A;
		player.moveAnimDelay_ = 0;
	}
	else
	{
		// 歩きアニメ―ションの切り替え
		if (player.moveAnimDelay_++ > WALK_ANIM_DELAY)
		{
			player.curPlayerAnim_ == DOWN_A ? player.curPlayerAnim_ = DOWN_B : player.curPlayerAnim_ = DOWN_A;
			player.moveAnimDelay_ = 0;
		}
	}
}

void Player::MoveController(void)
{
	movedPos_.x = 0.0f;
	movedPos_.y = 0.0f;

	MoveState moveState = MOVE_STATE_IDLE;
	
	// 走り
	if (InputManager::GetInstance()->IsNew(KEY_INPUT_LSHIFT))
	{
		moveState = MOVE_STATE_RUN;
	}

	// 左に移動
	if (InputManager::GetInstance()->IsNew(KEY_INPUT_A))
	{
		// 向きを左に
		reverseFlg_ = true;

		switch (moveState)
		{
		case MOVE_STATE_RUN:	// 走り
			movedPos_.x = -RUN_SPEED;
			break;
		default:
			// 通常移動
			movedPos_.x = -WALK_SPEED;
			moveState = MOVE_STATE_WALK;
			break;
		}
	}
	// 右に移動
	else if (InputManager::GetInstance()->IsNew(KEY_INPUT_D))
	{
		// 向きを右に
		reverseFlg_ = false;

		switch (moveState)
		{
		case MOVE_STATE_RUN:	// 走り
			movedPos_.x = RUN_SPEED;
			break;
		default:
			// 通常移動
			movedPos_.x = WALK_SPEED;
			moveState = MOVE_STATE_WALK;
			break;
		}
	}
	// 上に移動
	else if (InputManager::GetInstance()->IsNew(KEY_INPUT_W))
	{
		// 向きをデフォルトに
		reverseFlg_ = initialReverseFlg_;

		movedPos_.y = -WALK_SPEED;
		moveState = MOVE_STATE_UP;
	}
	// 下に移動
	else if (InputManager::GetInstance()->IsNew(KEY_INPUT_S))
	{
		// 向きをデフォルトに
		reverseFlg_ = initialReverseFlg_;

		movedPos_.y = WALK_SPEED;
		moveState = MOVE_STATE_DOWN;
	}

	// 移動状態をセット
	this->SetMoveState(moveState);
}

void Player::MoveEnd(void)
{
	// 移動予定の座標を確定させる
	image_.pos.x += movedPos_.x;
	image_.pos.y += movedPos_.y;
}

#ifdef _DEBUG
void Player::DebugDraw(void)
{
	switch (curMoveState_)
	{
	case MOVE_STATE_IDLE:DrawFormatString(0, 0, 0xff0000, "待機"); break;
	case MOVE_STATE_WALK:DrawFormatString(0, 0, 0xff0000, "歩行"); break;
	case MOVE_STATE_RUN:DrawFormatString(0, 0, 0xff0000, "走り"); break;
	case MOVE_STATE_UP:DrawFormatString(0, 0, 0xff0000, "上昇"); break;
	case MOVE_STATE_DOWN:DrawFormatString(0, 0, 0xff0000, "下降"); break;
	case MOVE_STATE_MAX:default:LogMessage("player->curMoveState->ありえない状態", "playerLog.txt"); break;
	}

	DrawFormatString(0, 20, 0xff0000, "PPos.x = %f  PPos.y = %f", image_.pos.x, image_.pos.y);
	DrawFormatString(0, 40, 0xff0000, "PMPos.x = %f  PMPos.y = %f", movedPos_.x, movedPos_.y);
}
#endif // _DEBUG
