#include "PauseScene.h"

#include <algorithm>
#include <memory>
#include <DxLib.h>

#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Monster/MonsterData.h"
#include "../../Object/Monster/MonsterParty.h"
#include "../../Object/Item/ItemData.h"
#include "../../Object/Item/Inventory.h"
#include "../../Object/Item/ItemUse.h"

#include "../SceneManager.h"
#include "../MapScene/MapScene.h"
#include "../TitleScene/TitleScene.h"

namespace
{
	// メインメニューの項目
	enum MenuItem
	{
		MENU_PARTY,
		MENU_BAG,
		MENU_MAP,
		MENU_SAVE,
		MENU_OPTION,
		MENU_TITLE,
		MENU_COUNT,
	};

	const char* const MENU_LABELS[MENU_COUNT] =
	{
		"てもち", "もちもの", "マップ", "セーブ", "オプション", "タイトルへ もどる",
	};

	// てもちを選んだあとの項目
	const int ACTION_COUNT = 3;
	const char* const ACTION_LABELS[ACTION_COUNT] = { "つよさを みる", "いれかえる", "もどる" };

	const int BAG_VISIBLE = 8;		// もちもの一覧に一度に出す行数

	// 色（0xRRGGBB）
	const unsigned int BLACK = 0x000000;
	const unsigned int WHITE = 0xFFFFFF;
	const unsigned int GRAY = 0x999999;
	const unsigned int RED = 0xCC2222;
	const unsigned int ORANGE = 0xFF8800;
	const unsigned int BLUE = 0x3366CC;

	bool Trg(int key)
	{
		return InputManager::GetInstance()->IsTrgDown(key);
	}

	bool IsDecide(void)
	{
		return Trg(KEY_INPUT_RETURN);
	}

	bool IsCancel(void)
	{
		return Trg(KEY_INPUT_ESCAPE) || Trg(KEY_INPUT_BACK);
	}

	const char* TypeName(ElementType t)
	{
		static const char* const names[] =
		{
			"炎", "水", "風", "ノーマル", "宝石", "機械", "霊", "光", "闇",
			"龍", "音", "幻", "業", "氷", "毒", "土", "武闘", "神",
		};
		const int i = static_cast<int>(t);
		if (i < 0 || i >= static_cast<int>(sizeof(names) / sizeof(names[0])))
		{
			return "";
		}
		return names[i];
	}

	const char* AilmentName(StatusAilment a)
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

	// フィールドで使えるアイテムか（回復系だけ）
	bool IsFieldUsable(const ItemMasterData& item)
	{
		if (!item.usableInField)
		{
			return false;
		}
		switch (item.effect)
		{
		case ItemEffect::HEAL_HP:
		case ItemEffect::REVIVE:
		case ItemEffect::CURE_STATUS:
		case ItemEffect::RESTORE_PP:
			return true;
		default:
			return false;
		}
	}
}

PauseScene::PauseScene(void)
{
}

PauseScene::~PauseScene(void)
{
}

void PauseScene::Setup(const MonsterData* data, MonsterParty* party,
	const ItemData* items, Inventory* inventory,
	std::function<bool(void)> saveFunc)
{
	data_ = data;
	party_ = party;
	items_ = items;
	inventory_ = inventory;
	saveFunc_ = std::move(saveFunc);
}

void PauseScene::Init(void)
{
	mode_ = Mode::MENU;
	returnMode_ = Mode::MENU;
	menuCursor_ = 0;
	partyCursor_ = 0;
	actionCursor_ = 0;
	swapFrom_ = 0;
	bagCursor_ = 0;
	targetCursor_ = 0;
	confirmCursor_ = 1;
	usingItemId_ = 0;
	bagList_.clear();
	msg_.clear();
}

void PauseScene::Load(void)
{
}

void PauseScene::LoadEnd(void)
{
}

void PauseScene::Update(void)
{
	switch (mode_)
	{
	case Mode::MENU:			UpdateMenu();			break;
	case Mode::PARTY:			UpdateParty();			break;
	case Mode::PARTY_ACTION:	UpdatePartyAction();	break;
	case Mode::PARTY_DETAIL:	UpdatePartyDetail();	break;
	case Mode::PARTY_SWAP:		UpdatePartySwap();		break;
	case Mode::BAG:				UpdateBag();			break;
	case Mode::BAG_TARGET:		UpdateBagTarget();		break;
	case Mode::OPTION:			UpdateOption();			break;
	case Mode::CONFIRM_TITLE:	UpdateConfirmTitle();	break;
	case Mode::MESSAGE:			UpdateMessage();		break;
	}
}

