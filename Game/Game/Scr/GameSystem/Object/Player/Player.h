#pragma once
#include "PlayerParam.h"
#include "../ObjectBase.h"
#include "../../../Common/Common.h"
#include "../../../Utility/Utility.h"

class Player : public ObjectBase
{
public:
	// コンストラクタ
	Player(void);

	// デストラクタ
	~Player(void) {};

	void Init(void) override;
	void Load(void) override;
	void LoadEnd(void) override;
	void Draw(void) override;
	void Release(void) override;

public:

	// ワールド座標の取得
	int GetWorldPosX() const { return Utility::Round(image_.pos.x); }
	int GetWorldPosY() const { return Utility::Round(image_.pos.y); }

private:
	// 状態の関数
	static void StateNormal(ObjectBase& obj);

private:
	// 最後に向いていた向き（停止時の待機画像の選択に使う）
	enum Facing
	{
		FACING_DOWN,	// 正面（下）
		FACING_SIDE,	// 横（左右の反転は reverseFlg_ で管理）
		FACING_UP,		// 後ろ（上）
	};

	// 向きに応じた待機画像をセットする
	void SetIdleAnim(void);

private:
	// 移動状態の関数
	static void MoveStateIdle(ObjectBase& obj);
	static void MoveStateWalk(ObjectBase& obj);
	static void MoveStateRun(ObjectBase& obj);
	static void MoveStateUp(ObjectBase& obj);
	static void MoveStateDown(ObjectBase& obj);

private:
	// 移動関数(ベースから継承 毎ループ呼ばれる)
	void MoveController(void) override;
	void MoveEnd(void) override;

public:

	Vector2 GetPlayerPos(void) { return playerPos_; }

private:

	Vector2F movedPos_;					// 移動計算用の座標			

	Image image_;						// 変数(画像は別で持つ)

	Vector2 playerPos_;					// プレイヤー表示座標

	// アニメーション
	PlayerAnimation curPlayerAnim_;		// 現在のアニメーション
	int animHandle_[MAX];				// アニメーションの画像
	int moveAnimDelay_;					// 移動アニメーションの切り替え時間
	bool reverseFlg_;					// 画像を反転させるか
	bool initialReverseFlg_;            // 初期の画像の向き
	bool isRun_;                        // ダッシュ中か（上下アニメ速度の切り替え用）
	Facing facing_;						// 最後に向いていた向き

#ifdef _DEBUG
private:
	void DebugDraw(void);
#endif // _DEBUG

};