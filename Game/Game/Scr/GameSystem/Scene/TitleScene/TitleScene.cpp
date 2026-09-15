#include "TitleScene.h"
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../GameScene/GameScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"

TitleScene::TitleScene(void)
{
}

TitleScene::~TitleScene(void)
{
}

void TitleScene::Init(void)
{
}

void TitleScene::Load(void)
{
}

void TitleScene::LoadEnd(void)
{
}

void TitleScene::Update(void)
{
	// ƒ|[ƒY‰æ–Ê‚ðÏ‚Þ
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<PauseScene>());
	}

	// ƒQ[ƒ€‰æ–Ê‚É‘JˆÚ
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_SPACE))
	{
		SceneManager::GetInstance()->ChangeScene(std::make_shared<GameScene>());
	}
}

void TitleScene::Draw(void)
{
	DrawBox(0, 0, 640, 480, GetColor(255, 0, 255), TRUE);
}

void TitleScene::Release(void)
{
}