void PauseScene::Draw(void)
{
	// 後ろのゲーム画面を暗くする
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 127);
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, BLACK, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	if (mode_ == Mode::MESSAGE)
	{
		DrawScreen(returnMode_);
		DrawMessage();
	}
	else
	{
		DrawScreen(mode_);
	}
}

void PauseScene::Release(void)
{
	for (auto& pair : images_)
	{
		if (pair.second >= 0)
		{
			DeleteGraph(pair.second);
		}
	}
	images_.clear();
}

// =============================================================
// 更新
// =============================================================

void PauseScene::UpdateMenu(void)
{
	if (IsCancel())
	{
		// メニューを閉じてゲームに戻る
		SceneManager::GetInstance()->PopScene();
		return;
	}

	MoveCursor(menuCursor_, MENU_COUNT);

	if (!IsDecide())
	{
		return;
	}

	switch (menuCursor_)
	{
	case MENU_PARTY:
		if (party_ == nullptr || party_->GetCount() == 0)
		{
			ShowMessage({ "てもちの モンスターが いません" }, Mode::MENU);
		}
		else
		{
			partyCursor_ = 0;
			mode_ = Mode::PARTY;
		}
		break;

	case MENU_BAG:
		RefreshBagList();
		bagCursor_ = 0;
		mode_ = Mode::BAG;
		break;

	case MENU_MAP:
		SceneManager::GetInstance()->PushScene(std::make_shared<MapScene>());
		break;

	case MENU_SAVE:
		if (saveFunc_ && saveFunc_())
		{
			ShowMessage({ "セーブしました！" }, Mode::MENU);
		}
		else
		{
			ShowMessage({ "セーブに しっぱいしました…" }, Mode::MENU);
		}
		break;

	case MENU_OPTION:
		mode_ = Mode::OPTION;
		break;

	case MENU_TITLE:
		confirmCursor_ = 1;		// 誤操作を防ぐため「いいえ」から始める
		mode_ = Mode::CONFIRM_TITLE;
		break;
	}
}

void PauseScene::UpdateParty(void)
{
	if (IsCancel())
	{
		mode_ = Mode::MENU;
		return;
	}

	MoveCursor(partyCursor_, party_->GetCount());

	if (IsDecide())
	{
		actionCursor_ = 0;
		mode_ = Mode::PARTY_ACTION;
	}
}

void PauseScene::UpdatePartyAction(void)
{
	if (IsCancel())
	{
		mode_ = Mode::PARTY;
		return;
	}

	MoveCursor(actionCursor_, ACTION_COUNT);

	if (!IsDecide())
	{
		return;
	}

	switch (actionCursor_)
	{
	case 0:		// つよさを みる
		mode_ = Mode::PARTY_DETAIL;
		break;

	case 1:		// いれかえる
		if (party_->GetCount() < 2)
		{
			ShowMessage({ "いれかえる あいてが いません" }, Mode::PARTY_ACTION);
		}
		else
		{
			swapFrom_ = partyCursor_;
			mode_ = Mode::PARTY_SWAP;
		}
		break;

	default:	// もどる
		mode_ = Mode::PARTY;
		break;
	}
}

void PauseScene::UpdatePartyDetail(void)
{
	const int count = party_->GetCount();

	// 左右で、ほかのモンスターのつよさに切り替える
	if (Trg(KEY_INPUT_LEFT))
	{
		partyCursor_ = (partyCursor_ + count - 1) % count;
	}
	else if (Trg(KEY_INPUT_RIGHT))
	{
		partyCursor_ = (partyCursor_ + 1) % count;
	}

	if (IsCancel() || IsDecide())
	{
		mode_ = Mode::PARTY_ACTION;
	}
}

void PauseScene::UpdatePartySwap(void)
{
	if (IsCancel())
	{
		partyCursor_ = swapFrom_;
		mode_ = Mode::PARTY;
		return;
	}

	MoveCursor(partyCursor_, party_->GetCount());

	if (IsDecide())
	{
		if (partyCursor_ != swapFrom_)
		{
			party_->Swap(swapFrom_, partyCursor_);
		}
		mode_ = Mode::PARTY;
	}
}

