#include "BattleScene.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <DxLib.h>
#include "../SceneManager.h"
#include "../PauseScene/PauseScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../../AppSystem/Application/Application.h"
#include "../../Object/Monster/MonsterParty.h"	
#include "../../Object/Monster/MonsterGrowth.h"
#include "../../Object/Item/ItemData.h"
#include "../../Object/Item/Inventory.h"
#include "../../Object/Item/ItemUse.h"
#include "../../Object/NPC/TrainerData.h"

namespace
{
	// ---------- 入力の共通化（キー割り当てをここで一元管理） ----------
	bool IsConfirm(InputManager* in)	// 決定：Enter / Space
	{
		return in->IsTrgDown(KEY_INPUT_RETURN) || in->IsTrgDown(KEY_INPUT_SPACE);
	}

	bool IsCancel(InputManager* in)		// 戻る・キャンセル：BackSpace / Esc
	{
		return in->IsTrgDown(KEY_INPUT_BACK) || in->IsTrgDown(KEY_INPUT_ESCAPE);
	}

	bool IsUp(InputManager* in)			// 上：↑ / W
	{
		return in->IsTrgDown(KEY_INPUT_UP) || in->IsTrgDown(KEY_INPUT_W);
	}

	bool IsDown(InputManager* in)		// 下：↓ / S
	{
		return in->IsTrgDown(KEY_INPUT_DOWN) || in->IsTrgDown(KEY_INPUT_S);
	}

	// スプライトシートを1コマずつに分割して読み込む（keyごとにキャッシュ）。画像なし・読み込み失敗ならnullptr
	const std::vector<int>* LoadSheetCached(std::unordered_map<int, std::vector<int>>& cache, int key,
		const std::string& path, int columns, int frameCount, int frameW, int frameH)
	{
		if (key <= 0 || path.empty() || frameW <= 0 || frameH <= 0)
		{
			return nullptr;
		}

		auto it = cache.find(key);
		if (it != cache.end())
		{
			return it->second.empty() ? nullptr : &it->second;
		}

		const int cols = std::max(1, columns);
		const int count = std::max(1, frameCount);
		const int rows = (count + cols - 1) / cols;
		std::vector<int> handles(count, -1);
		if (LoadDivGraph(path.c_str(), count, cols, rows, frameW, frameH, handles.data()) == -1)
		{
			handles.clear();	// 読み込み失敗：エフェクトなしで進める
		}

		std::vector<int>& slot = cache[key];
		slot = std::move(handles);
		return slot.empty() ? nullptr : &slot;
	}

	const int RANK_MAX = 6;

	// ランク補正の倍率（+1で1.5倍、-1で約0.67倍…）
	float RankMul(int rank)
	{
		return (rank >= 0) ? (2.0f + rank) / 2.0f : 2.0f / (2.0f - rank);
	}

	const char* AilmentText(StatusAilment a)
	{
		switch (a)
		{
		case StatusAilment::POISON:	return "どく";
		case StatusAilment::SLEEP:	return "ねむり";
		case StatusAilment::BURN:	return "やけど";
		case StatusAilment::DREAM:	return "ゆめ";
		case StatusAilment::COLD:	return "かぜ";
		default:					return "";
		}
	}
}

BattleScene::BattleScene(void)
	: rng_(std::random_device{}())
{
}

BattleScene::~BattleScene(void)
{
}

void BattleScene::Setup(const MonsterData* data, MonsterParty* party, int wildMonsterId, int wildLevel)
{
	data_ = data;
	party_ = party;
	wildId_ = wildMonsterId;
	wildLevel_ = wildLevel;
	trainer_ = nullptr;
	prize_ = 0;
}

void BattleScene::SetupTrainer(const MonsterData* data, MonsterParty* party, const TrainerMasterData* trainer)
{
	data_ = data;
	party_ = party;
	trainer_ = trainer;
	prize_ = 0;
}

void BattleScene::SetItems(const ItemData* items, Inventory* inventory)
{
	items_ = items;
	inventory_ = inventory;
}

void BattleScene::SetPlayerSprite(const std::string& path, int frames, int columns, int frameW, int frameH)
{
	playerSpritePath_ = path;
	playerSpriteFrames_ = frames;
	playerSpriteColumns_ = columns;
	playerSpriteW_ = frameW;
	playerSpriteH_ = frameH;
}

void BattleScene::Init(void)
{
	sendOut_ = {};
	catch_ = {};
	hidden_ = { false, false };
	steps_.clear();
	learnQueue_.clear();
	evolveQueue_.clear();
	evolveState_ = EvolveState::NONE;
	evolveBegun_ = false;
	needSwitch_ = false;
	runAttempts_ = 0;
	bagList_.clear();
	usingItemId_ = 0;
	itemTarget_ = 0;
	result_ = Result::NONE;
	cursor_ = 0;

	// わるあがき：PPが尽きたときの固定技（威力50、与えたダメージの1/4の反動）
	struggle_ = MoveData{};
	struggle_.id = -1;
	struggle_.name = "わるあがき";
	struggle_.type = ElementType::NONE;
	struggle_.power = 50;
	struggle_.accuracy = 0;
	struggle_.effect = MoveEffect::RECOIL;
	struggle_.effectValue = 25;
	struggle_.effectChance = 100;

	// 準備ができていない／戦えるモンスターがいない場合は開始しない
	int first = -1;
	if (party_ != nullptr)
	{
		for (int i = 0; i < party_->GetCount(); i++)
		{
			if (party_->Get(i)->currentHp > 0)
			{
				first = i;
				break;
			}
		}
	}
	if (data_ == nullptr || first < 0
		|| !CreateEnemy()
		|| data_->GetMonster(party_->Get(first)->monsterId) == nullptr)
	{
		result_ = Result::ESCAPED;
		phase_ = Phase::END;
		msg_ = "バトルを始められなかった…";
		return;
	}

	activeIndex_ = first;
	SetCombatant(PLAYER, party_->Get(first));
	SetCombatant(ENEMY, &enemy_);

	// ボール演出の設定を外部ファイルから読む（無ければ仮のボールで進む）
	ballData_.Load(BALL_EFFECT_CSV);

	// ボールから出てくるまでは姿を隠す（野生の敵は最初から見えている）
	hidden_[PLAYER] = true;
	hidden_[ENEMY] = (trainer_ != nullptr);

	// トレーナー戦：モンスターを出す前に、トレーナーの立ち絵を出す
	trainerImg_ = -1;
	showTrainer_ = false;
	if (trainer_ != nullptr)
	{
		trainerImg_ = LoadTrainerPortrait(trainer_->imagePath);
		showTrainer_ = (trainerImg_ != -1);
	}

	// 開始メッセージ
	if (trainer_ != nullptr)
	{
		Say(trainer_->name + "が しょうぶを しかけてきた！");
		if (!trainer_->introMessage.empty())
		{
			PushSay(trainer_->name + "「" + trainer_->introMessage + "」");
		}
		Push([this]()
			{
				showTrainer_ = false;	// ここで立ち絵は消える（ボールを投げる演出へ）
				Say(trainer_->name + "は " + enemy_.nickname + "を くりだした！");
				StartSendOut(ENEMY);
			});
	}
	else
	{
		Say(Name(ENEMY) + "が あらわれた！");
	}
	Push([this]()
		{
			Say("ゆけっ！ " + side_[PLAYER].mon->nickname + "！");
			StartSendOut(PLAYER);
		});
	phase_ = Phase::MESSAGE;
}

void BattleScene::Load(void)
{
}

void BattleScene::LoadEnd(void)
{
}

void BattleScene::Update(void)
{
	UpdateMoveAnim();
	UpdateSendOut();
	UpdateCatch();
	UpdateHpBars();

	// ポーズ画面を積む
	//if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_ESCAPE))
	//{
	//	SceneManager::GetInstance()->PushScene(std::make_shared<PauseScene>());
	//	return;
	//}

	switch (phase_)
	{
	case Phase::MESSAGE:
		// 技・ボールの演出中はEnterを受け付けない（演出が終わってから次へ進める）
		if (!moveAnim_.active && !sendOut_.active && !catch_.active && IsConfirm(InputManager::GetInstance()))
		{
			RunNextStep();
		}
		break;
	case Phase::COMMAND:
		UpdateCommand();
		break;
	case Phase::MOVE_SELECT:
		UpdateMoveSelect();
		break;
	case Phase::PARTY_SELECT:
		UpdatePartySelect();
		break;
	case Phase::LEARN_SELECT:
		UpdateLearnSelect();
		break;
	case Phase::BAG:
		UpdateBag();
		break;
	case Phase::ITEM_TARGET:
		UpdateItemTarget();
		break;
	case Phase::EVOLVING:
		UpdateEvolve();
		break;
	case Phase::EVOLVE_SELECT:
		UpdateEvolveSelect();
		break;
	case Phase::END:
		if (IsConfirm(InputManager::GetInstance()))
		{
			// バトルが完全に終わったあとで進化する（進化がなければそのままゲーム画面に戻る）
			evolveBegun_ = true;
			if (!StartNextEvolve())
			{
				SceneManager::GetInstance()->PopScene();
			}
		}
		break;
	}
}

void BattleScene::Draw(void)
{
	// 現在の描画先サイズを取得（画面サイズ変更にも追従）
	GetDrawScreenSize(&screenW_, &screenH_);

	// 背景（画面全体）
	DrawBox(0, 0, screenW_, screenH_, GetColor(190, 230, 190), TRUE);

	// 進化中は専用の画面
	if (evolveState_ != EvolveState::NONE)
	{
		DrawEvolve();
		return;
	}

	// モンスター画像（敵＝右上、味方＝左下）
	// ボールから出てくるまでは描かない（出てくる最中は DrawSendOut が描く）
	for (int side : { ENEMY, PLAYER })
	{
		if (side == ENEMY && catch_.active)
		{
			continue;
		}
		if (!hidden_[side])
		{
			int cx = 0, bottom = 0, size = 0;
			GetMonsterLayout(side, cx, bottom, size);
			DrawMonsterImage(side, cx, bottom, size);
		}
	}
	DrawCatch();	// 捕まえる演出
	DrawSendOut();	// ボールの演出
	DrawMoveAnim();	// 技エフェクト（ステータス枠より下に描く）
	DrawTrainerPortrait();	// トレーナーの立ち絵（戦闘の最初と、勝ったあと）

	// 敵（左上）とプレイヤー（右下）の情報
	if (!hidden_[ENEMY])
	{
		DrawStatusBox(ENEMY, SX(30), SY(30));
	}
	if (!hidden_[PLAYER])
	{
		DrawStatusBox(PLAYER, SX(370), SY(250));
	}

	switch (phase_)
	{
	case Phase::MESSAGE:
	case Phase::END:
		DrawMessageBox(msg_);
		break;
	case Phase::COMMAND:
		DrawMessageBox(side_[PLAYER].mon->nickname + " は どうする？");
		DrawCommandMenu();
		break;
	case Phase::MOVE_SELECT:
		DrawMoveMenu();
		break;
	case Phase::PARTY_SELECT:
		DrawMessageBox(needSwitch_ ? "どのモンスターを出す？" : "だれと入れ替える？");
		DrawPartyMenu();
		break;
	case Phase::LEARN_SELECT:
		DrawLearnMenu();
		break;
	case Phase::BAG:
		DrawBagMenu();
		break;
	case Phase::ITEM_TARGET:
		DrawMessageBox("だれに つかう？");
		DrawPartyMenu();
		break;
	case Phase::EVOLVING:
	case Phase::EVOLVE_SELECT:
		break;	// DrawEvolveで描画済み
	}
}

void BattleScene::Release(void)
{
	for (auto& pair : images_)
	{
		if (pair.second != -1)
		{
			DeleteGraph(pair.second);
		}
	}
	for (auto& pair : animFrames_)
	{
		for (int h : pair.second)
		{
			if (h != -1)
			{
				DeleteGraph(h);
			}
		}
	}
	animFrames_.clear();
	for (auto& pair : ballFrames_)
	{
		for (int h : pair.second)
		{
			if (h != -1)
			{
				DeleteGraph(h);
			}
		}
	}
	ballFrames_.clear();
	for (auto& pair : catchFrames_)
	{
		for (int h : pair.second)
		{
			if (h != -1)
			{
				DeleteGraph(h);
			}
		}
	}
	catchFrames_.clear();
	catch_ = {};
	for (int h : playerSprite_)
	{
		if (h != -1)
		{
			DeleteGraph(h);
		}
	}
	playerSprite_.clear();
	sendOut_ = {};
	moveAnim_ = {};
	images_.clear();
	steps_.clear();
	learnQueue_.clear();
	evolveQueue_.clear();
	evolveState_ = EvolveState::NONE;
	evolveBegun_ = false;
	bagList_.clear();
	side_ = {};
}

