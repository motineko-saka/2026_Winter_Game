#include "GameScene.h"
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../TitleScene/TitleScene.h"
#include "../BattleScene/BattleScene.h"
#include "../MapScene/MapScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Player/Player.h"
#include "../../../AppSystem/Camera/Camera2D.h"
#include "../../Object/Stage/StageManager.h"

GameScene::GameScene(void)
{
}

GameScene::~GameScene(void)
{
}

void GameScene::Init(void)
{

	//mainScreenId_ = MakeScreen(Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, true);
	StageManager::GetInstance()->Init();

	player_->Init();
}

void GameScene::Load(void)
{
	StageManager::GetInstance()->CreateInstance();
	StageManager::GetInstance()->Load();

	player_ = new Player();

	player_->Load();
}

void GameScene::LoadEnd(void)
{
	StageManager::GetInstance()->LoadEnd();

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

	StageManager::GetInstance()->Update();

	player_->Update();

	// プレイヤーのワールド座標を渡す
	Camera2D::GetInstance()->ScreenMove(player_->GetWorldPosX(), player_->GetWorldPosY());
}

void GameScene::Draw(void)
{

	// 画面のズーム処理
	//SetDrawScreen(mainScreenId_);

	//ClearDrawScreen();

	StageManager::GetInstance()->Draw();
	player_->Draw();

	//SetDrawScreen(DX_SCREEN_BACK);

	//ClearDrawScreen();

	//DrawRotaGraph(
	//	Application::SCREEN_SIZE_X / 2, 
	//	Application::SCREEN_SIZE_Y / 2, 
	//	2.0, 0.0, mainScreenId_, true);

}

void GameScene::Release(void)
{
	player_->Release();
	delete player_;

	StageManager::GetInstance()->Release();
	StageManager::DeleteInstance();
}