void PauseScene::UpdateBag(void)
{
	if (IsCancel())
	{
		mode_ = Mode::MENU;
		return;
	}

	MoveCursor(bagCursor_, static_cast<int>(bagList_.size()));

	if (!IsDecide() || bagList_.empty())
	{
		return;
	}

	const ItemMasterData* item = (items_ != nullptr) ? items_->GetItem(bagList_[bagCursor_]) : nullptr;
	if (item == nullptr)
	{
		return;
	}

	if (!IsFieldUsable(*item))
	{
		ShowMessage({ "それは ここでは つかえない！" }, Mode::BAG);
	}
	else if (party_ == nullptr || party_->GetCount() == 0)
	{
		ShowMessage({ "つかう あいてが いません" }, Mode::BAG);
	}
	else
	{
		usingItemId_ = item->id;
		targetCursor_ = 0;
		mode_ = Mode::BAG_TARGET;
	}
}

void PauseScene::UpdateBagTarget(void)
{
	if (IsCancel())
	{
		mode_ = Mode::BAG;
		return;
	}

	MoveCursor(targetCursor_, party_->GetCount());

	if (IsDecide())
	{
		UseItemOnTarget();
	}
}

void PauseScene::UpdateOption(void)
{
	// ※設定項目はまだない。決まったらここに追加する
	if (IsCancel() || IsDecide())
	{
		mode_ = Mode::MENU;
	}
}

void PauseScene::UpdateConfirmTitle(void)
{
	if (IsCancel())
	{
		mode_ = Mode::MENU;
		return;
	}

	if (Trg(KEY_INPUT_LEFT) || Trg(KEY_INPUT_RIGHT) || Trg(KEY_INPUT_UP) || Trg(KEY_INPUT_DOWN))
	{
		confirmCursor_ = 1 - confirmCursor_;
	}

	if (!IsDecide())
	{
		return;
	}

	if (confirmCursor_ == 0)
	{
		// タイトルへ戻る（ゲーム画面も含めて全シーンを解放してから切り替える）
		SceneManager::GetInstance()->ChangeSceneAll(std::make_shared<TitleScene>());
	}
	else
	{
		mode_ = Mode::MENU;
	}
}

void PauseScene::UpdateMessage(void)
{
	if (IsDecide() || IsCancel())
	{
		mode_ = returnMode_;
	}
}

void PauseScene::MoveCursor(int& cursor, int count)
{
	if (count <= 0)
	{
		cursor = 0;
		return;
	}

	if (Trg(KEY_INPUT_UP))
	{
		cursor = (cursor + count - 1) % count;
	}
	else if (Trg(KEY_INPUT_DOWN))
	{
		cursor = (cursor + 1) % count;
	}
}

void PauseScene::ShowMessage(const std::vector<std::string>& lines, Mode returnMode)
{
	msg_ = lines;
	returnMode_ = returnMode;
	mode_ = Mode::MESSAGE;
}

void PauseScene::RefreshBagList(void)
{
	bagList_.clear();
	if (items_ == nullptr || inventory_ == nullptr)
	{
		return;
	}

	for (const auto& pair : inventory_->GetAll())
	{
		if (items_->GetItem(pair.first) != nullptr)
		{
			bagList_.push_back(pair.first);
		}
	}
}

void PauseScene::UseItemOnTarget(void)
{
	const ItemMasterData* item = (items_ != nullptr) ? items_->GetItem(usingItemId_) : nullptr;
	MonsterInstance* target = party_->Get(targetCursor_);
	std::string result;

	if (item != nullptr && target != nullptr && inventory_ != nullptr
		&& inventory_->Has(usingItemId_)
		&& ItemUse::ApplyToMonster(*data_, *item, *target, result))
	{
		// 効果があったときだけ消費する
		if (item->consumable)
		{
			inventory_->Remove(usingItemId_);
		}

		RefreshBagList();
		const int last = std::max(0, static_cast<int>(bagList_.size()) - 1);
		bagCursor_ = std::min(bagCursor_, last);

		ShowMessage({ item->name + "を つかった！", result }, Mode::BAG);
	}
	else
	{
		ShowMessage({ "しかし なにも おこらなかった！" }, Mode::BAG);
	}
}