// =============================================================
// 進行管理
// =============================================================

void BattleScene::Say(const std::string& text)
{
	msg_ = text;
	said_ = true;
}

void BattleScene::Push(Step step)
{
	// 実行中のステップから呼ばれた順に、先頭へ並べて差し込む
	steps_.insert(steps_.begin() + insertIdx_, std::move(step));
	insertIdx_++;
}

void BattleScene::PushSay(const std::string& text)
{
	Push([this, text]() { Say(text); });
}

void BattleScene::PushFaint(int side)
{
	Push([this, side]() { OnFaint(side); });
}

void BattleScene::RunNextStep(void)
{
	// メッセージが出るまで、メッセージを出さないステップは続けて実行する
	said_ = false;
	while (!said_ && !steps_.empty())
	{
		Step step = std::move(steps_.front());
		steps_.pop_front();
		insertIdx_ = 0;
		step();
	}
	if (!said_)
	{
		Finish();
	}
}

void BattleScene::StartMessages(void)
{
	phase_ = Phase::MESSAGE;
	RunNextStep();
}

void BattleScene::Finish(void)
{
	cursor_ = 0;

	// 進化の進行：「おや…？」のあとは演出へ、結果メッセージのあとは通常の流れへ戻る
	if (evolveState_ == EvolveState::INTRO)
	{
		if (evolveCur_.candidates.size() > 1)
		{
			// 分岐進化：進化先を選ばせる
			evolveState_ = EvolveState::SELECT;
			phase_ = Phase::EVOLVE_SELECT;
			return;
		}
		BeginEvolveAnim();
		return;
	}
	if (evolveState_ == EvolveState::RESULT)
	{
		evolveState_ = EvolveState::NONE;
	}

	// ひんしになったので、次に出すモンスターを選ばせる
	if (needSwitch_ && result_ == Result::NONE)
	{
		phase_ = Phase::PARTY_SELECT;
		for (int i = 0; i < party_->GetCount(); i++)
		{
			if (party_->Get(i)->currentHp > 0)
			{
				cursor_ = i;
				break;
			}
		}
		return;
	}

	// 技を覚えたいが枠が埋まっている
	if (!learnQueue_.empty())
	{
		phase_ = Phase::LEARN_SELECT;
		return;
	}

	if (result_ != Result::NONE)
	{
		// 戦闘終了後の進化の最中：次の進化へ。全部終わったらシーンを閉じる
		if (evolveBegun_)
		{
			if (!StartNextEvolve())
			{
				SceneManager::GetInstance()->PopScene();
			}
			return;
		}
		phase_ = Phase::END;
		return;
	}

	phase_ = Phase::COMMAND;
}

// =============================================================
// ターン処理
// =============================================================

void BattleScene::BeginTurn(ActionType type, int arg)
{
	steps_.clear();
	insertIdx_ = 0;

	const int enemySlot = ChooseEnemyMove();

	if (type == ActionType::SWITCH)
	{
		// 交代は技より先
		Push([this, arg]() { SwitchPlayer(arg); });
		Push([this, enemySlot]() { ExecMove(ENEMY, enemySlot); });
	}
	else if (type == ActionType::ITEM)
	{
		// アイテム：arg＝アイテムID。ボールが成功すれば result_ が CAUGHT になり、敵の行動は行われない
		const int target = itemTarget_;
		Push([this, arg, target]() { UseItem(arg, target); });
		Push([this, enemySlot]() { ExecMove(ENEMY, enemySlot); });
	}
	else if (type == ActionType::RUN)
	{
		Push([this]()
			{
				runAttempts_++;
				const int ps = MonsterParty::CalcSpeed(*side_[PLAYER].master, side_[PLAYER].mon->level);
				const int es = MonsterParty::CalcSpeed(*side_[ENEMY].master, side_[ENEMY].mon->level);
				const int f = ps * 128 / std::max(1, es) + 30 * runAttempts_;
				if (f >= 256 || Rand(0, 255) < f)
				{
					result_ = Result::ESCAPED;
					Say("うまく にげきれた！");
				}
				else
				{
					Say("にげられない！");
				}
			});
		Push([this, enemySlot]() { ExecMove(ENEMY, enemySlot); });
	}
	else
	{
		// 技：優先度 → 素早さの順（同じならランダム）
		const int slot = arg;
		const int pPrio = GetPriority(PLAYER, slot);
		const int ePrio = GetPriority(ENEMY, enemySlot);
		bool playerFirst;
		if (pPrio != ePrio)
		{
			playerFirst = pPrio > ePrio;
		}
		else
		{
			const int ps = MonsterParty::CalcSpeed(*side_[PLAYER].master, side_[PLAYER].mon->level);
			const int es = MonsterParty::CalcSpeed(*side_[ENEMY].master, side_[ENEMY].mon->level);
			playerFirst = (ps != es) ? (ps > es) : (Rand(0, 1) == 0);
		}

		if (playerFirst)
		{
			Push([this, slot]() { ExecMove(PLAYER, slot); });
			Push([this, enemySlot]() { ExecMove(ENEMY, enemySlot); });
		}
		else
		{
			Push([this, enemySlot]() { ExecMove(ENEMY, enemySlot); });
			Push([this, slot]() { ExecMove(PLAYER, slot); });
		}
	}

	// ターン終了時の状態異常ダメージ
	Push([this]() { EndOfTurnDamage(PLAYER); });
	Push([this]() { EndOfTurnDamage(ENEMY); });

	StartMessages();
}

int BattleScene::ChooseEnemyMove(void)
{
	std::vector<int> usable;
	for (size_t i = 0; i < enemy_.moves.size(); i++)
	{
		if (enemy_.moves[i].moveId != MonsterParty::MOVE_NONE && enemy_.moves[i].currentPp > 0)
		{
			usable.push_back(static_cast<int>(i));
		}
	}
	if (usable.empty())
	{
		return -1;	// わるあがき
	}
	return usable[Rand(0, static_cast<int>(usable.size()) - 1)];
}

int BattleScene::GetPriority(int side, int slot) const
{
	if (slot < 0)
	{
		return 0;
	}
	const MoveData* move = data_->GetMove(side_[side].mon->moves[slot].moveId);
	return (move != nullptr) ? move->priority : 0;
}

bool BattleScene::CanCatch(std::string& reason) const
{
	if (trainer_ != nullptr)
	{
		reason = "ひとの モンスターは とれない！";
		return false;
	}
	if (side_[ENEMY].master->catchRate <= 0)
	{
		reason = "この モンスターは つかまえられない！";
		return false;
	}
	if (party_->IsFull())
	{
		reason = "てもちが いっぱいで つかまえられない！";
		return false;
	}
	return true;
}

// アイテムを使う（BeginTurn のステップとして呼ばれる）
// 効果の中身は Item.csv の「効果」「効果値」で決まる。
void BattleScene::UseItem(int itemId, int partyIndex)
{
	const ItemMasterData* item = (items_ != nullptr) ? items_->GetItem(itemId) : nullptr;
	if (item == nullptr || inventory_ == nullptr || !inventory_->Has(itemId))
	{
		Say("しかし なにも おこらなかった！");
		return;
	}
	if (item->consumable)
	{
		inventory_->Remove(itemId);
	}

	switch (item->effect)
	{
	case ItemEffect::CATCH:
		TryCatch(itemId, item->effectValue / 100.0, item->name);
		return;

	case ItemEffect::ATK_UP:
	case ItemEffect::DEF_UP:
		Say(item->name + "を つかった！");
		ChangeRank(PLAYER, item->effect == ItemEffect::ATK_UP, item->effectValue);
		return;

	case ItemEffect::HEAL_HP:
	case ItemEffect::REVIVE:
	case ItemEffect::CURE_STATUS:
	case ItemEffect::RESTORE_PP:
	{
		MonsterInstance* target = party_->Get(partyIndex);
		std::string msg;
		Say(item->name + "を つかった！");
		if (target != nullptr && ItemUse::ApplyToMonster(*data_, *item, *target, msg))
		{
			PushSay(msg);
		}
		else
		{
			PushSay("しかし なにも おこらなかった！");
		}
		return;
	}

	default:
		Say("しかし なにも おこらなかった！");
		return;
	}
}

// バッグに出すのは「持っていて、戦闘中に使える」アイテムだけ
void BattleScene::RefreshBagList(void)
{
	bagList_.clear();
	if (items_ == nullptr || inventory_ == nullptr)
	{
		return;
	}
	for (const auto& pair : inventory_->GetAll())
	{
		const ItemMasterData* item = items_->GetItem(pair.first);
		if (item != nullptr && item->usableInBattle && pair.second > 0)
		{
			bagList_.push_back(pair.first);
		}
	}
}

// 捕獲判定（ポケモン第3世代に近い式）
//  捕獲値 a = (3*最大HP - 2*現在HP) * 捕獲率 * ボール補正 * 状態異常補正 / (3*最大HP)
//  a >= 255 なら確定で捕獲。そうでなければ4回のゆれ判定をすべて通れば捕獲。
//  捕獲率は 1〜255 の想定（MonsterMaster.csv の値）。
void BattleScene::TryCatch(int itemId, double ballRate, const std::string& ballName)
{
	Combatant& e = side_[ENEMY];
	const std::string name = e.mon->nickname;

	Say(ballName + "を なげた！");

	const double maxHp = std::max(1, MonsterParty::GetMaxHp(*data_, *e.mon));
	const double hp = std::max(1, e.mon->currentHp);

	double statusRate = 1.0;
	if (e.mon->ailment == StatusAilment::SLEEP)
	{
		statusRate = 2.0;
	}
	else if (e.mon->ailment != StatusAilment::NONE)
	{
		statusRate = 1.5;
	}

	double a = (3.0 * maxHp - 2.0 * hp) * e.master->catchRate * ballRate * statusRate / (3.0 * maxHp);
	a = std::max(1.0, a);

	// ゆれる回数（0〜3）を決める。4回目まで通れば捕獲成功
	int shakes = 0;
	bool caught = false;
	if (a >= 255.0)
	{
		caught = true;
		shakes = 3;
	}
	else
	{
		const double b = 1048560.0 / std::sqrt(std::sqrt(16711680.0 / a));
		while (shakes < 4 && Rand(0, 65535) < b)
		{
			shakes++;
		}
		caught = (shakes >= 4);
		shakes = std::min(shakes, 3);
	}

	// ボールを投げて捕まえる演出（ゆれる回数もここで見せる。終わるまでEnterは効かない）
	StartCatch(itemId, shakes, caught);

	if (caught)
	{
		// 手持ちに加える（満員のときはそもそも投げられない）
		party_->Add(enemy_);
		// 手持ちの配列が再確保されても大丈夫なように、味方側のポインタを取り直す
		side_[PLAYER].mon = party_->Get(activeIndex_);
		result_ = Result::CAUGHT;
		PushSay("やった！ " + name + "を つかまえた！");
		return;
	}

	static const char* const failMsg[4] =
	{
		"ああっ！ ボールから でてしまった！",
		"ううっ！ ボールから でてしまった！",
		"ざんねん！ あと すこしだったのに！",
		"おしい！ もうすこしで つかまえられたのに！",
	};
	PushSay(failMsg[shakes]);	// 出てきたあとのメッセージ
}

