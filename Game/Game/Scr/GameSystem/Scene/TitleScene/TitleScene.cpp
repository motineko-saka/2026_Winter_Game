#include "TitleScene.h"
#include "../SceneManager.h"
#include "../GameEndScene/GameEndScene.h"
#include "../GameScene/GameScene.h"
#include "../SaveSlotScene/SaveSlotScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"

TitleScene::TitleScene(void)
{
}

TitleScene::~TitleScene(void)
{
}

void TitleScene::Init(void)
{
	Load();
}

void TitleScene::Load(void)
{
	image_ = LoadGraph("Data/Image/UI/Title/Title.png");
}

void TitleScene::LoadEnd(void)
{
	Init();
}

void TitleScene::Update(void)
{
	// ポーズ画面を積む
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<GameEndScene>());
	}

	const int n = static_cast<int>(Menu::MAX);
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_UP))
	{
		cursor_ = (cursor_ + n - 1) % n;
	}
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_DOWN))
	{
		cursor_ = (cursor_ + 1) % n;
	}

	// 決定
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN)
		|| InputManager::GetInstance()->IsTrgDown(KEY_INPUT_Z)
		|| InputManager::GetInstance()->IsTrgDown(KEY_INPUT_SPACE))
	{
		Decide();
	}
}

void TitleScene::Decide(void)
{
	switch (static_cast<Menu>(cursor_))
	{
	case Menu::NEW_GAME:
		// はじめから：ロードせずにゲームを始める
		SceneManager::GetInstance()->ChangeScene(std::make_shared<GameScene>());
		break;

	case Menu::CONTINUE:
	{
		// つづきから：セーブデータを選ぶ画面を重ねる（選ばれたらゲームへ）
		auto scene = std::make_shared<SaveSlotScene>();
		scene->Setup(SaveSlotScene::Mode::LOAD, [](int slot)
			{
				auto game = std::make_shared<GameScene>();
				game->SetLoadSlot(slot);
				SceneManager::GetInstance()->ChangeScene(game);
			});
		SceneManager::GetInstance()->PushScene(scene);
		break;
	}

	default:
		break;
	}
}

void TitleScene::Draw(void)
{
	if (image_ >= 0)
	{
		DrawGraph(0, 0, image_, TRUE);
	}
	else
	{
		DrawBox(0, 0, 640, 480, GetColor(255, 0, 255), TRUE);
	}

	// メニュー
	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);

	const char* items[] = { "はじめから", "つづきから" };
	const int white = GetColor(255, 255, 255);
	const int yellow = GetColor(255, 230, 120);
	const int x = sw / 2 - 60;
	const int y = sh - 160;

	for (int i = 0; i < static_cast<int>(Menu::MAX); i++)
	{
		const bool selected = (i == cursor_);
		DrawFormatString(x, y + i * 36, selected ? yellow : white,
			"%s %s", selected ? "▶" : "　", items[i]);
	}
}

void TitleScene::Release(void)
{
}