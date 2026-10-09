#include "GameScene.h"

#include <cstdio>
#include <fstream>
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

	monsterData_.Init();
	monsterParty_.Init();

	itemData_.Init();
	inventory_.Init();

	// テスト用：Item.csv の ID 1〜5 を5個ずつ持たせる（存在するIDだけ）
	// ※本番では、ショップや拾得で増える形に差し替える
	for (int id = 1; id <= 5; id++)
	{
		if (itemData_.GetItem(id) != nullptr)
		{
			inventory_.Add(id, 5);
		}
	}

	// 図鑑番号7番のモンスターをLv5で手持ちに入れる
	MonsterInstance starter;
	if (MonsterParty::Create(monsterData_, 7, 5, starter))
	{
		monsterParty_.Add(starter);
	}
}

void GameScene::Load(void)
{
	StageManager::GetInstance()->CreateInstance();
	StageManager::GetInstance()->Load();

	player_ = new Player();

	player_->Load();

	monsterData_.Load();
	itemData_.Load();
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
	// バトルが終わっていたら、結果を受け取って処理する
	CheckBattleResult();

	// 全滅演出中は操作を受け付けない
	if (defeatPhase_ != DefeatPhase::NONE)
	{
		UpdateDefeat();
		return;
	}

	// ポーズ画面を積む
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	{
		// メニュー画面を積む（手持ち・バッグ・セーブ処理を渡す）
		auto pause = std::make_shared<PauseScene>();
		pause->Setup(&monsterData_, &monsterParty_, &itemData_, &inventory_,
			[this]() { return SaveGame(); });
		SceneManager::GetInstance()->PushScene(pause);
	}

	// バトル画面に遷移
	//if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
	//{
	//	SceneManager::GetInstance()->PushScene(std::make_shared<BattleScene>());
	//}

	// マップ画面に遷移
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_M))
	{
		SceneManager::GetInstance()->PushScene(std::make_shared<MapScene>());
	}

	StageManager::GetInstance()->Update();

	player_->Update();

	// プレイヤーのワールド座標を渡す
	Camera2D::GetInstance()->ScreenMove(player_->GetWorldPosX(), player_->GetWorldPosY());

	// エンカウント判定(指定チップを踏んだ時に確率で遭遇)
	// 戦えるモンスターがいない／バトル結果の処理待ちのときは判定しない
	if (battle_ == nullptr && monsterParty_.HasAlive())
	{
		int monsterId, level;
		if (StageManager::GetInstance()->CheckEncounter(
			player_->GetWorldPosX(), player_->GetWorldPosY(), monsterId, level))
		{
			StartBattle(monsterId, level);
		}
	}
}

void GameScene::Draw(void)
{

	// 画面のズーム処理
	//SetDrawScreen(mainScreenId_);

	//ClearDrawScreen();

	StageManager::GetInstance()->Draw();
	player_->Draw();

#ifdef _DEBUG
	// デバッグ表示：今いるマスのチップ番号と、足元が通行不可かどうか
	{
		StageManager* sm = StageManager::GetInstance();
		int tx, ty, ground, object;
		sm->GetChipNos(player_->GetWorldPosX(), player_->GetWorldPosY(), tx, ty, ground, object);
		DrawFormatString(0, 60, 0xffff00, "Stage=%d Tile=(%d,%d) ground=%d object=%d",
			sm->GetCurrentStageId(), tx, ty, ground, object);
		// 16.0f は Player.cpp の FOOT_OFFSET_Y と同じ値
		DrawFormatString(0, 80, 0xffff00, "FootBlocked=%d",
			sm->IsBlocked(player_->GetWorldPosX(), player_->GetWorldPosY() + 16.0f) ? 1 : 0);
	}
#endif

	// 全滅演出（暗転とメッセージ）
	DrawDefeat();

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

	battle_ = nullptr;

	inventory_.Release();
	itemData_.Release();
	monsterData_.Release();
}

void GameScene::StartBattle(int monsterId, int level)
{
	battle_ = std::make_shared<BattleScene>();
	battle_->Setup(&monsterData_, &monsterParty_, monsterId, level);
	battle_->SetItems(&itemData_, &inventory_);		// バッグを使えるようにする
	SceneManager::GetInstance()->PushScene(battle_);
}

void GameScene::CheckBattleResult(void)
{
	if (battle_ == nullptr)
	{
		return;
	}

	const BattleScene::Result result = battle_->GetResult();
	if (result == BattleScene::Result::NONE)
	{
		return;		// まだ戦闘中
	}

	battle_ = nullptr;

	// 勝ち・逃げた・捕まえた は手持ちやバッグがバトル側で更新済みなので、ここでは何もしない
	if (result == BattleScene::Result::LOSE)
	{
		StartDefeat();
	}
}

void GameScene::StartDefeat(void)
{
	defeatPhase_ = DefeatPhase::FADE_OUT;
	defeatTimer_ = 0;
}

