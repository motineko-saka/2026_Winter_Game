#include "PauseScene.h"

#include <DxLib.h>

#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"

#include "../SceneManager.h"

PauseScene::PauseScene(void)
{
}

PauseScene::~PauseScene(void)
{
}

void PauseScene::Init(void)
{
	selectGameEnd_ = false;
}

void PauseScene::Load(void)
{
}

void PauseScene::LoadEnd(void)
{
}

void PauseScene::Update(void)
{
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		// 自分自身を消す
		SceneManager::GetInstance()->PopScene();
	}

	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_LEFT))
	{
		selectGameEnd_ = true;
	}
	else if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RIGHT))
	{
		selectGameEnd_ = false;
	}

	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
	{
		if (selectGameEnd_) SceneManager::GetInstance()->GameEnd();
		else SceneManager::GetInstance()->PopScene();
	}
}

void PauseScene::Draw(void)
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 127);
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, 0x000000, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 220);
	DrawBox(
		Application::SCREEN_SIZE_X * 0.5 - 100,
		Application::SCREEN_SIZE_Y * 0.5 - 100,
		Application::SCREEN_SIZE_X * 0.5 + 100,
		Application::SCREEN_SIZE_Y * 0.5 + 100,
		0xFFFFFF,
		true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	DrawFormatString(
		Application::SCREEN_SIZE_X * 0.5 - 90,
		Application::SCREEN_SIZE_Y * 0.5 - 50,
		0x000000,
		"ゲームを終了しますか？"
	);

	DrawFormatString(
		Application::SCREEN_SIZE_X * 0.5 - 60,
		Application::SCREEN_SIZE_Y * 0.5,
		0x000000,
		"はい"
	);

	DrawFormatString(
		Application::SCREEN_SIZE_X * 0.5 + 10,
		Application::SCREEN_SIZE_Y * 0.5,
		0x000000,
		"いいえ"
	);

	if (selectGameEnd_)
	{
		DrawBox(
			Application::SCREEN_SIZE_X * 0.5 - 75,
			Application::SCREEN_SIZE_Y * 0.5 - 10,
			Application::SCREEN_SIZE_X * 0.5 - 10,
			Application::SCREEN_SIZE_Y * 0.5 + 20,
			0x7777777,
			false);
	}
	else
	{
		DrawBox(
			Application::SCREEN_SIZE_X * 0.5,
			Application::SCREEN_SIZE_Y * 0.5 - 10,
			Application::SCREEN_SIZE_X * 0.5 + 65,
			Application::SCREEN_SIZE_Y * 0.5 + 20,
			0x7777777,
			false);
	}

}

void PauseScene::Release(void)
{
}
