#include "BattleScene.h"
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../GameScene/GameScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../../AppSystem/Application/Application.h"


BattleScene::BattleScene(void)
{
}

BattleScene::~BattleScene(void)
{
}

void BattleScene::Init(void)
{
}

void BattleScene::Load(void)
{
}

void BattleScene::LoadEnd(void)
{
}

void BattleScene::Update(void)
{				
	// ポーズ画面を積む
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<PauseScene>());
	}

	// ゲーム画面に戻る
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
	{
		SceneManager::GetInstance()->PopScene();
	}
}

void BattleScene::Draw(void)
{
	DrawBox(0, 0, 640, 480, GetColor(0, 255, 0), TRUE);
}

void BattleScene::Release(void)
{
}