void BattleScene::ExecMove(int atk, int slot)
{
	const int def = 1 - atk;
	Combatant& a = side_[atk];
	Combatant& d = side_[def];

	// 戦闘が終わっている、どちらかがひんしなら何もしない
	if (result_ != Result::NONE || a.mon->currentHp <= 0 || d.mon->currentHp <= 0)
	{
		return;
	}

	// 睡眠：残りターンの間は動けない（起きたターンも動けない）
	if (a.mon->ailment == StatusAilment::SLEEP)
	{
		if (a.sleepTurns <= 0)
		{
			a.sleepTurns = Rand(1, 3);
		}
		a.sleepTurns--;
		if (a.sleepTurns > 0)
		{
			Say(Name(atk) + "は ぐうぐう ねむっている。");
		}
		else
		{
			a.mon->ailment = StatusAilment::NONE;
			Say(Name(atk) + "は めをさました！");
		}
		return;
	}

	// 使う技を決める
	const MoveData* move = &struggle_;
	if (slot >= 0)
	{
		MoveSlot& ms = a.mon->moves[slot];
		move = data_->GetMove(ms.moveId);
		if (move == nullptr)
		{
			return;
		}
		if (ms.currentPp > 0)
		{
			ms.currentPp--;
		}
	}

	Say(Name(atk) + "の " + move->name + "！");

	// 命中判定（自分に使う変化技と、命中率0の技は必中）
	const bool selfMove = (move->power <= 0)
		&& (move->effect == MoveEffect::ATK_UP || move->effect == MoveEffect::HEAL);
	if (!selfMove && move->accuracy > 0 && Rand(1, 100) > move->accuracy)
	{
		PushSay(Name(atk) + "の こうげきは はずれた！");
		return;
	}

	// 技の演出を開始（前に出る → エフェクト。命中したときだけ）
	StartMoveAnim(*move, atk);

	// ダメージ計算
	int dealt = 0;
	if (move->power > 0)
	{
		const float eff = data_->GetTypeEffectiveness(move->type, d.master->type1, d.master->type2);
		if (eff <= 0.0f)
		{
			PushSay("こうかが ないようだ…");
			return;
		}

		const bool crit = (Rand(1, 24) == 1);
		const int level = a.mon->level;

		float attack = MonsterParty::CalcAttack(*a.master, level) * RankMul(a.atkRank);
		if (a.mon->ailment == StatusAilment::BURN)
		{
			attack *= 0.5f;	// やけどで攻撃半減
		}
		const float defense = MonsterParty::CalcDefense(*d.master, d.mon->level) * RankMul(d.defRank);

		float dmg = (2.0f * level / 5.0f + 2.0f) * move->power * attack / std::max(1.0f, defense) / 50.0f + 2.0f;

		// タイプ一致ボーナス
		if (move->type != ElementType::NONE && (move->type == a.master->type1 || move->type == a.master->type2))
		{
			dmg *= 1.5f;
		}
		dmg *= eff;
		if (crit)
		{
			dmg *= 1.5f;
		}
		dmg *= Rand(85, 100) / 100.0f;

		dealt = std::max(1, static_cast<int>(dmg));
		dealt = std::min(dealt, d.mon->currentHp);
		d.mon->currentHp -= dealt;
		if (moveAnim_.active)
		{
			moveAnim_.hit = true;	// ダメージが入った：エフェクトのあとに、相手が赤く点滅して揺れる
		}

		if (crit)
		{
			PushSay("きゅうしょに あたった！");
		}
		if (eff > 1.0f)
		{
			PushSay("エレガントな一撃！");
		}
		else if (eff < 1.0f)
		{
			PushSay("こうかは いまひとつのようだ…");
		}
	}

	// 追加効果
	ApplyEffect(atk, *move, dealt);

	// ひんし判定（相手→自分の順）
	if (d.mon->currentHp <= 0)
	{
		PushFaint(def);
	}
	if (a.mon->currentHp <= 0)
	{
		PushFaint(atk);
	}
}

void BattleScene::ApplyEffect(int atk, const MoveData& move, int dealt)
{
	if (move.effect == MoveEffect::NONE)
	{
		return;
	}

	const int def = 1 - atk;
	Combatant& a = side_[atk];
	Combatant& d = side_[def];

	// 反動：与えたダメージの effectValue %（必ず発動）
	if (move.effect == MoveEffect::RECOIL)
	{
		if (dealt <= 0)
		{
			return;
		}
		int recoil = std::max(1, dealt * move.effectValue / 100);
		recoil = std::min(recoil, a.mon->currentHp);
		a.mon->currentHp -= recoil;
		PushSay(Name(atk) + "は はんどうを うけた！");
		return;
	}

	// 発動確率（変化技で0なら必ず発動）
	const bool isStatusMove = (move.power <= 0);
	int chance = move.effectChance;
	if (isStatusMove && chance <= 0)
	{
		chance = 100;
	}
	if (Rand(1, 100) > chance)
	{
		if (isStatusMove)
		{
			PushSay("しかし うまく きまらなかった！");
		}
		return;
	}

	const int value = std::max(1, move.effectValue);

	switch (move.effect)
	{
	case MoveEffect::POISON:
		if (d.mon->currentHp <= 0)
		{
			return;
		}
		if (d.mon->ailment != StatusAilment::NONE)
		{
			if (isStatusMove)
			{
				PushSay("しかし うまく きまらなかった！");
			}
			return;
		}
		d.mon->ailment = StatusAilment::POISON;
		PushSay(Name(def) + "は どくを あびた！");
		break;

	case MoveEffect::PARALYSIS:
		// StatusAilment に麻痺がまだ無いので、今は何も起きない
		break;

	case MoveEffect::ATK_UP:
		ChangeRank(atk, true, value);
		break;

	case MoveEffect::DEF_DOWN:
		if (d.mon->currentHp > 0)
		{
			ChangeRank(def, false, -value);
		}
		break;

	case MoveEffect::HEAL:
	{
		const int maxHp = MonsterParty::CalcMaxHp(*a.master, a.mon->level);
		const int amount = (move.effectValue > 0) ? move.effectValue : maxHp / 2;
		const int before = a.mon->currentHp;
		a.mon->currentHp = std::min(maxHp, a.mon->currentHp + amount);
		if (a.mon->currentHp == before)
		{
			PushSay(Name(atk) + "の HPは まんたんだ！");
		}
		else
		{
			PushSay(Name(atk) + "は たいりょくを かいふくした！");
		}
		break;
	}

	default:
		break;
	}
}

void BattleScene::ChangeRank(int side, bool isAttack, int delta)
{
	Combatant& c = side_[side];
	int& rank = isAttack ? c.atkRank : c.defRank;
	const int before = rank;
	rank = std::max(-RANK_MAX, std::min(rank + delta, RANK_MAX));

	const std::string stat = isAttack ? "こうげき" : "ぼうぎょ";
	if (rank == before)
	{
		PushSay(Name(side) + "の " + stat + "は もう " + (delta > 0 ? "あがらない！" : "さがらない！"));
		return;
	}
	PushSay(Name(side) + "の " + stat + "が "
		+ (std::abs(delta) >= 2 ? "ぐーんと " : "") + (delta > 0 ? "あがった！" : "さがった！"));
}

void BattleScene::EndOfTurnDamage(int side)
{
	if (result_ != Result::NONE)
	{
		return;
	}
	Combatant& c = side_[side];
	if (c.mon->currentHp <= 0)
	{
		return;
	}

	int div = 0;
	std::string text;
	if (c.mon->ailment == StatusAilment::POISON)
	{
		div = 8;
		text = "は どくの ダメージを うけている！";
	}
	else if (c.mon->ailment == StatusAilment::BURN)
	{
		div = 16;
		text = "は やけどの ダメージを うけている！";
	}
	if (div == 0)
	{
		return;
	}

	const int maxHp = MonsterParty::CalcMaxHp(*c.master, c.mon->level);
	const int dmg = std::min(std::max(1, maxHp / div), c.mon->currentHp);
	c.mon->currentHp -= dmg;
	Say(Name(side) + text);
	if (c.mon->currentHp <= 0)
	{
		PushFaint(side);
	}
}

void BattleScene::OnFaint(int side)
{
	Say(Name(side) + "は たおれた！");

	if (side == ENEMY)
	{
		// トレーナー戦では、経験値が増える（第3引数＝トレーナー戦かどうか）
		const int exp = MonsterGrowth::CalcGainExp(*side_[ENEMY].master, side_[ENEMY].mon->level, trainer_ != nullptr);

		// トレーナーに次のモンスターがいるなら、経験値のあとに次を出す
		if (trainer_ != nullptr && result_ == Result::NONE && HasNextEnemy())
		{
			Push([this, exp]() { GainPlayerExp(exp); });
			Push([this]() { SendNextEnemy(); });
			return;
		}

		if (result_ == Result::NONE)
		{
			result_ = Result::WIN;
		}
		Push([this, exp]() { GainPlayerExp(exp); });
		if (trainer_ != nullptr)
		{
			Push([this]() { OnTrainerDefeated(); });
		}
		return;
	}

	if (result_ != Result::NONE)
	{
		return;
	}
	if (party_->HasAlive())
	{
		needSwitch_ = true;
	}
	else
	{
		result_ = Result::LOSE;
		PushSay("めのまえが まっくらになった…");
	}
}

void BattleScene::GainPlayerExp(int amount)
{
	MonsterInstance* p = side_[PLAYER].mon;
	if (p == nullptr || p->currentHp <= 0)
	{
		return;
	}

	const GrowthResult r = MonsterGrowth::GainExp(*data_, *p, amount);
	if (r.gainedExp <= 0)
	{
		return;	// 最大レベル
	}
	Say(p->nickname + "は " + std::to_string(r.gainedExp) + " けいけんちを もらった！");

	if (r.newLevel > r.oldLevel)
	{
		PushSay(p->nickname + "は レベル" + std::to_string(r.newLevel) + "に あがった！");
		CheckEvolution(*p, r.newLevel);	// 実際の進化はバトル終了後（ENDでEnterを押したあと）
	}
	for (int moveId : r.learnedMoves)
	{
		const MoveData* m = data_->GetMove(moveId);
		if (m != nullptr)
		{
			PushSay(p->nickname + "は " + m->name + "を おぼえた！");
		}
	}
	for (int moveId : r.pendingMoves)
	{
		learnQueue_.push_back(moveId);	// 戦闘後の技選択で聞く
	}
}

// レベルアップで進化できるか調べる（アイテム進化は戦闘では扱わない）
// 条件を満たす進化先が複数あれば、すべて候補として持ち、あとで選ばせる。
void BattleScene::CheckEvolution(MonsterInstance& mon, int level)
{
	const MonsterMasterData* master = data_->GetMonster(mon.monsterId);
	if (master == nullptr)
	{
		return;
	}

	EvolveEntry entry;
	for (const Evolution& e : master->evolutions)
	{
		if (e.itemId != 0 || e.level <= 0 || e.level > level)
		{
			continue;
		}
		if (data_->GetMonster(e.toId) == nullptr)
		{
			continue;
		}
		if (std::find(entry.candidates.begin(), entry.candidates.end(), e.toId) != entry.candidates.end())
		{
			continue;	// 同じ進化先の重複は無視
		}
		entry.candidates.push_back(e.toId);
	}

	if (entry.candidates.empty())
	{
		return;
	}
	entry.mon = &mon;
	entry.toId = entry.candidates.front();
	evolveQueue_.push_back(entry);
}

bool BattleScene::StartNextEvolve(void)
{
	while (!evolveQueue_.empty())
	{
		if (StartEvolve())
		{
			return true;
		}
	}
	return false;
}

bool BattleScene::StartEvolve(void)
{
	evolveCur_ = evolveQueue_.front();
	evolveQueue_.pop_front();

	if (evolveCur_.mon == nullptr || evolveCur_.candidates.empty())
	{
		return false;
	}
	const MonsterMasterData* oldMaster = data_->GetMonster(evolveCur_.mon->monsterId);
	if (oldMaster == nullptr)
	{
		return false;
	}

	evolveOldImg_ = LoadMonsterImage(oldMaster->frontPath);	// 進化画面は正面画像

	// 候補の正面画像を先に読んでおく（選択画面と演出で使う）
	evolveCandImgs_.clear();
	for (int id : evolveCur_.candidates)
	{
		evolveCandImgs_.push_back(LoadMonsterImage(data_->GetMonster(id)->frontPath));
	}

	// 候補が1つならそれで確定、複数なら選択後に上書きされる
	evolveCur_.toId = evolveCur_.candidates.front();
	evolveNewImg_ = evolveCandImgs_.front();

	evolved_ = false;
	evolveShowNew_ = false;
	evolveState_ = EvolveState::INTRO;

	phase_ = Phase::MESSAGE;
	Say("おや…？ " + evolveCur_.mon->nickname + "の ようすが…！");
	return true;
}

void BattleScene::BeginEvolveAnim(void)
{
	evolveState_ = EvolveState::ANIM;
	evolveTimer_ = 0;
	evolveFlip_ = 22;
	evolveShowNew_ = false;
	phase_ = Phase::EVOLVING;
}

// 次のレベルアップでまた進化できる
void BattleScene::CancelEvolve(void)
{
	evolved_ = false;
	evolveState_ = EvolveState::RESULT;
	steps_.clear();
	insertIdx_ = 0;
	PushSay("あれ…？ " + evolveCur_.mon->nickname + "の へんかが とまった！");
	StartMessages();
}

