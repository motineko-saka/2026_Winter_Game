#include "GameClerScene.h"
#include <cstring>
#include <memory>
#include <DxLib.h>
#include "../SceneManager.h"
#include "../TitleScene/TitleScene.h"
#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"

namespace
{
	const int FADE_FRAMES = 60;		// 暗いところから明るくなるまで

	// 文字を画面の中央に、倍率つきで描く
	void DrawCenteredText(int y, int color, double scale, const char* text)
	{
		const int w = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
		const int x = (Application::SCREEN_SIZE_X - static_cast<int>(w * scale)) / 2;
		DrawExtendString(x, y, scale, scale, text, color);
	}
}

GameClearScene::GameClearScene(void)
{
}

GameClearScene::~GameClearScene(void)
{
}

void GameClearScene::Init(void)
{
	timer_ = 0;
}

void GameClearScene::Load(void)
{
}

void GameClearScene::LoadEnd(void)
{
}

void GameClearScene::Update(void)
{
	timer_++;

	// 文字が出きってから、Enterで次へ
	if (timer_ >= FADE_FRAMES && InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
	{
		// 積まれているシーン（GameScene など）をすべて解放して、タイトルへ戻る
		SceneManager::GetInstance()->ChangeSceneAll(std::make_shared<TitleScene>());
	}
}

void GameClearScene::Draw(void)
{
	// 背景（真っ暗）
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, GetColor(0, 0, 0), true);

	// 文字は少しずつ明るく出す
	const int alpha = (timer_ >= FADE_FRAMES) ? 255 : 255 * timer_ / FADE_FRAMES;
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);

	const int cy = Application::SCREEN_SIZE_Y / 2;
	DrawCenteredText(cy - 70, GetColor(255, 230, 100), 4.0, "GAME CLEAR");
	DrawCenteredText(cy + 50, GetColor(200, 200, 200), 1.5, "あそんでくれて ありがとう！");
	DrawCenteredText(cy + 110, GetColor(160, 160, 160), 1.0, "(α版)");

	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void GameClearScene::Release(void)
{
}