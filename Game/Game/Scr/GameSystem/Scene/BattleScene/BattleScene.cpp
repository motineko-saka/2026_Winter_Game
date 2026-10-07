#include "BattleScene.h"
#include <algorithm>
#include <cmath>
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

namespace
{
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
}

void BattleScene::SetItems(const ItemData* items, Inventory* inventory)
{
	items_ = items;
	inventory_ = inventory;
}

void BattleScene::Init(void)
{
	steps_.clear();
	learnQueue_.clear();
	evolveQueue_.clear();
	evolveState_ = EvolveState::NONE;
	evolveBegun_ = false;
	needSwitch_ = false;
	runAttempts_ = 0;
	enemyInBall_ = false;
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
		|| !MonsterParty::Create(*data_, wildId_, wildLevel_, enemy_)
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

	// 開始メッセージ
	Say(Name(ENEMY) + "が あらわれた！");
	Push([this]() { Say("ゆけっ！ " + side_[PLAYER].mon->nickname + "！"); });
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
		if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
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
		if (InputManager::GetInstance()->IsTrgDown(KEY_INPUT_RETURN))
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
	if (!enemyInBall_)
	{
		DrawMonsterImage(ENEMY, SX(480), SY(210), SY(180));
	}
	DrawMonsterImage(PLAYER, SX(170), SY(350), SY(200));

	// 敵（左上）とプレイヤー（右下）の情報
	DrawStatusBox(ENEMY, SX(30), SY(30));
	DrawStatusBox(PLAYER, SX(370), SY(250));

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
		TryCatch(item->effectValue / 100.0, item->name);
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
void BattleScene::TryCatch(double ballRate, const std::string& ballName)
{
	Combatant& e = side_[ENEMY];
	const std::string name = e.mon->nickname;

	enemyInBall_ = true;
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

	// ゆれの演出
	for (int i = 0; i < shakes; i++)
	{
		PushSay(i == 0 ? "ボールが ゆれた…" : "…ゆれた…");
	}

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
	std::string msg = failMsg[shakes];
	Push([this, msg]()
		{
			enemyInBall_ = false;	// ボールから出てくる
			Say(msg);
		});
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

		if (crit)
		{
			PushSay("きゅうしょに あたった！");
		}
		if (eff > 1.0f)
		{
			PushSay("こうかは ばつぐんだ！");
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
		if (result_ == Result::NONE)
		{
			result_ = Result::WIN;
		}
		const int exp = MonsterGrowth::CalcGainExp(*side_[ENEMY].master, side_[ENEMY].mon->level, false);
		Push([this, exp]() { GainPlayerExp(exp); });
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
	if (input->IsTrgDown(KEY_INPUT_UP))
	{
		cursor_ = (cursor_ + count - 1) % count;
	}
	if (input->IsTrgDown(KEY_INPUT_DOWN))
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

	if (!input->IsTrgDown(KEY_INPUT_RETURN))
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
		BeginTurn(ActionType::RUN, 0);
		break;
	}
}

void BattleScene::UpdateMoveSelect(void)
{
	auto* input = InputManager::GetInstance();
	MoveCursor(4);

	if (input->IsTrgDown(KEY_INPUT_BACK))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 0;
		return;
	}
	if (input->IsTrgDown(KEY_INPUT_RETURN))
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
	if (!needSwitch_ && input->IsTrgDown(KEY_INPUT_BACK))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 1;
		return;
	}
	if (!input->IsTrgDown(KEY_INPUT_RETURN))
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

	if (!input->IsTrgDown(KEY_INPUT_RETURN))
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

	if (input->IsTrgDown(KEY_INPUT_BACK))
	{
		phase_ = Phase::COMMAND;
		cursor_ = 2;
		return;
	}
	if (!input->IsTrgDown(KEY_INPUT_RETURN) || bagList_.empty())
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

	if (input->IsTrgDown(KEY_INPUT_BACK))
	{
		// バッグに戻る（さっき選んでいたアイテムにカーソルを合わせる）
		phase_ = Phase::BAG;
		auto it = std::find(bagList_.begin(), bagList_.end(), usingItemId_);
		cursor_ = (it != bagList_.end()) ? static_cast<int>(it - bagList_.begin()) : 0;
		return;
	}
	if (!input->IsTrgDown(KEY_INPUT_RETURN))
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
	if (input->IsTrgDown(KEY_INPUT_BACK))
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

	if (input->IsTrgDown(KEY_INPUT_BACK))
	{
		CancelEvolve();
		return;
	}
	if (input->IsTrgDown(KEY_INPUT_RETURN))
	{
		evolveCur_.toId = evolveCur_.candidates[cursor_];
		evolveNewImg_ = evolveCandImgs_[cursor_];
		BeginEvolveAnim();
	}
}

void BattleScene::UpdateHpBars(void)
{
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
	DrawImageFit(side_[side].image, cx, bottomY, size);
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
		DrawFormatString(x + SX(10), y + SY(44), black, "HP %d / %d", std::max(0, c.mon->currentHp), maxHp);
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

	if (phase_ == Phase::MESSAGE || phase_ == Phase::END)
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
	return (side == ENEMY) ? "野生の " + n : n;
}

int BattleScene::Rand(int lo, int hi)
{
	return std::uniform_int_distribution<int>(lo, hi)(rng_);
}