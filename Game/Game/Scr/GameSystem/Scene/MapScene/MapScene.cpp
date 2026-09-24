#include "MapScene.h"
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../GameScene/GameScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"

MapScene::MapScene(void)
{
}

MapScene::~MapScene(void)
{
}

void MapScene::Init(void)
{
	Load();
}

void MapScene::Load(void)
{
	map_handle_ = LoadGraph("Data/Image/UI/Map/Map/Map.png");
}

void MapScene::LoadEnd(void)
{
	Init();
}

void MapScene::Update(void)
{
	// ポーズ画面を積む
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<PauseScene>());
	}

	// ゲーム画面に戻る
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_M))
	{
		SceneManager::GetInstance()->PopScene();
	}

}

void MapScene::Draw(void)
{
	// 背景色
	//DrawBox(0, 0, 640, 480, GetColor(244, 229, 17), true);

	DrawGraph(0, 0, map_handle_, true);
}

void MapScene::Release(void)
{
	// 読み込んだグラフィックハンドルを解放
	if (map_handle_ != -1)
	{
		DeleteGraph(map_handle_);
		map_handle_ = -1;
	}
}