// 進化後の種族が「今のレベルちょうど」で覚える技を習得させる。
// 空き枠があればそのまま覚え、埋まっていれば忘れる技の選択（learnQueue_）に回す。
void BattleScene::LearnEvolutionMoves(MonsterInstance& mon)
{
	const MonsterMasterData* master = data_->GetMonster(mon.monsterId);
	if (master == nullptr)
	{
		return;
	}

	for (const LearnEntry& e : master->learnset)
	{
		if (e.level != mon.level)
		{
			continue;
		}
		const MoveData* move = data_->GetMove(e.moveId);
		if (move == nullptr)
		{
			continue;
		}

		// すでに覚えている／覚える予定の技は飛ばす
		bool known = std::find(learnQueue_.begin(), learnQueue_.end(), e.moveId) != learnQueue_.end();
		for (const MoveSlot& slot : mon.moves)
		{
			if (slot.moveId == e.moveId)
			{
				known = true;
			}
		}
		if (known)
		{
			continue;
		}

		MoveSlot* freeSlot = nullptr;
		for (MoveSlot& slot : mon.moves)
		{
			if (slot.moveId == MonsterParty::MOVE_NONE)
			{
				freeSlot = &slot;
				break;
			}
		}

		if (freeSlot != nullptr)
		{
			freeSlot->moveId = e.moveId;
			freeSlot->currentPp = move->pp;
			PushSay(mon.nickname + "は " + move->name + "を おぼえた！");
		}
		else
		{
			learnQueue_.push_back(e.moveId);
		}
	}
}

void BattleScene::ApplyEvolution(MonsterInstance& mon, int toId)
{
	const MonsterMasterData* oldMaster = data_->GetMonster(mon.monsterId);
	const MonsterMasterData* newMaster = data_->GetMonster(toId);
	if (oldMaster == nullptr || newMaster == nullptr)
	{
		return;
	}

	const int oldMaxHp = MonsterParty::CalcMaxHp(*oldMaster, mon.level);
	const int newMaxHp = MonsterParty::CalcMaxHp(*newMaster, mon.level);

	// ニックネームを付けていなければ、新しい種族名に変わる
	if (mon.nickname == oldMaster->name)
	{
		mon.nickname = newMaster->name;
	}
	mon.monsterId = toId;

	// 最大HPが増えた分だけ現在HPも増える（戦えるまま進化するので最低1は残す）
	mon.currentHp = std::max(1, std::min(mon.currentHp + (newMaxHp - oldMaxHp), newMaxHp));

	// 成長率が変わっても、今のレベルの最低経験値を下回らないようにする
	mon.exp = std::max(mon.exp, MonsterGrowth::GetExpForLevel(newMaster->growthRate, mon.level));

	// 戦闘中の表示も進化後に合わせる
	for (auto& c : side_)
	{
		if (c.mon == &mon)
		{
			c.master = newMaster;
			c.image = LoadMonsterImage(newMaster->backPath);	// 戦闘中の味方は背面画像
		}
	}
}

void BattleScene::SwitchPlayer(int partyIndex)
{
	activeIndex_ = partyIndex;
	SetCombatant(PLAYER, party_->Get(partyIndex));
	Say("ゆけっ！ " + side_[PLAYER].mon->nickname + "！");
	StartSendOut(PLAYER);
}

void BattleScene::SetCombatant(int side, MonsterInstance* mon)
{
	Combatant& c = side_[side];
	c = Combatant{};
	c.mon = mon;
	c.master = data_->GetMonster(mon->monsterId);
	c.dispHp = static_cast<float>(mon->currentHp);

	// 敵は正面、味方は背面の画像を使う
	if (c.master != nullptr)
	{
		c.image = LoadMonsterImage(side == ENEMY ? c.master->frontPath : c.master->backPath);
	}
}

// シートを1コマずつに分割して読み込む。同じIDはキャッシュを返す。失敗したらnullptr
const std::vector<int>* BattleScene::LoadAnimFrames(const MoveAnimData& anim)
{
	auto it = animFrames_.find(anim.id);
	if (it != animFrames_.end())
	{
		return it->second.empty() ? nullptr : &it->second;
	}

	std::vector<int> handles(anim.frameCount, -1);
	const int rows = (anim.frameCount + anim.columns - 1) / anim.columns;
	if (LoadDivGraph(anim.imagePath.c_str(), anim.frameCount, anim.columns, rows,
		anim.frameW, anim.frameH, handles.data()) == -1)
	{
		handles.clear();	// 読み込み失敗：エフェクトなしで進める
	}

	std::vector<int>& slot = animFrames_[anim.id];
	slot = std::move(handles);
	return slot.empty() ? nullptr : &slot;
}

void BattleScene::StartMoveAnim(const MoveData& move, int atk)
{
	moveAnim_ = {};

	// 相手が使うときは、相手用のエフェクトがあればそちらを使う（なければ自分用で代用）
	const int animId = (atk == ENEMY && move.enemyAnimId > 0) ? move.enemyAnimId : move.animId;
	const MoveAnimData* anim = (animId > 0) ? data_->GetMoveAnim(animId) : nullptr;
	if (anim == nullptr && move.animId > 0)
	{
		anim = data_->GetMoveAnim(move.animId);
	}
	const std::vector<int>* frames = (anim != nullptr) ? LoadAnimFrames(*anim) : nullptr;
	if (frames == nullptr)
	{
		anim = nullptr;	// 画像が読めないときは、画像エフェクトなしで進める
	}

	// 前に出るのは攻撃技だけ（変化技は出ない）
	const bool lunge = (move.power > 0);
	if (!lunge && anim == nullptr)
	{
		return;	// 何も演出しない
	}

	moveAnim_.active = true;
	moveAnim_.stage = MoveAnimStage::LUNGE;
	moveAnim_.anim = anim;
	moveAnim_.frames = frames;
	moveAnim_.attacker = atk;
	moveAnim_.side = (anim != nullptr && anim->pos == AnimPos::USER) ? atk : 1 - atk;
	moveAnim_.lunge = lunge;
	moveAnim_.hit = false;
	moveAnim_.timer = 0;
}

int BattleScene::MoveAnimStageFrames(void) const
{
	switch (moveAnim_.stage)
	{
	case MoveAnimStage::LUNGE:
		return moveAnim_.lunge ? MOVE_LUNGE_FRAMES : 0;
	case MoveAnimStage::EFFECT:
		if (moveAnim_.anim == nullptr)
		{
			return 0;
		}
		return moveAnim_.anim->frameCount * moveAnim_.anim->frameTime * moveAnim_.anim->loops;
	case MoveAnimStage::HIT:
		return moveAnim_.hit ? MOVE_HIT_FRAMES : 0;
	case MoveAnimStage::RETURN:
		return moveAnim_.lunge ? MOVE_LUNGE_FRAMES : 0;
	}
	return 0;
}

void BattleScene::UpdateMoveAnim(void)
{
	if (!moveAnim_.active)
	{
		return;
	}

	moveAnim_.timer++;

	// 今のステージが終わったら次へ（長さ0のステージは飛ばす）
	while (moveAnim_.timer >= MoveAnimStageFrames())
	{
		moveAnim_.timer = 0;
		switch (moveAnim_.stage)
		{
		case MoveAnimStage::LUNGE:
			moveAnim_.stage = MoveAnimStage::EFFECT;
			break;
		case MoveAnimStage::EFFECT:
			moveAnim_.stage = MoveAnimStage::HIT;
			break;
		case MoveAnimStage::HIT:
			moveAnim_.stage = MoveAnimStage::RETURN;
			break;
		case MoveAnimStage::RETURN:
			moveAnim_ = {};	// 演出おわり
			return;
		}
	}
}

// モンスター画像のずれと、赤い点滅（技の演出中だけ値が入る）
void BattleScene::GetMonsterAnimOffset(int side, int& ox, int& oy, bool& red) const
{
	ox = 0;
	oy = 0;
	red = false;
	if (!moveAnim_.active)
	{
		return;
	}

	// 技を使った側：前に出て、演出が終わったら戻る（味方は右上へ、敵は左下へ）
	if (side == moveAnim_.attacker && moveAnim_.lunge)
	{
		float rate = 1.0f;
		if (moveAnim_.stage == MoveAnimStage::LUNGE)
		{
			const float t = static_cast<float>(moveAnim_.timer) / MOVE_LUNGE_FRAMES;
			rate = 1.0f - (1.0f - t) * (1.0f - t);	// 出だしが速く、最後にゆっくり止まる
		}
		else if (moveAnim_.stage == MoveAnimStage::RETURN)
		{
			rate = 1.0f - static_cast<float>(moveAnim_.timer) / MOVE_LUNGE_FRAMES;
		}
		const int dir = (side == PLAYER) ? 1 : -1;
		ox = static_cast<int>(SX(30) * dir * rate);
		oy = static_cast<int>(-SY(20) * dir * rate);
	}

	// 技を受けた側：赤く点滅して、左右に揺れる（だんだん小さくなる）
	if (side != moveAnim_.attacker && moveAnim_.stage == MoveAnimStage::HIT && moveAnim_.hit)
	{
		const int t = moveAnim_.timer;
		const float decay = 1.0f - static_cast<float>(t) / MOVE_HIT_FRAMES;
		ox += static_cast<int>(std::sin(t * 1.3f) * SX(10) * decay);
		red = ((t / 4) % 2 == 0);
	}
}

void BattleScene::DrawMoveAnim(void) const
{
	// 画像エフェクトを描くのは、エフェクトのステージの間だけ
	if (!moveAnim_.active || moveAnim_.stage != MoveAnimStage::EFFECT || moveAnim_.anim == nullptr)
	{
		return;
	}
	const MoveAnimData& a = *moveAnim_.anim;
	const int index = (moveAnim_.timer / a.frameTime) % static_cast<int>(moveAnim_.frames->size());

	// 表示位置（モンスター画像の中心。Drawの配置を変えたらここも合わせる）
	int x = SX(320);
	int y = SY(180);
	if (a.pos != AnimPos::CENTER)
	{
		if (moveAnim_.side == ENEMY)
		{
			x = SX(480);
			y = SY(120);
		}
		else
		{
			x = SX(170);
			y = SY(250);
		}
	}

	const double scale = a.scalePercent / 100.0 * screenH_ / BASE_H;
	if (a.additive)
	{
		SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
	}
	DrawRotaGraph(x, y, scale, 0.0, (*moveAnim_.frames)[index], TRUE);
	if (a.additive)
	{
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}
}

int BattleScene::LoadMonsterImage(const std::string& path)
{
	if (path.empty())
	{
		return -1;
	}

	auto it = images_.find(path);
	if (it != images_.end())
	{
		return it->second;
	}

	int handle = LoadGraph(path.c_str());	// 失敗すると-1（その場合は描画をスキップする）
	images_[path] = handle;
	return handle;
}

// Trainer.csv の画像指定から、立ち絵に使う1枚（下向き・立ち）を読み込む
//   フォルダ指定（末尾が / ）… Idle.png
//   ken.png@3x4 … 2行目（下向き）の先頭コマ / ken.png@4 … 左から2番目（下向き）
//   上|下|左|右 … 「下」の画像（@N なら先頭コマ） / ken.png … その1枚
int BattleScene::LoadTrainerPortrait(const std::string& spec)
{
	if (spec.empty())
	{
		return -1;
	}

	const std::string key = "trainer:" + spec;
	auto it = images_.find(key);
	if (it != images_.end())
	{
		return it->second;
	}

	int result = -1;

	// フォルダ指定
	if (spec.back() == '/' || spec.back() == '\\')
	{
		result = LoadGraph((spec + "Idle.png").c_str());
		images_[key] = result;
		return result;
	}

	// 向きごと指定のときは「下」（2番目）だけを使う
	std::string one = spec;
	const size_t bar = spec.find('|');
	if (bar != std::string::npos)
	{
		const size_t next = spec.find('|', bar + 1);
		one = spec.substr(bar + 1, (next == std::string::npos) ? std::string::npos : next - bar - 1);
		if (one.empty())
		{
			images_[key] = -1;
			return -1;
		}
	}

	// 末尾の @3x4 / @4 / @3 を読む
	std::string path = one;
	int cols = 1;
	int rows = 0;
	const size_t at = one.rfind('@');
	if (at != std::string::npos)
	{
		path = one.substr(0, at);
		const std::string suffix = one.substr(at + 1);
		const size_t x = suffix.find('x');
		cols = std::max(1, std::atoi(suffix.substr(0, x).c_str()));
		rows = (x == std::string::npos) ? 0 : std::atoi(suffix.substr(x + 1).c_str());
	}

	if (cols == 1 && rows == 0)
	{
		result = LoadGraph(path.c_str());	// 1枚だけ
	}
	else
	{
		// 分割して、使う1コマだけ残す（ほかのハンドルは消す）
		const int rowCount = (rows > 0) ? rows : 1;
		const int whole = LoadGraph(path.c_str());
		if (whole != -1)
		{
			int w = 0, h = 0;
			GetGraphSize(whole, &w, &h);
			DeleteGraph(whole);

			std::vector<int> handles(static_cast<size_t>(cols) * rowCount, -1);
			if (w >= cols && h >= rowCount
				&& LoadDivGraph(path.c_str(), cols * rowCount, cols, rowCount, w / cols, h / rowCount, handles.data()) != -1)
			{
				// 3x4 … 下向きは2行目の先頭。@4 … 左から2番目。
				const size_t use = (rows == 4) ? static_cast<size_t>(1) * cols : (cols == 4 ? 1 : 0);
				result = handles[use];
				for (size_t i = 0; i < handles.size(); i++)
				{
					if (i != use && handles[i] != -1)
					{
						DeleteGraph(handles[i]);
					}
				}
			}
		}
	}

	images_[key] = result;
	return result;
}

