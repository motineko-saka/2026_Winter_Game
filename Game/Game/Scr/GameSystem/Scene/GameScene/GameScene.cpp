#include "GameScene.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../TitleScene/TitleScene.h"
#include "../BattleScene/BattleScene.h"
#include "../MapScene/MapScene.h"
#include "../NovelScene/NovelScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Player/Player.h"
#include "../../../AppSystem/Camera/Camera2D.h"
#include "../../Object/Stage/StageManager.h"

namespace
{
	// 文字列を区切り文字で分ける（空の要素も残す）
	std::vector<std::string> SplitBy(const std::string& s, char sep)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (true)
		{
			const size_t pos = s.find(sep, start);
			if (pos == std::string::npos)
			{
				out.push_back(s.substr(start));
				break;
			}
			out.push_back(s.substr(start, pos - start));
			start = pos + 1;
		}
		return out;
	}

	// 歩きのコマを切り替えるピクセル数（これだけ歩くごとに次のコマ）
	const int TRAINER_WALK_ANIM_PIXELS = 12;

	// 「@」以降の指定を読み取る。  "ken.png@4" → 列4、 "ken.png@3x4" → 列3・行4
	// 指定がない、または書式が違うときは false（path などは変更しない）
	bool ParseSuffix(const std::string& spec, std::string& path, int& cols, int& rows)
	{
		const size_t at = spec.rfind('@');
		if (at == std::string::npos || at == 0 || at + 1 >= spec.size())
		{
			return false;
		}

		const std::string s = spec.substr(at + 1);
		try
		{
			size_t pos = 0;
			const int c = std::stoi(s, &pos);
			int r = 0;
			if (pos < s.size())
			{
				if (s[pos] != 'x' || pos + 1 >= s.size())
				{
					return false;
				}
				size_t pos2 = 0;
				r = std::stoi(s.substr(pos + 1), &pos2);
				if (pos + 1 + pos2 != s.size())
				{
					return false;
				}
			}
			if (c < 1 || c > 16 || r < 0 || r > 16)
			{
				return false;
			}
			path = spec.substr(0, at);
			cols = c;
			rows = r;
			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	// 1枚の画像を横に n 等分して読み込む（n が1なら普通に1枚）。失敗したら空
	std::vector<int> LoadFrames(const std::string& path, int n)
	{
		std::vector<int> out;
		const int whole = LoadGraph(path.c_str());
		if (whole < 0)
		{
			return out;
		}
		if (n <= 1)
		{
			out.push_back(whole);
			return out;
		}

		int w = 0;
		int h = 0;
		GetGraphSize(whole, &w, &h);
		DeleteGraph(whole);
		if (w < n || h <= 0)
		{
			return out;
		}

		std::vector<int> handles(n, -1);
		if (LoadDivGraph(path.c_str(), n, n, 1, w / n, h, handles.data()) != -1)
		{
			out = handles;
		}
		return out;
	}

	// 立ち絵＋歩きA＋歩きB のコマを作る（読み込めなかったものは飛ばす。全部ないなら空）
	// 立ち絵がないときは、歩きAを立ち絵として使う
	std::vector<int> BuildFrames(int stand, int walkA, int walkB)
	{
		std::vector<int> v;
		const int first = (stand >= 0) ? stand : ((walkA >= 0) ? walkA : walkB);
		if (first < 0)
		{
			return v;
		}
		v.push_back(first);
		if (walkA >= 0)
		{
			v.push_back(walkA);
		}
		if (walkB >= 0)
		{
			v.push_back(walkB);
		}
		return v;
	}

	// 画像の指定を読み込む（書き方は TrainerData.h のコメント参照）
	TrainerSprite LoadTrainerSprite(const std::string& spec)
	{
		TrainerSprite sp;
		if (spec.empty())
		{
			return sp;
		}

		// フォルダ指定（末尾が / ）：プレイヤーと同じファイル名の画像を読み込む
		//   Idle.png                … 立ち絵（下向き）
		//   Walk_1.png, Walk_2.png        … 下向きの歩き
		//   Back_Walk_1.png, Back_Walk_2.png … 上向きの歩き
		//   Beside_1.png, Beside_2.png    … 右向きの歩き（左向きは、これを左右反転して使う）
		//   省略できる立ち絵：Back_Idle.png（上向き）、Beside_Idle.png（横向き）。なければ歩きAを使う
		if (spec.back() == '/' || spec.back() == '\\')
		{
			auto load = [&spec](const char* name) { return LoadGraph((spec + name).c_str()); };

			const int idle = load("Idle.png");
			const int down1 = load("Walk_1.png");
			const int down2 = load("Walk_2.png");
			const int up1 = load("Back_Walk_1.png");
			const int up2 = load("Back_Walk_2.png");
			const int back = load("Back_Idle.png");
			const int side1 = load("Beside_1.png");
			const int side2 = load("Beside_2.png");
			const int sideIdle = load("Beside_Idle.png");

			const int UP = static_cast<int>(TrainerDir::UP);
			const int DOWN = static_cast<int>(TrainerDir::DOWN);
			const int LEFT = static_cast<int>(TrainerDir::LEFT);
			const int RIGHT = static_cast<int>(TrainerDir::RIGHT);

			sp.frames[DOWN] = BuildFrames(idle, down1, down2);
			sp.frames[UP] = BuildFrames(back, up1, up2);
			sp.frames[RIGHT] = BuildFrames(sideIdle, side1, side2);

			// 左向きは右向きと同じ画像を、左右反転して使う
			sp.frames[LEFT] = sp.frames[RIGHT];
			sp.mirror[LEFT] = true;
			return sp;
		}

		// 向きごとに指定：上|下|左|右 （それぞれ ken.png または ken.png@3）
		if (spec.find('|') != std::string::npos)
		{
			const std::vector<std::string> parts = SplitBy(spec, '|');
			for (size_t i = 0; i < sp.frames.size() && i < parts.size(); i++)
			{
				if (parts[i].empty())
				{
					continue;
				}
				std::string path = parts[i];
				int cols = 1;
				int rows = 0;
				ParseSuffix(parts[i], path, cols, rows);
				sp.frames[i] = LoadFrames(path, cols);
			}
			return sp;
		}

		std::string path;
		int cols = 1;
		int rows = 0;
		if (ParseSuffix(spec, path, cols, rows))
		{
			// ken.png@4 … 横に4等分。左から 上・下・左・右（歩きなし）
			if (rows == 0 && cols == 4)
			{
				const std::vector<int> handles = LoadFrames(path, 4);
				if (handles.size() == 4)
				{
					for (size_t i = 0; i < 4; i++)
					{
						sp.frames[i].push_back(handles[i]);
					}
				}
				return sp;
			}

			// ken.png@3x4 … 横が歩きのコマ、縦が 上・下・左・右
			if (rows == 4)
			{
				const int whole = LoadGraph(path.c_str());
				if (whole < 0)
				{
					return sp;
				}
				int w = 0;
				int h = 0;
				GetGraphSize(whole, &w, &h);
				DeleteGraph(whole);

				std::vector<int> handles(static_cast<size_t>(cols) * 4, -1);
				if (w >= cols && h >= 4
					&& LoadDivGraph(path.c_str(), cols * 4, cols, 4, w / cols, h / 4, handles.data()) != -1)
				{
					for (int d = 0; d < 4; d++)
					{
						sp.frames[d].assign(handles.begin() + d * cols, handles.begin() + (d + 1) * cols);
					}
				}
			}
			return sp;	// 上記以外の書き方は読み込まない
		}

		// 1枚だけ：どの向きでも同じ画像
		const int handle = LoadGraph(spec.c_str());
		if (handle >= 0)
		{
			for (auto& f : sp.frames)
			{
				f.push_back(handle);
			}
		}
		return sp;
	}

	// 表示するコマを選ぶ。立っているときは先頭のコマ。
	//   歩いているとき … 3コマ以上なら、先頭（立ち絵）を除いたコマを順に。2コマなら交互に。
	int PickFrame(const std::vector<int>& frames, bool walking, int walkedPixels)
	{
		const int n = static_cast<int>(frames.size());
		if (n == 0)
		{
			return -1;
		}
		if (!walking || n == 1)
		{
			return frames[0];
		}

		const int step = walkedPixels / TRAINER_WALK_ANIM_PIXELS;
		if (n == 2)
		{
			return frames[step % 2];
		}
		return frames[1 + step % (n - 1)];
	}

	// 向きを1マス分の移動量に直す
	void DirToVector(TrainerDir dir, int& dx, int& dy)
	{
		dx = 0;
		dy = 0;
		switch (dir)
		{
		case TrainerDir::UP:	dy = -1;	break;
		case TrainerDir::DOWN:	dy = 1;		break;
		case TrainerDir::LEFT:	dx = -1;	break;
		case TrainerDir::RIGHT:	dx = 1;		break;
		}
	}
}

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

	trainerData_.Init();
	defeatedTrainers_.clear();
	battleTrainerId_ = -1;
	lastStageId_ = -1;
	money_ = 0;
	trainerTileOverride_.clear();
	trainerEventPhase_ = TrainerEventPhase::NONE;

	// テスト用：Item.csv の ID 1〜5 を5個ずつ持たせる（存在するIDだけ）
	// ※本番では、ショップや拾得で増える形に差し替える
	for (int id = 1; id <= 15; id++)
	{
		if (itemData_.GetItem(id) != nullptr)
		{
			inventory_.Add(id, 5);
		}
	}

	// 図鑑番号7番のモンスターをLv5で手持ちに入れる
	//MonsterInstance starter;
	//if (MonsterParty::Create(monsterData_, 7, 5, starter))
	//{
	//	monsterParty_.Add(starter);
	//}
}

