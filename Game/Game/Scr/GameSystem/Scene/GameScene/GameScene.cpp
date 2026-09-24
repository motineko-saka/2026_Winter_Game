#include "GameScene.h"
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../TitleScene/TitleScene.h"
#include "../BattleScene/BattleScene.h"
#include "../MapScene/MapScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Player/Player.h"

GameScene::GameScene(void)
{
}

GameScene::~GameScene(void)
{
}

void GameScene::Init(void)
{
	player_->Init();
}

void GameScene::Load(void)
{
	player_ = new Player();

	player_->Load();
}

void GameScene::LoadEnd(void)
{
	player_->LoadEnd();

	// 初期化
	Init();
}

void GameScene::Update(void)
{
	// ポーズ画面を積む
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<PauseScene>());
	}

	// バトル画面に遷移
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<BattleScene>());
	}

	// マップ画面に遷移
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_M))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<MapScene>());
	}

	player_->Update();
}

void GameScene::Draw(void)
{
	player_->Draw();
}

void GameScene::Release(void)
{
	player_->Delete();
	delete player_;
}