// トレーナーの立ち絵を、相手モンスターの位置に描く（ドット絵なのでにじませず拡大する）
void BattleScene::DrawTrainerPortrait(void) const
{
	if (!showTrainer_ || trainerImg_ == -1)
	{
		return;
	}

	int cx = 0, bottom = 0, size = 0;
	GetMonsterLayout(ENEMY, cx, bottom, size);

	const int prevMode = GetDrawMode();	// 元の設定に戻せるように覚えておく
	SetDrawMode(DX_DRAWMODE_NEAREST);
	DrawImageFit(trainerImg_, cx, bottom, size);
	SetDrawMode(prevMode);
}

// =============================================================
// 入力
// =============================================================

void BattleScene::MoveCursor(int count)
{
	if (count <= 0)
	{
		return;
	}
	auto* input = InputManager::GetInstance();
	if (IsUp(input))
	{
		cursor_ = (cursor_ + count - 1) % count;
	}
	if (IsDown(input))
	{
		cursor_ = (cursor_ + 1) % count;
	}
}

bool BattleScene::HasUsableMove(void) const
{
	for (const auto& m : side_[PLAYER].mon->moves)
	{
		if (m.moveId != MonsterParty::MOVE_NONE && m.currentPp > 0)
		{
			return true;
		}
	}
	return false;
}

void BattleScene::UpdateCommand(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(4);

	if (!IsConfirm(input))
	{
		return;
	}

	switch (cursor_)
	{
	case 0:	// たたかう
		if (HasUsableMove())
		{
			phase_ = Phase::MOVE_SELECT;
			cursor_ = 0;
		}
		else
		{
			BeginTurn(ActionType::MOVE, -1);	// わるあがき
		}
		break;
	case 1:	// いれかえ
		phase_ = Phase::PARTY_SELECT;
		cursor_ = activeIndex_;
		break;
	case 2:	// バッグ
		RefreshBagList();
		phase_ = Phase::BAG;
		cursor_ = 0;
		break;
	case 3:	// にげる
		if (trainer_ != nullptr)
		{
			// トレーナー戦では逃げられない（ターンは使わない）
			steps_.clear();
			insertIdx_ = 0;
			PushSay("トレーナーとの しょうぶからは にげられない！");
			StartMessages();
		}
		else
		{
			BeginTurn(ActionType::RUN, 0);
		}
		break;
	}
}

void BattleScene::UpdateMoveSelect(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(4);

	if (IsCancel(input))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 0;
		return;
	}
	if (IsConfirm(input))
	{
		const MoveSlot& m = side_[PLAYER].mon->moves[cursor_];
		if (m.moveId != MonsterParty::MOVE_NONE && m.currentPp > 0)
		{
			BeginTurn(ActionType::MOVE, cursor_);
		}
	}
}

void BattleScene::UpdatePartySelect(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(party_->GetCount());

	// ひんしで強制的に選ぶとき以外は戻れる
	if (!needSwitch_ && IsCancel(input))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 1;
		return;
	}
	if (!IsConfirm(input))
	{
		return;
	}

	const MonsterInstance* m = party_->Get(cursor_);
	if (m == nullptr || m->currentHp <= 0 || cursor_ == activeIndex_)
	{
		return;	// 出せない
	}

	const int idx = cursor_;
	if (needSwitch_)
	{
		// ひんし後の入れ替え：相手は行動しない
		needSwitch_ = false;
		steps_.clear();
		insertIdx_ = 0;
		Push([this, idx]() { SwitchPlayer(idx); });
		StartMessages();
	}
	else
	{
		BeginTurn(ActionType::SWITCH, idx);
	}
}

void BattleScene::UpdateLearnSelect(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(5);	// 0〜3：忘れる技、4：あきらめる

	// 戻る／Escなら「あきらめる」にカーソルを移す（確定はしない）
	if (IsCancel(input))
	{
		cursor_ = 4;
		return;
	}

	if (!IsConfirm(input))
	{
		return;
	}

	MonsterInstance* p = side_[PLAYER].mon;
	const int newMoveId = learnQueue_.front();
	learnQueue_.pop_front();
	const MoveData* newMove = data_->GetMove(newMoveId);
	const std::string newName = (newMove != nullptr) ? newMove->name : "？";

	if (cursor_ < 4)
	{
		const MoveData* oldMove = data_->GetMove(p->moves[cursor_].moveId);
		const std::string oldName = (oldMove != nullptr) ? oldMove->name : "？";
		if (MonsterGrowth::ReplaceMove(*data_, *p, cursor_, newMoveId))
		{
			Say(p->nickname + "は " + oldName + "を わすれて " + newName + "を おぼえた！");
		}
		else
		{
			Say(p->nickname + "は " + newName + "を おぼえなかった。");
		}
	}
	else
	{
		Say(p->nickname + "は " + newName + "を おぼえるのを あきらめた。");
	}

	// メッセージ後、まだ覚えたい技があればFinishでまた選択に戻る
	steps_.clear();
	phase_ = Phase::MESSAGE;
}

void BattleScene::UpdateBag(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(static_cast<int>(bagList_.size()));

	if (IsCancel(input))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 2;
		return;
	}
	if (!IsConfirm(input) || bagList_.empty())
	{
		return;
	}

	const int itemId = bagList_[cursor_];
	const ItemMasterData* item = items_->GetItem(itemId);
	if (item == nullptr)
	{
		return;
	}

	// 使えない場合はターンを使わず、メッセージだけ出してコマンドへ戻る
	auto refuse = [this](const std::string& reason)
		{
			steps_.clear();
			insertIdx_ = 0;
			PushSay(reason);
			StartMessages();
		};

	if (item->effect == ItemEffect::CATCH)
	{
		std::string reason;
		if (!CanCatch(reason))
		{
			refuse(reason);
			return;
		}
		BeginTurn(ActionType::ITEM, itemId);
	}
	else if (ItemUse::NeedsTarget(*item))
	{
		usingItemId_ = itemId;
		phase_ = Phase::ITEM_TARGET;
		cursor_ = activeIndex_;
	}
	else if (item->effect == ItemEffect::ATK_UP || item->effect == ItemEffect::DEF_UP)
	{
		BeginTurn(ActionType::ITEM, itemId);
	}
	else
	{
		refuse("いまは つかえない！");
	}
}

void BattleScene::UpdateItemTarget(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(party_->GetCount());

	if (IsCancel(input))
	{
		// バッグに戻る（さっき選んでいたアイテムにカーソルを合わせる）
		phase_ = Phase::BAG;
		auto it = std::find(bagList_.begin(), bagList_.end(), usingItemId_);
		cursor_ = (it != bagList_.end()) ? static_cast<int>(it - bagList_.begin()) : 0;
		return;
	}
	if (!IsConfirm(input))
	{
		return;
	}

	const MonsterInstance* m = party_->Get(cursor_);
	const ItemMasterData* item = items_->GetItem(usingItemId_);
	if (m == nullptr || item == nullptr)
	{
		return;
	}

	std::string reason;
	if (!ItemUse::CanUseOnMonster(*data_, *item, *m, reason))
	{
		// 使っても意味がない：アイテムは減らさず、ターンも使わない
		steps_.clear();
		insertIdx_ = 0;
		PushSay(reason);
		StartMessages();
		return;
	}

	itemTarget_ = cursor_;
	BeginTurn(ActionType::ITEM, usingItemId_);
}

void BattleScene::UpdateEvolve(void)
{
	auto* input = InputManager::GetInstance();
	MonsterInstance* mon = evolveCur_.mon;

	// BACKでキャンセル
	if (IsCancel(input))
	{
		CancelEvolve();
		return;
	}

	evolveTimer_++;

	// 進化前と進化後の画像を交互に出す（だんだん速くなる）
	if (--evolveFlip_ <= 0)
	{
		evolveShowNew_ = !evolveShowNew_;
		evolveFlip_ = std::max(3, 22 - evolveTimer_ / 10);
	}

	if (evolveTimer_ >= EVOLVE_FRAMES)
	{
		const std::string oldNick = mon->nickname;
		const std::string newName = data_->GetMonster(evolveCur_.toId)->name;
		ApplyEvolution(*mon, evolveCur_.toId);

		evolved_ = true;
		evolveState_ = EvolveState::RESULT;
		steps_.clear();
		insertIdx_ = 0;
		PushSay("おめでとう！ " + oldNick + "は " + newName + "に しんかした！");
		LearnEvolutionMoves(*mon);	// 進化後の種族の技（結果メッセージのあとに続く）
		StartMessages();
	}
}

void BattleScene::UpdateEvolveSelect(void)
{
	auto* input = InputManager::GetInstance();
	const int count = static_cast<int>(evolveCur_.candidates.size());
	MoveCursor(count);

	if (IsCancel(input))
	{
		CancelEvolve();
		return;
	}
	if (IsConfirm(input))
	{
		evolveCur_.toId = evolveCur_.candidates[cursor_];
		evolveNewImg_ = evolveCandImgs_[cursor_];
		BeginEvolveAnim();
	}
}

void BattleScene::UpdateHpBars(void)
{
	// 前に出ている間とエフェクトの再生中は、HPバーを動かさない（食らう演出から減らす）
	if (moveAnim_.active
		&& (moveAnim_.stage == MoveAnimStage::LUNGE || moveAnim_.stage == MoveAnimStage::EFFECT))
	{
		return;
	}

	for (auto& c : side_)
	{
		if (c.mon == nullptr)
		{
			continue;
		}
		const float target = static_cast<float>(std::max(0, c.mon->currentHp));
		const float diff = target - c.dispHp;
		if (std::abs(diff) <= 0.5f)
		{
			c.dispHp = target;
		}
		else
		{
			c.dispHp += diff * 0.12f + (diff > 0 ? 0.3f : -0.3f);
		}
	}
}

// =============================================================
// 描画
// =============================================================

void BattleScene::DrawHpBar(int x, int y, int w, int h, float ratio) const
{
	ratio = std::max(0.0f, std::min(ratio, 1.0f));
	DrawBox(x, y, x + w, y + h, GetColor(90, 90, 90), TRUE);

	unsigned int color = GetColor(60, 200, 80);
	if (ratio <= 0.2f)
	{
		color = GetColor(220, 60, 50);
	}
	else if (ratio <= 0.5f)
	{
		color = GetColor(230, 200, 40);
	}
	DrawBox(x, y, x + static_cast<int>(w * ratio), y + h, color, TRUE);
}

void BattleScene::DrawMonsterImage(int side, int cx, int bottomY, int size) const
{
	// 技の演出によるずれ（前に出る・揺れる）と、赤い点滅
	int ox = 0;
	int oy = 0;
	bool red = false;
	GetMonsterAnimOffset(side, ox, oy, red);

	if (red)
	{
		SetDrawBright(255, 70, 70);
	}
	DrawImageFit(side_[side].image, cx + ox, bottomY + oy, size);
	if (red)
	{
		// 赤を重ねて、暗い色のモンスターでも点滅が分かるようにする
		SetDrawBlendMode(DX_BLENDMODE_ADD, 110);
		SetDrawBright(255, 0, 0);
		DrawImageFit(side_[side].image, cx + ox, bottomY + oy, size);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetDrawBright(255, 255, 255);
	}
}

// 画像の下端中央を(cx, bottomY)に合わせ、縦横の長い方がsizeに収まるよう縮尺して描く
void BattleScene::DrawImageFit(int img, int cx, int bottomY, int size) const
{
	if (img == -1)
	{
		return;	// 画像なし（パス未記入・読み込み失敗）
	}

	int w = 0, h = 0;
	GetGraphSize(img, &w, &h);
	if (w <= 0 || h <= 0)
	{
		return;
	}

	double scale = static_cast<double>(size) / std::max(w, h);
	int cy = bottomY - static_cast<int>(h * scale / 2);
	DrawRotaGraph(cx, cy, scale, 0.0, img, TRUE);
}

