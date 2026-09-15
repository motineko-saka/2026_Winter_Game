#pragma once
#include "../Application/Application.h"

// 2Dカメラクラス
class Player;

class Camera2D
{
public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new Camera2D(); } };
	static Camera2D* GetInstance(void) { return instance_; };
	static void DeleteInstance(void) { if (instance_ != nullptr) { delete instance_; instance_ = nullptr; } }

private:
	// 静的インスタンス
	static Camera2D* instance_;
private:
	// デフォルトコンストラクタをprivateにして、外部から生成できない様にする
	Camera2D(void);
	// デストラクタも同様
	~Camera2D(void);

	// コピー・ムーブ操作を禁止
	Camera2D(const Camera2D&) = delete;
	Camera2D& operator=(const Camera2D&) = delete;
	Camera2D(Camera2D&&) = delete;
	Camera2D& operator=(Camera2D&&) = delete;

private:
	// クラス内定数
	static constexpr int GAME_SIZE_X = 6400;
	static constexpr int GAME_SIZE_Y = 6400;

	// スクリーンが移動を始める範囲
	static constexpr int SCREEN_MOVE_LEFT	= Application::SCREEN_SIZE_X / 2;	// 左端
	static constexpr int SCREEN_MOVE_RIGHT	= Application::SCREEN_SIZE_X / 2;	// 右端
	static constexpr int SCREEN_MOVE_UP		= Application::SCREEN_SIZE_Y / 2;	// 上端
	static constexpr int SCREEN_MOVE_DOWN	= Application::SCREEN_SIZE_Y / 2;	// 下端

public:

	void ScreenMove(int lookPosX, int lookPosY);

	int GetScreenPosX(void) { return screenPosX_; }
	int GetScreenPosY(void) { return screenPosY_; }

	int GetScreenPostoWorldPosX(int posX) { return  posX - screenPosX_; }
	int GetScreenPostoWorldPosY(int posY) { return  posY - screenPosY_; }

	// ワールド座標 → スクリーン座標に変換
	int WorldToScreenX(int worldX) const { return worldX - screenPosX_; }
	int WorldToScreenY(int worldY) const { return worldY - screenPosY_; }

private:
	// 変数
	int screenPosX_ = 0;
	int screenPosY_ = 0;
};
