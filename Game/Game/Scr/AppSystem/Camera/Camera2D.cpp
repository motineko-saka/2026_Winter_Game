#include "Camera2D.h"

Camera2D* Camera2D::instance_ = nullptr;

Camera2D::Camera2D()
{
}

Camera2D::~Camera2D()
{
}

/// <summary>
/// スクリーンの移動処理
/// </summary>
/// <param name="lookPosX">スクリーン座標が見るX座標</param>
/// <param name="lookPosY">スクリーン座標が見るY座標</param>
void Camera2D::ScreenMove(int lookPosX, int lookPosY)
{
	// プレイヤー（lookPos）が画面の中央に来るようにスクリーンの左上座標を計算する
	// スクリーンX座標 ＝ プレイヤーのワールドX － (画面幅の半分)
	screenPosX_ = lookPosX - (Application::SCREEN_SIZE_X / 2);
	screenPosY_ = lookPosY - (Application::SCREEN_SIZE_Y / 2);

	// --- マップの端（ワールドの端）を超えないようにクランプする ---

	// 左端
	if (screenPosX_ < 0)
	{
		screenPosX_ = 0;
	}
	// 右端
	if (screenPosX_ > GAME_SIZE_X - Application::SCREEN_SIZE_X)
	{
		screenPosX_ = GAME_SIZE_X - Application::SCREEN_SIZE_X;
	}
	// 上端
	if (screenPosY_ < 0)
	{
		screenPosY_ = 0;
	}
	// 下端
	if (screenPosY_ > GAME_SIZE_Y - Application::SCREEN_SIZE_Y)
	{
		screenPosY_ = GAME_SIZE_Y - Application::SCREEN_SIZE_Y;
	}
}