// =============================================================
// 描画
// =============================================================

void PauseScene::DrawScreen(Mode mode)
{
	switch (mode)
	{
	case Mode::MENU:
		DrawMenu();
		break;
	case Mode::PARTY:
		DrawPartyList(partyCursor_, -1, "てもち");
		break;
	case Mode::PARTY_ACTION:
		DrawPartyList(partyCursor_, -1, "てもち");
		DrawPartyAction();
		break;
	case Mode::PARTY_DETAIL:
		DrawPartyDetail();
		break;
	case Mode::PARTY_SWAP:
		DrawPartyList(partyCursor_, swapFrom_, "だれと いれかえますか？");
		break;
	case Mode::BAG:
		DrawBag();
		break;
	case Mode::BAG_TARGET:
		DrawPartyList(targetCursor_, -1, "だれに つかいますか？");
		break;
	case Mode::OPTION:
		DrawOption();
		break;
	case Mode::CONFIRM_TITLE:
		DrawMenu();
		DrawConfirmTitle();
		break;
	default:
		break;
	}
}

void PauseScene::DrawWindow(int x1, int y1, int x2, int y2)
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
	DrawBox(x1, y1, x2, y2, WHITE, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawBox(x1, y1, x2, y2, BLACK, false);
}

void PauseScene::DrawMenu(void)
{
	const int x1 = SX(420);
	const int y1 = SY(20);
	DrawWindow(x1, y1, SX(620), SY(20 + 20 + MENU_COUNT * 38));

	for (int i = 0; i < MENU_COUNT; i++)
	{
		const int y = SY(34 + i * 38);
		if (i == menuCursor_)
		{
			DrawFormatString(x1 + SX(12), y, ORANGE, ">");
		}
		DrawFormatString(x1 + SX(34), y, BLACK, "%s", MENU_LABELS[i]);
	}
}

void PauseScene::DrawHpBar(int x, int y, int w, int h, float ratio)
{
	ratio = std::max(0.0f, std::min(1.0f, ratio));

	DrawBox(x, y, x + w, y + h, 0x444444, true);

	unsigned int color = 0x33BB33;			// 緑
	if (ratio <= 0.2f)
	{
		color = 0xDD3333;					// 赤
	}
	else if (ratio <= 0.5f)
	{
		color = 0xDDBB22;					// 黄
	}
	DrawBox(x, y, x + static_cast<int>(w * ratio), y + h, color, true);
	DrawBox(x, y, x + w, y + h, BLACK, false);
}

void PauseScene::DrawPartyList(int cursor, int markIndex, const char* title)
{
	DrawWindow(SX(20), SY(20), SX(620), SY(460));
	DrawFormatString(SX(36), SY(30), BLACK, "%s", title);

	const int count = party_->GetCount();
	for (int i = 0; i < count; i++)
	{
		MonsterInstance* mon = party_->Get(i);
		if (mon == nullptr)
		{
			continue;
		}

		const int y = SY(58 + i * 66);
		const int x1 = SX(36);
		const int x2 = SX(604);
		const int y2 = y + SY(60);

		// 選択中の行を強調する
		if (i == cursor)
		{
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, 90);
			DrawBox(x1, y, x2, y2, ORANGE, true);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			DrawBox(x1, y, x2, y2, ORANGE, false);
		}
		else if (i == markIndex)
		{
			DrawBox(x1, y, x2, y2, BLUE, false);
		}

		const MonsterMasterData* master = data_->GetMonster(mon->monsterId);
		const int maxHp = std::max(1, MonsterParty::GetMaxHp(*data_, *mon));
		const int hp = std::max(0, mon->currentHp);

		if (master != nullptr)
		{
			DrawImageFit(GetImage(master->iconPath), x1 + SX(6), y + SY(6), SY(48));
		}

		DrawFormatString(x1 + SX(64), y + SY(8), BLACK, "%s   Lv%d", mon->nickname.c_str(), mon->level);

		const char* ailment = AilmentName(mon->ailment);
		if (ailment[0] != '\0')
		{
			DrawFormatString(x1 + SX(280), y + SY(8), RED, "%s", ailment);
		}

		DrawHpBar(x1 + SX(64), y + SY(34), SX(220), SY(10), static_cast<float>(hp) / static_cast<float>(maxHp));

		if (hp <= 0)
		{
			DrawFormatString(x1 + SX(310), y + SY(30), RED, "ひんし");
		}
		else
		{
			DrawFormatString(x1 + SX(310), y + SY(30), BLACK, "HP %d / %d", hp, maxHp);
		}
	}
}