void BattleScene::DrawStatusBox(int side, int x, int y) const
{
	const Combatant& c = side_[side];
	if (c.mon == nullptr || c.master == nullptr)
	{
		return;
	}

	const unsigned int black = GetColor(0, 0, 0);
	const int maxHp = MonsterParty::CalcMaxHp(*c.master, c.mon->level);
	const int boxW = SX(240);
	const int boxH = SY((side == PLAYER) ? 84 : 56);
	const int barW = SX(220);

	DrawBox(x, y, x + boxW, y + boxH, GetColor(255, 255, 255), TRUE);
	DrawBox(x, y, x + boxW, y + boxH, GetColor(60, 60, 60), FALSE);

	DrawFormatString(x + SX(10), y + SY(6), black, "%s  Lv%d", c.mon->nickname.c_str(), c.mon->level);
	const char* ailment = AilmentText(c.mon->ailment);
	if (ailment[0] != '\0')
	{
		DrawFormatString(x + SX(180), y + SY(6), GetColor(160, 0, 120), "%s", ailment);
	}

	DrawHpBar(x + SX(10), y + SY(28), barW, SY(10), c.dispHp / static_cast<float>(std::max(1, maxHp)));

	if (side == PLAYER)
	{
		const int dispHp = std::min(maxHp, std::max(0, static_cast<int>(c.dispHp + 0.5f)));
		DrawFormatString(x + SX(10), y + SY(44), black, "HP %d / %d", dispHp, maxHp);
		// 経験値バー
		const float expRatio = MonsterGrowth::GetExpRatio(*data_, *c.mon);
		DrawBox(x + SX(10), y + SY(70), x + SX(10) + barW, y + SY(76), GetColor(90, 90, 90), TRUE);
		DrawBox(x + SX(10), y + SY(70), x + SX(10) + static_cast<int>(barW * expRatio), y + SY(76), GetColor(70, 130, 230), TRUE);
	}
}

void BattleScene::DrawMessageBox(const std::string& text) const
{
	DrawBox(0, SY(360), screenW_, screenH_, GetColor(255, 255, 255), TRUE);
	DrawBox(SX(4), SY(364), screenW_ - SX(4), screenH_ - SY(4), GetColor(60, 60, 60), FALSE);
	DrawFormatString(SX(24), SY(384), GetColor(0, 0, 0), "%s", text.c_str());

	if ((phase_ == Phase::MESSAGE || phase_ == Phase::END)
		&& !moveAnim_.active && !sendOut_.active && !catch_.active)
	{
		DrawString(SX(600), SY(450), "▼", GetColor(0, 0, 0));
	}
}

void BattleScene::DrawCommandMenu(void) const
{
	static const char* const items[4] = { "たたかう", "いれかえ", "バッグ", "にげる" };

	DrawBox(SX(400), SY(360), screenW_, screenH_, GetColor(255, 255, 255), TRUE);
	DrawBox(SX(404), SY(364), screenW_ - SX(4), screenH_ - SY(4), GetColor(60, 60, 60), FALSE);
	for (int i = 0; i < 4; i++)
	{
		DrawFormatString(SX(450), SY(372 + i * 26), GetColor(0, 0, 0), "%s", items[i]);
	}
	DrawString(SX(425), SY(372 + cursor_ * 26), "▶", GetColor(0, 0, 0));
}

void BattleScene::DrawMoveMenu(void) const
{
	const unsigned int black = GetColor(0, 0, 0);
	const MonsterInstance* p = side_[PLAYER].mon;

	DrawBox(0, SY(360), screenW_, screenH_, GetColor(255, 255, 255), TRUE);
	DrawBox(SX(4), SY(364), screenW_ - SX(4), screenH_ - SY(4), GetColor(60, 60, 60), FALSE);

	for (int i = 0; i < 4; i++)
	{
		const MoveSlot& slot = p->moves[i];
		const int x = SX(50 + (i % 2) * 220);
		const int y = SY(380 + (i / 2) * 36);
		const MoveData* move = (slot.moveId != MonsterParty::MOVE_NONE) ? data_->GetMove(slot.moveId) : nullptr;

		if (move == nullptr)
		{
			DrawString(x, y, "－", GetColor(150, 150, 150));
			continue;
		}
		const unsigned int color = (slot.currentPp > 0) ? black : GetColor(170, 170, 170);
		DrawFormatString(x, y, color, "%s", move->name.c_str());
	}

	// カーソル（2列×2行の配置）
	DrawString(SX(25 + (cursor_ % 2) * 220), SY(380 + (cursor_ / 2) * 36), "▶", black);

	// 選択中の技の詳細
	const MoveSlot& sel = p->moves[cursor_];
	const MoveData* selMove = (sel.moveId != MonsterParty::MOVE_NONE) ? data_->GetMove(sel.moveId) : nullptr;
	if (selMove != nullptr)
	{
		DrawFormatString(SX(480), SY(380), black, "PP %d / %d", sel.currentPp, selMove->pp);
		DrawFormatString(SX(480), SY(410), black, "威力 %d", selMove->power);
		DrawFormatString(SX(480), SY(436), black, "命中 %d", selMove->accuracy);
	}
}

void BattleScene::DrawPartyMenu(void) const
{
	const unsigned int black = GetColor(0, 0, 0);

	DrawBox(SX(60), SY(30), SX(580), SY(340), GetColor(245, 245, 255), TRUE);
	DrawBox(SX(60), SY(30), SX(580), SY(340), GetColor(60, 60, 60), FALSE);

	for (int i = 0; i < party_->GetCount(); i++)
	{
		const MonsterInstance* m = party_->Get(i);
		const int y = SY(48 + i * 48);
		const int maxHp = MonsterParty::GetMaxHp(*data_, *m);

		unsigned int color = black;
		if (m->currentHp <= 0)
		{
			color = GetColor(200, 60, 60);
		}
		else if (i == activeIndex_)
		{
			color = GetColor(110, 110, 110);
		}
		DrawFormatString(SX(110), y, color, "%s  Lv%d", m->nickname.c_str(), m->level);
		DrawFormatString(SX(400), y, color, "HP %d / %d", std::max(0, m->currentHp), maxHp);
		DrawHpBar(SX(110), y + SY(22), SX(200), SY(8), static_cast<float>(m->currentHp) / static_cast<float>(std::max(1, maxHp)));
		if (i == activeIndex_)
		{
			DrawString(SX(330), y, "(戦闘中)", color);
		}
	}
	DrawString(SX(80), SY(48 + cursor_ * 48), "▶", black);
}

void BattleScene::DrawLearnMenu(void) const
{
	const unsigned int black = GetColor(0, 0, 0);
	const MonsterInstance* p = side_[PLAYER].mon;
	const MoveData* newMove = data_->GetMove(learnQueue_.front());

	DrawMessageBox(p->nickname + "は " + (newMove != nullptr ? newMove->name : "？")
		+ "を おぼえたい！ わすれる技を えらんでください。");

	DrawBox(SX(380), SY(190), SX(620), SY(350), GetColor(255, 255, 255), TRUE);
	DrawBox(SX(380), SY(190), SX(620), SY(350), GetColor(60, 60, 60), FALSE);
	for (int i = 0; i < 4; i++)
	{
		const MoveData* m = data_->GetMove(p->moves[i].moveId);
		DrawFormatString(SX(430), SY(202 + i * 26), black, "%s", m != nullptr ? m->name.c_str() : "－");
	}
	DrawString(SX(430), SY(202 + 4 * 26), "あきらめる", black);
	DrawString(SX(405), SY(202 + cursor_ * 26), ">", black);
}

void BattleScene::DrawBagMenu(void) const
{
	const unsigned int black = GetColor(0, 0, 0);
	const int VISIBLE = 8;	// 一度に表示する行数
	const int n = static_cast<int>(bagList_.size());

	DrawBox(SX(280), SY(20), SX(620), SY(340), GetColor(245, 245, 255), TRUE);
	DrawBox(SX(280), SY(20), SX(620), SY(340), GetColor(60, 60, 60), FALSE);

	if (n == 0)
	{
		DrawString(SX(310), SY(40), "つかえる どうぐが ない", black);
		DrawMessageBox("BACKキーで もどる");
		return;
	}

	// カーソルが見える範囲にスクロール
	const int first = std::max(0, std::min(cursor_ - 3, n - VISIBLE));
	for (int i = 0; i < VISIBLE && first + i < n; i++)
	{
		const int id = bagList_[first + i];
		const ItemMasterData* item = items_->GetItem(id);
		if (item == nullptr)
		{
			continue;
		}
		const int y = SY(36 + i * 34);
		DrawFormatString(SX(330), y, black, "%s", item->name.c_str());
		DrawFormatString(SX(540), y, black, "x%2d", inventory_->GetCount(id));
	}
	DrawString(SX(300), SY(36 + (cursor_ - first) * 34), "▶", black);
	if (first > 0)
	{
		DrawString(SX(595), SY(24), "↑", black);
	}
	if (first + VISIBLE < n)
	{
		DrawString(SX(595), SY(318), "↓", black);
	}

	// 選択中のアイテムの説明
	const ItemMasterData* sel = items_->GetItem(bagList_[cursor_]);
	DrawMessageBox((sel != nullptr) ? sel->description : "");
}

void BattleScene::DrawEvolve(void) const
{
	const unsigned int black = GetColor(0, 0, 0);

	// 進化先の選択画面：左に候補のプレビュー、右に候補の一覧
	if (evolveState_ == EvolveState::SELECT)
	{
		const int count = static_cast<int>(evolveCur_.candidates.size());
		DrawImageFit(evolveCandImgs_[cursor_], SX(190), SY(320), SY(240));

		const int boxH = SY(30 + count * 30);
		DrawBox(SX(380), SY(60), SX(620), SY(60) + boxH, GetColor(255, 255, 255), TRUE);
		DrawBox(SX(380), SY(60), SX(620), SY(60) + boxH, GetColor(60, 60, 60), FALSE);
		for (int i = 0; i < count; i++)
		{
			const MonsterMasterData* m = data_->GetMonster(evolveCur_.candidates[i]);
			DrawFormatString(SX(430), SY(75 + i * 30), black, "%s", (m != nullptr) ? m->name.c_str() : "？");
		}
		DrawString(SX(405), SY(75 + cursor_ * 30), "▶", black);

		DrawMessageBox("どの すがたに しんかする？");
		DrawString(SX(24), SY(430), "BACKキー：しんかを やめる", GetColor(120, 120, 120));
		return;
	}

	const int cx = SX(320);
	const int bottom = SY(320);
	const int size = SY(240);

	int img = -1;
	if (evolveState_ == EvolveState::ANIM)
	{
		img = evolveShowNew_ ? evolveNewImg_ : evolveOldImg_;
	}
	else
	{
		img = evolved_ ? evolveNewImg_ : evolveOldImg_;
	}
	DrawImageFit(img, cx, bottom, size);

	// 演出中は、時間がたつほど白く光らせる
	if (evolveState_ == EvolveState::ANIM)
	{
		const int glow = 40 + 160 * evolveTimer_ / EVOLVE_FRAMES;
		SetDrawBlendMode(DX_BLENDMODE_ADD, glow);
		DrawImageFit(img, cx, bottom, size);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	}

	DrawMessageBox(msg_);
	if (evolveState_ == EvolveState::ANIM)
	{
		DrawString(SX(24), SY(430), "BACKキー：しんかを やめる", GetColor(120, 120, 120));
	}
}

// =============================================================
// 補助
// =============================================================

std::string BattleScene::Name(int side) const
{
	const std::string& n = side_[side].mon->nickname;
	if (side != ENEMY)
	{
		return n;
	}
	return (trainer_ != nullptr) ? "あいての " + n : "野生の " + n;
}

int BattleScene::Rand(int lo, int hi)
{
	return std::uniform_int_distribution<int>(lo, hi)(rng_);
}

// 相手のモンスターを作る
bool BattleScene::CreateEnemy(void)
{
	enemyParty_.clear();
	enemyIndex_ = 0;

	// 野生：1匹だけ
	if (trainer_ == nullptr)
	{
		return MonsterParty::Create(*data_, wildId_, wildLevel_, enemy_);
	}

	// トレーナー：手持ち全部を作る（作れないものは飛ばす）
	for (const auto& e : trainer_->party)
	{
		MonsterInstance m{};
		if (MonsterParty::Create(*data_, e.monsterId, e.level, m))
		{
			enemyParty_.push_back(m);
		}
	}
	if (enemyParty_.empty())
	{
		return false;
	}
	enemy_ = enemyParty_[0];
	return true;
}

bool BattleScene::HasNextEnemy(void) const
{
	return enemyIndex_ + 1 < static_cast<int>(enemyParty_.size());
}

void BattleScene::SendNextEnemy(void)
{
	// このターンの残り（出てきたばかりの相手の行動など）は行わない
	steps_.clear();
	insertIdx_ = 0;

	enemyIndex_++;
	enemy_ = enemyParty_[enemyIndex_];
	SetCombatant(ENEMY, &enemy_);

	Say(trainer_->name + "は " + enemy_.nickname + "を くりだした！");
	StartSendOut(ENEMY);
}