void GameScene::Load(void)
{
	StageManager::GetInstance()->CreateInstance();
	StageManager::GetInstance()->Load();

	player_ = new Player();

	player_->Load();

	monsterData_.Load();
	itemData_.Load();
	trainerData_.Load();
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

	// トレーナーに見つかった演出中は操作を受け付けない
	if (trainerEventPhase_ != TrainerEventPhase::NONE)
	{
		UpdateTrainerEvent();
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

	// ストーリーを再生（テスト用：Nキー。本番ではマップのイベントやNPCから StartStory を呼ぶ）
	if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_N))
	{
		StartStory("Data/Story/prologue.txt");
		return;
	}

	StageManager::GetInstance()->Update();

	player_->Update();

	// ステージが切り替わったら、トレーナーが立っているマスを通行不可にし直す
	if (StageManager::GetInstance()->GetCurrentStageId() != lastStageId_)
	{
		lastStageId_ = StageManager::GetInstance()->GetCurrentStageId();
		RebuildTrainerBlocks();
	}

	// プレイヤーのワールド座標を渡す
	Camera2D::GetInstance()->ScreenMove(player_->GetWorldPosX(), player_->GetWorldPosY());

	// エンカウント判定(指定チップを踏んだ時に確率で遭遇)
	// 戦えるモンスターがいない／バトル結果の処理待ちのときは判定しない
	if (battle_ == nullptr && monsterParty_.HasAlive())
	{
		// トレーナーに見つかったら勝負（野生のエンカウントより優先）
		if (CheckTrainerSight())
		{
			return;
		}

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
	DrawTrainers();
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

	// 同じ画像を複数の向きで使っていることがあるので、重複を除いてから削除する
	std::set<int> handles;
	for (auto& pair : trainerImages_)
	{
		for (const auto& frames : pair.second.frames)
		{
			for (int h : frames)
			{
				if (h >= 0)
				{
					handles.insert(h);
				}
			}
		}
	}
	for (int h : handles)
	{
		DeleteGraph(h);
	}
	trainerImages_.clear();
	trainerData_.Release();

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

void GameScene::StartStory(const std::string& scriptPath)
{
	auto novel = std::make_shared<NovelScene>();
	novel->Setup(scriptPath, &monsterData_, &monsterParty_, &itemData_, &inventory_);
	SceneManager::GetInstance()->PushScene(novel);
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

	const int prize = battle_->GetPrize();
	const int trainerId = battleTrainerId_;
	battle_ = nullptr;
	battleTrainerId_ = -1;

	// トレーナーに勝ったら、倒した記録をつけて賞金を受け取る
	if (result == BattleScene::Result::WIN && trainerId >= 0)
	{
		defeatedTrainers_.insert(trainerId);
		money_ += prize;
	}

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

		// 2行目：ステージID,X,Y,所持金
		ofs << "GAME,2\n";
		ofs << StageManager::GetInstance()->GetCurrentStageId() << ","
			<< player_->GetWorldPosX() << ","
			<< player_->GetWorldPosY() << ","
			<< money_ << "\n";

		// 3行目：倒したトレーナーのID（カンマ区切り。いなければ空行）
		bool firstId = true;
		for (int id : defeatedTrainers_)
		{
			if (!firstId)
			{
				ofs << ",";
			}
			ofs << id;
			firstId = false;
		}
		ofs << "\n";

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

void GameScene::StartTrainerBattle(const TrainerSpawn& spawn)
{
	const TrainerMasterData* trainer = trainerData_.GetTrainer(spawn.trainerId);
	if (trainer == nullptr)
	{
		return;
	}

	battleTrainerId_ = spawn.trainerId;
	battle_ = std::make_shared<BattleScene>();
	battle_->SetupTrainer(&monsterData_, &monsterParty_, trainer);
	battle_->SetItems(&itemData_, &inventory_);
	SceneManager::GetInstance()->PushScene(battle_);
}

bool GameScene::CheckTrainerSight(void)
{
	StageManager* sm = StageManager::GetInstance();
	const int csx = sm->GetChipSizeX();
	const int csy = sm->GetChipSizeY();

	// プレイヤーのいるマス（エンカウント判定と同じ座標を使う）
	const int px = static_cast<int>(player_->GetWorldPosX()) / csx;
	const int py = static_cast<int>(player_->GetWorldPosY()) / csy;
	const int stageId = sm->GetCurrentStageId();

	for (const TrainerSpawn& s : trainerData_.GetSpawns())
	{
		// 別のステージ、またはもう倒したトレーナーは見ない
		if (s.stageId != stageId || defeatedTrainers_.count(s.trainerId) > 0)
		{
			continue;
		}

		// トレーナーが今立っているマス（歩いて動いたあとは、そのマス）
		int ox = s.tileX;
		int oy = s.tileY;
		GetTrainerTile(s, ox, oy);

		int dx = 0;
		int dy = 0;
		switch (s.dir)
		{
		case TrainerDir::UP:	dy = -1;	break;
		case TrainerDir::DOWN:	dy = 1;		break;
		case TrainerDir::LEFT:	dx = -1;	break;
		case TrainerDir::RIGHT:	dx = 1;		break;
		}

		// 向いている方向に、視線の長さだけ1マスずつ調べる
		for (int k = 1; k <= s.sight; k++)
		{
			const int tx = ox + dx * k;
			const int ty = oy + dy * k;

			if (tx == px && ty == py)
			{
				// 見つかった！「！」を出して、プレイヤーの前まで歩いてから勝負を始める
				StartTrainerEvent(s, ox, oy, k - 1);
				return true;
			}

			// 壁やほかのトレーナーがあれば、その先は見えない
			if (sm->IsBlocked(static_cast<float>(tx * csx + csx / 2), static_cast<float>(ty * csy + csy / 2)))
			{
				break;
			}
		}
	}
	return false;
}

void GameScene::RebuildTrainerBlocks(void)
{
	const int stageId = StageManager::GetInstance()->GetCurrentStageId();

	std::vector<std::pair<int, int>> tiles;
	for (const TrainerSpawn& s : trainerData_.GetSpawns())
	{
		if (s.stageId == stageId)
		{
			int tx = s.tileX;
			int ty = s.tileY;
			GetTrainerTile(s, tx, ty);
			tiles.push_back(std::make_pair(tx, ty));
		}
	}
	StageManager::GetInstance()->SetExtraBlockTiles(tiles);
}

void GameScene::DrawTrainers(void)
{
	StageManager* sm = StageManager::GetInstance();
	const int csx = sm->GetChipSizeX();
	const int csy = sm->GetChipSizeY();
	const int stageId = sm->GetCurrentStageId();

	for (const TrainerSpawn& s : trainerData_.GetSpawns())
	{
		if (s.stageId != stageId)
		{
			continue;
		}

		int tileX = s.tileX;
		int tileY = s.tileY;
		GetTrainerTile(s, tileX, tileY);

		// 歩いてくる演出中のトレーナーは、歩いた分だけずらして描く
		int offX = 0;
		int offY = 0;
		const bool isEventTrainer = (trainerEventPhase_ != TrainerEventPhase::NONE && s.trainerId == eventSpawn_.trainerId);
		if (isEventTrainer)
		{
			int dx = 0;
			int dy = 0;
			DirToVector(s.dir, dx, dy);
			offX = dx * eventWalked_;
			offY = dy * eventWalked_;
		}

		const int x = Camera2D::GetInstance()->WorldToScreenX(tileX * csx + offX);
		const int y = Camera2D::GetInstance()->WorldToScreenY(tileY * csy + offY);

		const TrainerMasterData* trainer = trainerData_.GetTrainer(s.trainerId);
		const TrainerSprite& sprite = GetTrainerSprite((trainer != nullptr) ? trainer->imagePath : "");
		const bool walking = isEventTrainer && trainerEventPhase_ == TrainerEventPhase::WALK;
		const int img = PickFrame(sprite.frames[static_cast<int>(s.dir)], walking, eventWalked_);
		if (img >= 0)
		{
			if (sprite.mirror[static_cast<int>(s.dir)])
			{
				// 左右反転（x の左右を入れ替えて描く）
				DrawExtendGraph(x + csx, y, x, y + csy, img, true);
			}
			else
			{
				DrawExtendGraph(x, y, x + csx, y + csy, img, true);
			}
		}
		else
		{
			// 画像がないときは仮の四角（倒したあとは灰色）。向いている方向に白い印を出す
			const bool defeated = defeatedTrainers_.count(s.trainerId) > 0;
			DrawBox(x + 4, y + 4, x + csx - 4, y + csy - 4, defeated ? 0x888888 : 0xCC3333, true);

			int mx = x + csx / 2;
			int my = y + csy / 2;
			switch (s.dir)
			{
			case TrainerDir::UP:	my = y + 8;			break;
			case TrainerDir::DOWN:	my = y + csy - 8;	break;
			case TrainerDir::LEFT:	mx = x + 8;			break;
			case TrainerDir::RIGHT:	mx = x + csx - 8;	break;
			}
			DrawBox(mx - 3, my - 3, mx + 3, my + 3, 0xFFFFFF, true);
		}

		// 見つけた瞬間は、頭の上に「！」を出す
		if (isEventTrainer && trainerEventPhase_ == TrainerEventPhase::ALERT)
		{
			const int bx = x + csx / 2;
			const int by = y - 4;
			DrawBox(bx - 8, by - 22, bx + 8, by, 0xFFFFFF, true);
			DrawBox(bx - 8, by - 22, bx + 8, by, 0x000000, false);
			DrawFormatString(bx - 3, by - 19, 0xCC0000, "!");
		}
	}
}

const TrainerSprite& GameScene::GetTrainerSprite(const std::string& spec)
{
	auto it = trainerImages_.find(spec);
	if (it != trainerImages_.end())
	{
		return it->second;
	}

	// 失敗した向きは空のまま（仮の四角で表示される）。失敗も覚えておき、毎フレーム読み直さない
	return trainerImages_[spec] = LoadTrainerSprite(spec);
}

void GameScene::GetTrainerTile(const TrainerSpawn& spawn, int& tileX, int& tileY) const
{
	auto it = trainerTileOverride_.find(spawn.trainerId);
	if (it != trainerTileOverride_.end())
	{
		tileX = it->second.first;
		tileY = it->second.second;
	}
	else
	{
		tileX = spawn.tileX;
		tileY = spawn.tileY;
	}
}

void GameScene::StartTrainerEvent(const TrainerSpawn& spawn, int tileX, int tileY, int gap)
{
	eventSpawn_ = spawn;
	eventStartX_ = tileX;
	eventStartY_ = tileY;
	eventGap_ = std::max(0, gap);
	eventWalked_ = 0;
	eventTimer_ = 0;
	trainerEventPhase_ = TrainerEventPhase::ALERT;
}

void GameScene::UpdateTrainerEvent(void)
{
	switch (trainerEventPhase_)
	{
	case TrainerEventPhase::ALERT:
		if (++eventTimer_ >= TRAINER_ALERT_FRAMES)
		{
			trainerEventPhase_ = TrainerEventPhase::WALK;
		}
		break;

	case TrainerEventPhase::WALK:
	{
		StageManager* sm = StageManager::GetInstance();
		const bool horizontal = (eventSpawn_.dir == TrainerDir::LEFT || eventSpawn_.dir == TrainerDir::RIGHT);
		const int distance = eventGap_ * (horizontal ? sm->GetChipSizeX() : sm->GetChipSizeY());

		eventWalked_ += TRAINER_WALK_SPEED;
		if (eventWalked_ >= distance)
		{
			FinishTrainerEvent();
		}
		break;
	}

	default:
		break;
	}
}

void GameScene::FinishTrainerEvent(void)
{
	// 歩いた先のマスを、トレーナーの新しい立ち位置にする（通行不可マスも更新）
	int dx = 0;
	int dy = 0;
	DirToVector(eventSpawn_.dir, dx, dy);
	trainerTileOverride_[eventSpawn_.trainerId] =
		std::make_pair(eventStartX_ + dx * eventGap_, eventStartY_ + dy * eventGap_);
	RebuildTrainerBlocks();

	trainerEventPhase_ = TrainerEventPhase::NONE;
	eventWalked_ = 0;

	StartTrainerBattle(eventSpawn_);
}