void PauseScene::DrawPartyAction(void)
{
	const int x1 = SX(400);
	const int y1 = SY(300);
	DrawWindow(x1, y1, SX(600), y1 + SY(20 + ACTION_COUNT * 36));

	for (int i = 0; i < ACTION_COUNT; i++)
	{
		const int y = y1 + SY(14 + i * 36);
		if (i == actionCursor_)
		{
			DrawFormatString(x1 + SX(12), y, ORANGE, ">");
		}
		DrawFormatString(x1 + SX(34), y, BLACK, "%s", ACTION_LABELS[i]);
	}
}

void PauseScene::DrawPartyDetail(void)
{
	DrawWindow(SX(20), SY(20), SX(620), SY(460));

	MonsterInstance* mon = party_->Get(partyCursor_);
	const MonsterMasterData* master = (mon != nullptr) ? data_->GetMonster(mon->monsterId) : nullptr;
	if (mon == nullptr || master == nullptr)
	{
		return;
	}

	// 左：画像
	DrawImageFit(GetImage(master->frontPath), SX(40), SY(50), SY(200));

	// 右：基本情報
	const int tx = SX(280);
	const int maxHp = std::max(1, MonsterParty::GetMaxHp(*data_, *mon));
	const int hp = std::max(0, mon->currentHp);

	DrawFormatString(tx, SY(40), BLACK, "%s   Lv%d", mon->nickname.c_str(), mon->level);

	if (master->type2 != ElementType::NONE)
	{
		DrawFormatString(tx, SY(70), BLACK, "タイプ： %s / %s", TypeName(master->type1), TypeName(master->type2));
	}
	else
	{
		DrawFormatString(tx, SY(70), BLACK, "タイプ： %s", TypeName(master->type1));
	}

	DrawHpBar(tx, SY(102), SX(200), SY(10), static_cast<float>(hp) / static_cast<float>(maxHp));
	DrawFormatString(tx + SX(210), SY(98), BLACK, "HP %d / %d", hp, maxHp);

	const char* ailment = AilmentName(mon->ailment);
	DrawFormatString(tx, SY(124), (ailment[0] != '\0') ? RED : BLACK, "じょうたい： %s", (ailment[0] != '\0') ? ailment : "げんき");
	DrawFormatString(tx, SY(150), BLACK, "けいけんち： %d", mon->exp);

	DrawFormatString(tx, SY(184), BLACK, "こうげき： %d", MonsterParty::CalcAttack(*master, mon->level));
	DrawFormatString(tx, SY(208), BLACK, "ぼうぎょ： %d", MonsterParty::CalcDefense(*master, mon->level));
	DrawFormatString(tx, SY(232), BLACK, "すばやさ： %d", MonsterParty::CalcSpeed(*master, mon->level));

	// 下：わざ
	DrawFormatString(SX(40), SY(270), BLACK, "わざ");
	int row = 0;
	for (const MoveSlot& slot : mon->moves)
	{
		if (slot.moveId == MonsterParty::MOVE_NONE)
		{
			continue;
		}
		const MoveData* move = data_->GetMove(slot.moveId);
		if (move == nullptr)
		{
			continue;
		}

		const int y = SY(298 + row * 30);
		DrawFormatString(SX(60), y, BLACK, "%s", move->name.c_str());
		DrawFormatString(SX(300), y, BLACK, "%s", TypeName(move->type));
		DrawFormatString(SX(400), y, BLACK, "PP %d / %d", slot.currentPp, move->pp);
		row++;
	}

	DrawFormatString(SX(40), SY(430), GRAY, "←→：ほかの モンスター　Enter／ESC：もどる");
}