void BattleScene::OnTrainerDefeated(void)
{
	// 倒れた相手モンスターと情報枠を消して、代わりにトレーナーの立ち絵を出す
	if (trainerImg_ != -1)
	{
		hidden_[ENEMY] = true;
		showTrainer_ = true;
	}
	Say(trainer_->name + "との しょうぶに かった！");
	if (!trainer_->defeatMessage.empty())
	{
		PushSay(trainer_->name + "「" + trainer_->defeatMessage + "」");
	}
	if (trainer_->prize > 0)
	{
		prize_ = trainer_->prize;
		PushSay("しょうきんとして " + std::to_string(prize_) + "円を てにいれた！");
	}
}

// =============================================================
// モンスターを出す演出（ボールを投げる → バウンド → 開く → 出てくる）
// ボールごとの見た目・エフェクトは BallEffect.csv で変えられる
// =============================================================

// モンスター画像の配置（Draw と DrawSendOut で共通。ここを変えれば両方に反映される）
void BattleScene::GetMonsterLayout(int side, int& cx, int& bottomY, int& size) const
{
	if (side == ENEMY)
	{
		cx = SX(480);
		bottomY = SY(210);
		size = SY(180);
	}
	else
	{
		cx = SX(170);
		bottomY = SY(350);
		size = SY(200);
	}
}

// ボールが開いたときのエフェクトシートを分割して読み込む。画像なし・失敗ならnullptr
const std::vector<int>* BattleScene::LoadBallFrames(const BallEffectMaster& ball)
{
	return LoadSheetCached(ballFrames_, ball.id, ball.effectImage, ball.columns,
		ball.frameCount, ball.frameW, ball.frameH);
}

void BattleScene::StartSendOut(int side)
{
	hidden_[side] = true;	// 出てくるまでは隠す
	sendOut_ = {};

	// CSVにそのボールが無い（ファイルが無い）ときは、仮のボールで進める
	const BallEffectMaster* ball = ballData_.Get(GetBallId(side));
	if (ball == nullptr)
	{
		ball = &defaultBall_;
	}

	sendOut_.active = true;
	sendOut_.stage = SendOutStage::THROW;
	sendOut_.side = side;
	sendOut_.timer = 0;
	sendOut_.ball = ball;
	sendOut_.ballImg = LoadMonsterImage(ball->ballImage);
	sendOut_.openImg = ball->openImage.empty() ? sendOut_.ballImg : LoadMonsterImage(ball->openImage);
	sendOut_.effectFrames = LoadBallFrames(*ball);

	// プレイヤーの投げモーション（最初に使うときに読み込む）
	// 1枚ずつ別画像の場合：読み込めたものだけを順番に並べる
	if (side == PLAYER && playerSprite_.empty() && !playerImagePaths_.empty())
	{
		for (const std::string& path : playerImagePaths_)
		{
			const int handle = LoadGraph(path.c_str());
			if (handle != -1)
			{
				playerSprite_.push_back(handle);
			}
		}
	}
	// スプライトシートの場合
	if (side == PLAYER && playerSprite_.empty() && !playerSpritePath_.empty() && playerSpriteFrames_ > 0)
	{
		std::vector<int> handles(playerSpriteFrames_, -1);
		const int cols = std::max(1, playerSpriteColumns_);
		const int rows = (playerSpriteFrames_ + cols - 1) / cols;
		if (LoadDivGraph(playerSpritePath_.c_str(), playerSpriteFrames_, cols, rows,
			playerSpriteW_, playerSpriteH_, handles.data()) != -1)
		{
			playerSprite_ = std::move(handles);
		}
	}
}

int BattleScene::SendOutStageFrames(void) const
{
	const BallEffectMaster* ball = sendOut_.ball;
	switch (sendOut_.stage)
	{
	case SendOutStage::THROW:
		return SENDOUT_THROW_FRAMES;
	case SendOutStage::BOUNCE:
		return (ball != nullptr) ? std::max(0, ball->bounces) * SENDOUT_BOUNCE_FRAMES : 0;
	case SendOutStage::OPEN:
	{
		int f = SENDOUT_OPEN_MIN_FRAMES;
		if (ball != nullptr && sendOut_.effectFrames != nullptr)
		{
			f = std::max(f, static_cast<int>(sendOut_.effectFrames->size()) * ball->frameTime);
		}
		return f;
	}
	case SendOutStage::EMERGE:
		return SENDOUT_EMERGE_FRAMES;
	}
	return 0;
}

void BattleScene::UpdateSendOut(void)
{
	if (!sendOut_.active)
	{
		return;
	}

	sendOut_.timer++;

	// 今のステージが終わったら次へ（長さ0のステージは飛ばす）
	while (sendOut_.timer >= SendOutStageFrames())
	{
		sendOut_.timer = 0;
		switch (sendOut_.stage)
		{
		case SendOutStage::THROW:
			sendOut_.stage = SendOutStage::BOUNCE;
			break;
		case SendOutStage::BOUNCE:
			sendOut_.stage = SendOutStage::OPEN;
			break;
		case SendOutStage::OPEN:
			sendOut_.stage = SendOutStage::EMERGE;
			break;
		case SendOutStage::EMERGE:
			hidden_[sendOut_.side] = false;	// ここから通常の描画に戻る
			sendOut_ = {};
			return;
		}
	}
}

