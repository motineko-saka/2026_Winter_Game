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
	void Delete(void) override;

private:
	// 状態の関数
	static void StateNormal(ObjectBase& obj);

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

private:
	// 移動計算用の座標
	Vector2F movedPos_;

	// 変数(画像は別で持つ)
	Image image_;

	// アニメーション
	PlayerAnimation curPlayerAnim_;		// 現在のアニメーション
	int animHandle_[MAX];				// アニメーションの画像
	int moveAnimDelay_;					// 移動アニメーションの切り替え時間
	bool reverseFlg_;					// 画像を反転させるか
	bool initialReverseFlg_;            // 初期の画像の向き

#ifdef _DEBUG
private:
	void DebugDraw(void);
#endif // _DEBUG

};