void PauseScene::DrawBag(void)
{
	DrawWindow(SX(20), SY(20), SX(620), SY(460));
	DrawFormatString(SX(36), SY(30), BLACK, "もちもの");

	if (bagList_.empty())
	{
		DrawFormatString(SX(60), SY(80), GRAY, "もちものが ありません");
		return;
	}

	// 一覧（カーソルが画面外に出ないようにスクロールさせる）
	const int total = static_cast<int>(bagList_.size());
	int top = bagCursor_ - BAG_VISIBLE / 2;
	top = std::max(0, std::min(top, total - BAG_VISIBLE));

	for (int row = 0; row < BAG_VISIBLE && top + row < total; row++)
	{
		const int index = top + row;
		const int id = bagList_[index];
		const ItemMasterData* item = items_->GetItem(id);
		if (item == nullptr)
		{
			continue;
		}

		const int y = SY(62 + row * 34);
		const unsigned int color = IsFieldUsable(*item) ? BLACK : GRAY;	// ここで使えないものは灰色

		if (index == bagCursor_)
		{
			DrawFormatString(SX(40), y, ORANGE, ">");
		}
		DrawFormatString(SX(64), y, color, "%s", item->name.c_str());
		DrawFormatString(SX(420), y, color, "x%2d", inventory_->GetCount(id));
	}

	if (top > 0)
	{
		DrawFormatString(SX(580), SY(56), GRAY, "▲");
	}
	if (top + BAG_VISIBLE < total)
	{
		DrawFormatString(SX(580), SY(62 + BAG_VISIBLE * 34 - 20), GRAY, "▼");
	}

	// 説明
	const ItemMasterData* sel = items_->GetItem(bagList_[bagCursor_]);
	if (sel != nullptr)
	{
		DrawBox(SX(36), SY(350), SX(604), SY(440), BLACK, false);
		DrawFormatString(SX(50), SY(364), BLACK, "%s", sel->description.c_str());
		if (!IsFieldUsable(*sel))
		{
			DrawFormatString(SX(50), SY(400), GRAY, "（ここでは つかえません）");
		}
	}
}

void PauseScene::DrawOption(void)
{
	DrawWindow(SX(120), SY(140), SX(520), SY(340));
	DrawFormatString(SX(150), SY(160), BLACK, "オプション");
	DrawFormatString(SX(150), SY(220), GRAY, "せっていこうもくは まだ ありません");
	DrawFormatString(SX(150), SY(300), GRAY, "Enter／ESC：もどる");
}

void PauseScene::DrawConfirmTitle(void)
{
	DrawWindow(SX(100), SY(190), SX(540), SY(320));
	DrawFormatString(SX(124), SY(206), BLACK, "セーブしていない データは きえます。");
	DrawFormatString(SX(124), SY(234), BLACK, "タイトルへ もどりますか？");

	const char* labels[2] = { "はい", "いいえ" };
	for (int i = 0; i < 2; i++)
	{
		const int x = SX(180 + i * 160);
		if (i == confirmCursor_)
		{
			DrawFormatString(x - SX(22), SY(280), ORANGE, ">");
		}
		DrawFormatString(x, SY(280), BLACK, "%s", labels[i]);
	}
}

void PauseScene::DrawMessage(void)
{
	DrawWindow(SX(20), SY(360), SX(620), SY(450));

	for (size_t i = 0; i < msg_.size(); i++)
	{
		DrawFormatString(SX(40), SY(376) + static_cast<int>(i) * SY(28), BLACK, "%s", msg_[i].c_str());
	}
	DrawFormatString(SX(590), SY(424), BLACK, "▼");
}

// =============================================================
// 補助
// =============================================================

int PauseScene::GetImage(const std::string& path)
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

	const int handle = LoadGraph(path.c_str());
	images_[path] = handle;		// 失敗(-1)も覚えておき、毎フレーム読み直さない
	return handle;
}

void PauseScene::DrawImageFit(int img, int x, int y, int size)
{
	if (img < 0)
	{
		return;
	}

	int w = 0;
	int h = 0;
	GetGraphSize(img, &w, &h);
	if (w <= 0 || h <= 0)
	{
		return;
	}

	const double scale = std::min(static_cast<double>(size) / w, static_cast<double>(size) / h);
	const int dw = static_cast<int>(w * scale);
	const int dh = static_cast<int>(h * scale);
	const int dx = x + (size - dw) / 2;
	const int dy = y + (size - dh) / 2;
	DrawExtendGraph(dx, dy, dx + dw, dy + dh, img, true);
}

int PauseScene::SX(int x) const
{
	return x * Application::SCREEN_SIZE_X / BASE_W;
}

int PauseScene::SY(int y) const
{
	return y * Application::SCREEN_SIZE_Y / BASE_H;
}