// ボール1個を描く。画像が無いときは赤い円で代用する
void BattleScene::DrawBallSprite(int x, int y, int size, double angle, int alpha, int img) const
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::max(0, std::min(alpha, 255)));
	if (img != -1)
	{
		int w = 0, h = 0;
		GetGraphSize(img, &w, &h);
		if (w > 0 && h > 0)
		{
			DrawRotaGraph(x, y, static_cast<double>(size) / std::max(w, h), angle, img, TRUE);
		}
	}
	else
	{
		const int r = std::max(2, size / 2);
		DrawCircle(x, y, r, GetColor(220, 50, 50), TRUE);
		// 回転がわかるように、白い丸を円の内側で回す
		const int bx = x + static_cast<int>(std::cos(angle) * r * 0.45);
		const int by = y + static_cast<int>(std::sin(angle) * r * 0.45);
		DrawCircle(bx, by, std::max(2, r / 3), GetColor(255, 255, 255), TRUE);
		DrawCircle(x, y, r, GetColor(40, 40, 40), FALSE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// ボールを投げるプレイヤー：投げモーションのあと、左へ去っていく
void BattleScene::DrawThrower(void) const
{
	if (!sendOut_.active || sendOut_.side != PLAYER || playerSprite_.empty())
	{
		return;
	}

	const int n = static_cast<int>(playerSprite_.size());
	int index = n - 1;
	int offsetX = 0;
	if (sendOut_.stage == SendOutStage::THROW)
	{
		index = std::min(n - 1, sendOut_.timer * n / SENDOUT_THROW_FRAMES);
	}
	else if (sendOut_.stage == SendOutStage::BOUNCE)
	{
		offsetX = -static_cast<int>(SX(200) * std::min(1.0f, sendOut_.timer / 20.0f));
	}
	else
	{
		return;
	}
	DrawImageFit(playerSprite_[index], SX(70) + offsetX, SY(350), SY(190));
}

void BattleScene::DrawSendOut(void) const
{
	if (!sendOut_.active || sendOut_.ball == nullptr)
	{
		return;
	}

	const SendOutPlay& s = sendOut_;
	const BallEffectMaster& ball = *s.ball;

	int cx = 0, bottom = 0, size = 0;
	GetMonsterLayout(s.side, cx, bottom, size);

	const int landX = cx;				// ボールが落ちる場所（モンスターの足元）
	const int landY = bottom - SY(12);
	const int startX = (s.side == PLAYER) ? SX(100) : SX(600);	// 投げ始めの位置
	const int startY = (s.side == PLAYER) ? SY(240) : SY(30);
	const int ballSize = SY(ball.ballSize);
	const int total = std::max(1, SendOutStageFrames());
	const float t = std::min(1.0f, static_cast<float>(s.timer) / total);

	DrawThrower();

	switch (s.stage)
	{
	case SendOutStage::THROW:
	{
		// 山なりに飛んで、くるくる回る
		const int x = startX + static_cast<int>((landX - startX) * t);
		const int y = startY + static_cast<int>((landY - startY) * t - SY(90) * 4.0f * t * (1.0f - t));
		DrawBallSprite(x, y, ballSize, s.timer * 0.5, 255, s.ballImg);
		break;
	}

	case SendOutStage::BOUNCE:
	{
		// 落ちてから、だんだん小さくバウンドする
		const int k = s.timer / SENDOUT_BOUNCE_FRAMES;
		const float u = static_cast<float>(s.timer % SENDOUT_BOUNCE_FRAMES) / SENDOUT_BOUNCE_FRAMES;
		const float height = SY(36) * std::pow(0.55f, static_cast<float>(k));
		const int y = landY - static_cast<int>(height * 4.0f * u * (1.0f - u));
		DrawBallSprite(landX, y, ballSize, SENDOUT_THROW_FRAMES * 0.5 + s.timer * 0.1, 255, s.ballImg);
		break;
	}

	case SendOutStage::OPEN:
	{
		// ボールが開いてエフェクト。エフェクトはモンスターが出る位置の中心に出す
		const int ex = cx;
		const int ey = bottom - size / 2;

		const int ballAlpha = (t < 0.3f) ? 255 : static_cast<int>(255 * (1.0f - (t - 0.3f) / 0.7f));
		DrawBallSprite(landX, landY, ballSize, SENDOUT_THROW_FRAMES * 0.5, ballAlpha, s.openImg);

		if (s.effectFrames != nullptr && !s.effectFrames->empty())
		{
			const int index = std::min(static_cast<int>(s.effectFrames->size()) - 1, s.timer / ball.frameTime);
			const double scale = ball.scalePercent / 100.0 * screenH_ / BASE_H;
			if (ball.additive)
			{
				SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
			}
			DrawRotaGraph(ex, ey, scale, 0.0, (*s.effectFrames)[index], TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
		else
		{
			// エフェクト画像が無いときの簡易フラッシュ（広がりながら消える）
			const int radius = SY(20) + static_cast<int>(size * 0.6f * t);
			SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(220 * (1.0f - t)));
			DrawCircle(ex, ey, radius, GetColor(ball.glowR, ball.glowG, ball.glowB), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
		break;
	}

	case SendOutStage::EMERGE:
	{
		// ボールのあった位置から、光りながら大きくなって出てくる
		const float e = 1.0f - (1.0f - t) * (1.0f - t);
		const int img = side_[s.side].image;
		const int curSize = std::max(4, static_cast<int>(size * e));
		const int curBottom = landY + static_cast<int>((bottom - landY) * e);

		DrawImageFit(img, cx, curBottom, curSize);

		// 出はじめは、ボールの色で光らせる（だんだん元の色に戻る）
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(255 * (1.0f - t)));
		SetDrawBright(ball.glowR, ball.glowG, ball.glowB);
		DrawImageFit(img, cx, curBottom, curSize);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		SetDrawBright(255, 255, 255);
		break;
	}
	}
}

// =============================================================
// 捕まえる演出（投げる → 吸い込む → 落ちる → ゆれる → 成功／失敗）
// ボールごとの見た目は BallEffect.csv で変えられる。
//   ・吸い込みと、失敗して出てくるときは、モンスターを出す演出と同じエフェクト（effectImage）を使う
//   ・捕まえたときの光は catchImage（空なら簡易の星と光の輪）
// =============================================================

// このアイテムで投げるボールの設定を決める
//   BallEffect.csv の id にアイテムIDを書くと、そのボールを投げたときの演出になる。
//   無ければ SetBall で決めたボール、それも無ければ仮のボール。
const BallEffectMaster* BattleScene::GetCatchBall(int itemId) const
{
	const BallEffectMaster* ball = ballData_.Get(itemId);
	if (ball == nullptr)
	{
		ball = ballData_.Get(playerBallId_);
	}
	return (ball != nullptr) ? ball : &defaultBall_;
}

// 捕まえたときの光のシートを読み込む。画像なし・失敗ならnullptr
const std::vector<int>* BattleScene::LoadCatchFrames(const BallEffectMaster& ball)
{
	return LoadSheetCached(catchFrames_, ball.id, ball.catchImage, ball.catchColumns,
		ball.catchFrameCount, ball.catchFrameW, ball.catchFrameH);
}

// ボールが開いたときのエフェクトを描く。画像があればそのシート、無ければ広がるフラッシュ
//   (x, y)＝中心 / timer＝このステージの経過フレーム / t＝進み具合（0〜1） / size＝モンスターの大きさ
void BattleScene::DrawOpenEffect(int x, int y, int timer, float t, int size,
	const std::vector<int>* frames, const BallEffectMaster& ball) const
{
	if (frames != nullptr && !frames->empty())
	{
		const int index = std::min(static_cast<int>(frames->size()) - 1, timer / ball.frameTime);
		const double scale = ball.scalePercent / 100.0 * screenH_ / BASE_H;
		if (ball.additive)
		{
			SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
		}
		DrawRotaGraph(x, y, scale, 0.0, (*frames)[index], TRUE);
		SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		return;
	}

	// エフェクト画像が無いときの簡易フラッシュ（広がりながら消える）
	const int radius = SY(20) + static_cast<int>(size * 0.6f * t);
	SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(220 * (1.0f - t)));
	DrawCircle(x, y, radius, GetColor(ball.glowR, ball.glowG, ball.glowB), TRUE);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

// ボールを投げて捕まえる演出を始める
//   shakes＝ゆれる回数（0〜3）、caught＝捕まえられたか（どちらも捕獲判定の結果）
void BattleScene::StartCatch(int itemId, int shakes, bool caught)
{
	const BallEffectMaster* ball = GetCatchBall(itemId);

	catch_ = {};
	catch_.active = true;
	catch_.stage = CatchStage::THROW;
	catch_.shakes = std::max(0, std::min(shakes, 3));
	catch_.caught = caught;
	catch_.ball = ball;
	catch_.ballImg = LoadMonsterImage(ball->ballImage);
	catch_.openImg = ball->openImage.empty() ? catch_.ballImg : LoadMonsterImage(ball->openImage);
	catch_.openFrames = LoadBallFrames(*ball);
	catch_.catchFrames = LoadCatchFrames(*ball);
}

int BattleScene::CatchStageFrames(void) const
{
	const BallEffectMaster* ball = catch_.ball;
	switch (catch_.stage)
	{
	case CatchStage::THROW:
		return CATCH_THROW_FRAMES;
	case CatchStage::ABSORB:
	case CatchStage::FAIL:
	{
		// ボールが開くエフェクトの長さに合わせる（短いときは最低限の長さ）
		int f = (catch_.stage == CatchStage::ABSORB) ? CATCH_ABSORB_MIN_FRAMES : CATCH_FAIL_MIN_FRAMES;
		if (ball != nullptr && catch_.openFrames != nullptr)
		{
			f = std::max(f, static_cast<int>(catch_.openFrames->size()) * ball->frameTime);
		}
		return f;
	}
	case CatchStage::DROP:
		return CATCH_DROP_FRAMES;
	case CatchStage::SHAKE:
		return CATCH_SHAKE_WAIT_FRAMES + catch_.shakes * CATCH_SHAKE_FRAMES + CATCH_SHAKE_END_FRAMES;
	case CatchStage::SUCCESS:
	{
		int f = CATCH_SUCCESS_MIN_FRAMES;
		if (ball != nullptr && catch_.catchFrames != nullptr)
		{
			f = std::max(f, static_cast<int>(catch_.catchFrames->size()) * ball->catchFrameTime);
		}
		return f;
	}
	}
	return 0;
}

void BattleScene::UpdateCatch(void)
{
	if (!catch_.active)
	{
		return;
	}

	catch_.timer++;

	// 今のステージが終わったら次へ
	while (catch_.timer >= CatchStageFrames())
	{
		catch_.timer = 0;
		switch (catch_.stage)
		{
		case CatchStage::THROW:
			catch_.stage = CatchStage::ABSORB;
			break;
		case CatchStage::ABSORB:
			hidden_[ENEMY] = true;		// 吸い込まれた：モンスターと情報枠を消す
			catch_.stage = CatchStage::DROP;
			break;
		case CatchStage::DROP:
			catch_.stage = CatchStage::SHAKE;
			break;
		case CatchStage::SHAKE:
			if (catch_.caught)
			{
				catch_.stage = CatchStage::SUCCESS;
			}
			else
			{
				hidden_[ENEMY] = false;	// 出てくる：情報枠を戻す
				catch_.stage = CatchStage::FAIL;
			}
			break;
		case CatchStage::SUCCESS:
			catch_.active = false;
			catch_.hold = true;			// ボールは地面に残る（シーンが終わるまで）
			return;
		case CatchStage::FAIL:
			catch_ = {};				// 出てきた：ここから通常の描画に戻る
			return;
		}
	}
}

// 捕まえたときの簡易エフェクト（画像が無いとき）：ボールの色の光の輪と、放射状に広がる星
void BattleScene::DrawCatchSparkle(int x, int y, float t, const BallEffectMaster& ball) const
{
	const unsigned int color = GetColor(ball.glowR, ball.glowG, ball.glowB);
	const float e = 1.0f - (1.0f - t) * (1.0f - t);		// 出だしが速く、だんだん止まる

	// カチッと閉まった瞬間の光
	if (t < 0.2f)
	{
		SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(200 * (1.0f - t / 0.2f)));
		DrawCircle(x, y, SY(22), color, TRUE);
	}

	SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(255 * (1.0f - t)));

	// 光の輪
	DrawCircle(x, y, SY(12) + static_cast<int>(SY(50) * e), color, FALSE, 2);

	// 星（8個）：十字の形で、遠くへ行くほど小さくなる
	for (int i = 0; i < 8; i++)
	{
		const float angle = i * 3.1415926f / 4.0f + 0.4f;
		const float dist = SY(10) + SY(56) * e * ((i % 2 == 0) ? 1.0f : 0.7f);
		const int sx = x + static_cast<int>(std::cos(angle) * dist);
		const int sy = y + static_cast<int>(std::sin(angle) * dist);
		const int r = std::max(1, static_cast<int>(SY(6) * (1.0f - t)) + 1);
		DrawLine(sx - r, sy, sx + r, sy, color, 2);
		DrawLine(sx, sy - r, sx, sy + r, color, 2);
		DrawCircle(sx, sy, std::max(1, r / 2), color, TRUE);
	}
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void BattleScene::DrawCatch(void) const
{
	const CatchPlay& c = catch_;
	if ((!c.active && !c.hold) || c.ball == nullptr)
	{
		return;
	}
	const BallEffectMaster& ball = *c.ball;

	int cx = 0, bottom = 0, size = 0;
	GetMonsterLayout(ENEMY, cx, bottom, size);

	const int centerY = bottom - size / 2;	// モンスターの中心（ボールが当たる場所）
	const int landY = bottom - SY(12);		// ボールが落ちる地面
	const int ballSize = SY(ball.ballSize);

	// 捕まえたあと：ボールは暗くなって地面に残る
	if (c.hold)
	{
		SetDrawBright(150, 150, 150);
		DrawBallSprite(cx, landY, ballSize, 0.0, 255, c.ballImg);
		SetDrawBright(255, 255, 255);
		return;
	}

	const int total = std::max(1, CatchStageFrames());
	const float t = std::min(1.0f, static_cast<float>(c.timer) / total);
	const int enemyImg = side_[ENEMY].image;

	// モンスターを描く（glowAlpha＝ボールの色で光らせる強さ 0〜255）
	auto drawMonster = [&](int bottomY, int sz, int glowAlpha)
		{
			DrawImageFit(enemyImg, cx, bottomY, sz);
			if (glowAlpha > 0)
			{
				SetDrawBlendMode(DX_BLENDMODE_ADD, std::min(255, glowAlpha));
				SetDrawBright(ball.glowR, ball.glowG, ball.glowB);
				DrawImageFit(enemyImg, cx, bottomY, sz);
				SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
				SetDrawBright(255, 255, 255);
			}
		};

	switch (c.stage)
	{
	case CatchStage::THROW:
	{
		// モンスターはそのまま。ボールが手前（左下）から山なりに飛んできて、くるくる回る
		DrawMonsterImage(ENEMY, cx, bottom, size);
		const int startX = SX(110);
		const int startY = SY(250);
		const int x = startX + static_cast<int>((cx - startX) * t);
		const int y = startY + static_cast<int>((centerY - startY) * t - SY(90) * 4.0f * t * (1.0f - t));
		DrawBallSprite(x, y, ballSize, c.timer * 0.6, 255, c.ballImg);
		break;
	}

	case CatchStage::ABSORB:
	{
		// ボールが開いて、モンスターが光になって吸い込まれ、最後にボールが閉じる
		const float k = std::max(0.0f, std::min(1.0f, (t - 0.15f) / 0.6f));
		const float e = k * k;		// 後半ほど速く吸い込まれる
		if (k < 1.0f)
		{
			const int curSize = std::max(2, static_cast<int>(size * (1.0f - e)));
			const int curBottom = bottom + static_cast<int>((centerY - bottom) * e);
			drawMonster(curBottom, curSize, static_cast<int>(255 * e));
		}
		DrawBallSprite(cx, centerY, ballSize, 0.0, 255, (t < 0.75f) ? c.openImg : c.ballImg);
		DrawOpenEffect(cx, centerY, c.timer, std::min(1.0f, t / 0.5f), size, c.openFrames, ball);
		break;
	}

	case CatchStage::DROP:
	{
		// 落ちて、1回バウンドする
		int y = landY;
		if (c.timer < CATCH_FALL_FRAMES)
		{
			const float u = static_cast<float>(c.timer) / CATCH_FALL_FRAMES;
			y = centerY + static_cast<int>((landY - centerY) * u * u);
		}
		else
		{
			const float u = static_cast<float>(c.timer - CATCH_FALL_FRAMES)
				/ std::max(1, CATCH_DROP_FRAMES - CATCH_FALL_FRAMES);
			y = landY - static_cast<int>(SY(24) * 4.0f * u * (1.0f - u));
		}
		DrawBallSprite(cx, y, ballSize, 0.0, 255, c.ballImg);
		break;
	}

	case CatchStage::SHAKE:
	{
		// 地面でゆれる（1回ごとに左右へ傾いて、もとに戻る。回ごとに向きが入れ替わる）
		double angle = 0.0;
		const int local = c.timer - CATCH_SHAKE_WAIT_FRAMES;
		if (local >= 0)
		{
			const int k = local / CATCH_SHAKE_FRAMES;
			if (k < c.shakes)
			{
				const float u = static_cast<float>(local % CATCH_SHAKE_FRAMES) / CATCH_SHAKE_FRAMES;
				if (u < 0.6f)
				{
					angle = 0.45 * std::sin(u / 0.6f * 6.2831853f) * ((k % 2 == 0) ? 1.0 : -1.0);
				}
			}
		}
		const int x = cx + static_cast<int>(angle * ballSize * 0.5);
		DrawBallSprite(x, landY, ballSize, angle, 255, c.ballImg);
		break;
	}

	case CatchStage::SUCCESS:
	{
		// カチッ：ボールが光って、星が広がる。そのあとボールは少し暗くなる（捕まえた印）
		const int bright = 255 - static_cast<int>(105 * std::min(1.0f, t * 2.0f));
		SetDrawBright(bright, bright, bright);
		DrawBallSprite(cx, landY, ballSize, 0.0, 255, c.ballImg);
		SetDrawBright(255, 255, 255);

		if (c.catchFrames != nullptr && !c.catchFrames->empty())
		{
			const int index = std::min(static_cast<int>(c.catchFrames->size()) - 1, c.timer / ball.catchFrameTime);
			const double scale = ball.catchScalePercent / 100.0 * screenH_ / BASE_H;
			if (ball.additive)
			{
				SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
			}
			DrawRotaGraph(cx, landY, scale, 0.0, (*c.catchFrames)[index], TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}
		else
		{
			DrawCatchSparkle(cx, landY, t, ball);
		}
		break;
	}

	case CatchStage::FAIL:
	{
		// ボールが開いて、モンスターが出てくる（モンスターを出す演出と同じ）
		const float k = std::max(0.0f, (t - 0.25f) / 0.75f);
		if (k > 0.0f)
		{
			const float e = 1.0f - (1.0f - k) * (1.0f - k);
			const int curSize = std::max(4, static_cast<int>(size * e));
			const int curBottom = landY + static_cast<int>((bottom - landY) * e);
			drawMonster(curBottom, curSize, static_cast<int>(255 * (1.0f - k)));
		}
		const int ballAlpha = (t < 0.3f) ? 255 : static_cast<int>(255 * (1.0f - (t - 0.3f) / 0.7f));
		DrawBallSprite(cx, landY, ballSize, 0.0, ballAlpha, c.openImg);
		DrawOpenEffect(cx, centerY, c.timer, std::min(1.0f, t / 0.5f), size, c.openFrames, ball);
		break;
	}
	}
}