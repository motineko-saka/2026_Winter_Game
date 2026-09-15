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
	// 追従対象の位置に応じてスクリーンを移動させる
	// 追従対象の位置とスクリーンの位置の差分を取る
	int diffX = lookPosX - screenPosX_;
	int diffY = lookPosY - screenPosY_;

	// スクリーン座標が動き出す左端
	if (diffX < SCREEN_MOVE_LEFT)
	{
		screenPosX_ -= SCREEN_MOVE_LEFT - diffX;
	}
	// スクリーン座標が動き出す右端
	if (diffX > Application::SCREEN_SIZE_X - SCREEN_MOVE_RIGHT)
	{
		screenPosX_ -= Application::SCREEN_SIZE_X - SCREEN_MOVE_RIGHT - diffX;
	}
	// スクリーン座標が動き出す上端
	if (diffY < SCREEN_MOVE_UP)
	{
		screenPosY_ -= SCREEN_MOVE_UP - diffY;
	}
	// スクリーン座標が動き出す下端
	if (diffY > Application::SCREEN_SIZE_Y - SCREEN_MOVE_DOWN)
	{
		screenPosY_ -= Application::SCREEN_SIZE_Y - SCREEN_MOVE_DOWN - diffY;
	}

	// ワールド座標のサイズ
	int mapSizeX = GAME_SIZE_X;
	int mapSizeY = GAME_SIZE_Y;

	// スクリーンがワールド座標の左端に到達しているか？
	if (screenPosX_ < 0)
	{
		// 到達していたらそれ以上、左にスクリーン座標を動かないようにする
		screenPosX_ = 0;
	}
	// スクリーンがワールド座標の右端に到達しているか？
	if (screenPosX_ > mapSizeX - Application::SCREEN_SIZE_X)
	{
		// 到達していたらそれ以上、右にスクリーン座標を動かないようにする
		screenPosX_ = mapSizeX - Application::SCREEN_SIZE_X;
	}
	// スクリーンがワールド座標の上端に到達しているか？
	if (screenPosY_ < 0)
	{
		// 到達していたらそれ以上、上にスクリーン座標を動かないようにする
		screenPosY_ = 0;
	}
	// スクリーンがワールド座標の下端に到達しているか？
	if (screenPosY_ > mapSizeY - Application::SCREEN_SIZE_Y)
	{
		// 到達していたらそれ以上、下にスクリーン座標を動かないようにする
		screenPosY_ = mapSizeY - Application::SCREEN_SIZE_Y;
	}
}