void GameScene::UpdateDefeat(void)
{
	switch (defeatPhase_)
	{
	case DefeatPhase::FADE_OUT:
		if (++defeatTimer_ >= DEFEAT_FADE_FRAMES)
		{
			// 真っ暗になったところで、裏で回復と移動を済ませる
			RecoverFromDefeat();
			defeatPhase_ = DefeatPhase::MESSAGE;
			defeatTimer_ = 0;
		}
		break;

	case DefeatPhase::MESSAGE:
		defeatTimer_++;
		if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
		{
			defeatPhase_ = DefeatPhase::FADE_IN;
			defeatTimer_ = 0;
		}
		break;

	case DefeatPhase::FADE_IN:
		if (++defeatTimer_ >= DEFEAT_FADE_FRAMES)
		{
			defeatPhase_ = DefeatPhase::NONE;
			defeatTimer_ = 0;
		}
		break;

	default:
		break;
	}
}

void GameScene::DrawDefeat(void) const
{
	if (defeatPhase_ == DefeatPhase::NONE)
	{
		return;
	}

	// 暗転の濃さ（0〜255）
	int alpha = 255;
	if (defeatPhase_ == DefeatPhase::FADE_OUT)
	{
		alpha = 255 * defeatTimer_ / DEFEAT_FADE_FRAMES;
	}
	else if (defeatPhase_ == DefeatPhase::FADE_IN)
	{
		alpha = 255 - 255 * defeatTimer_ / DEFEAT_FADE_FRAMES;
	}

	SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, GetColor(0, 0, 0), true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	if (defeatPhase_ != DefeatPhase::MESSAGE)
	{
		return;
	}

	// メッセージウィンドウ
	const int x1 = 40;
	const int x2 = Application::SCREEN_SIZE_X - 40;
	const int y1 = Application::SCREEN_SIZE_Y - 160;
	const int y2 = Application::SCREEN_SIZE_Y - 40;
	DrawBox(x1, y1, x2, y2, GetColor(255, 255, 255), true);
	DrawBox(x1, y1, x2, y2, GetColor(0, 0, 0), false);

	const int black = GetColor(0, 0, 0);
	DrawFormatString(x1 + 20, y1 + 25, black, "てもちの モンスターが みんな たおれてしまった…");
	DrawFormatString(x1 + 20, y1 + 55, black, "はじまりの まちに もどされ、モンスターは かいふくした！");

	// 点滅する「次へ」の合図
	if ((defeatTimer_ / 30) % 2 == 0)
	{
		DrawFormatString(x2 - 40, y2 - 28, black, "▼");
	}
}

void GameScene::RecoverFromDefeat(void)
{
	// 手持ちを全回復して、最初の街に戻す
	// ※回復所を作ったら「最後に寄った回復所」に戻すように差し替える
	HealParty();
	StageManager::GetInstance()->ChangeStage(0);
	player_->Init();		// 開始位置に戻す

	// 暗転中はUpdateが止まっているので、カメラもここで合わせておく
	Camera2D::GetInstance()->ScreenMove(player_->GetWorldPosX(), player_->GetWorldPosY());
}

void GameScene::HealParty(void)
{
	for (int i = 0; i < monsterParty_.GetCount(); i++)
	{
		MonsterInstance* mon = monsterParty_.Get(i);
		if (mon == nullptr)
		{
			continue;
		}

		mon->currentHp = MonsterParty::GetMaxHp(monsterData_, *mon);
		mon->ailment = StatusAilment::NONE;

		for (MoveSlot& slot : mon->moves)
		{
			if (slot.moveId == MonsterParty::MOVE_NONE)
			{
				continue;
			}
			const MoveData* move = monsterData_.GetMove(slot.moveId);
			if (move != nullptr)
			{
				slot.currentPp = move->pp;
			}
		}
	}
}

bool GameScene::SaveGame(void)
{
	// 保存先フォルダを作る（すでにあれば何もしない）
	CreateDirectoryA("Data/Save", NULL);

	// 手持ちとバッグ
	bool ok = true;
	ok = monsterParty_.Save("Data/Save/party.csv") && ok;
	ok = inventory_.Save("Data/Save/inventory.csv") && ok;

	// ゲームの進行状態（今いるステージとプレイヤーの位置）
	// いったん別名で書いてから差し替える（書き込み途中で失敗しても元のセーブが壊れない）
	const std::string path = "Data/Save/game.csv";
	const std::string tmpPath = path + ".tmp";
	{
		std::ofstream ofs(tmpPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;
		}

		ofs << "GAME,1\n";
		ofs << StageManager::GetInstance()->GetCurrentStageId() << ","
			<< player_->GetWorldPosX() << ","
			<< player_->GetWorldPosY() << "\n";

		ofs.flush();
		if (!ofs)
		{
			ofs.close();
			std::remove(tmpPath.c_str());
			return false;
		}
	}
	std::remove(path.c_str());
	ok = (std::rename(tmpPath.c_str(), path.c_str()) == 0) && ok;

	return ok;
}