#include "ShopScene.h"

#include <algorithm>
#include <DxLib.h>

#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Item/ItemData.h"
#include "../../Object/Item/Inventory.h"
#include "../SceneManager.h"

namespace
{
	// メインメニューの項目
	enum MenuItem
	{
		MENU_BUY,
		MENU_SELL,
		MENU_QUIT,
		MENU_COUNT,
	};

	const char* const MENU_LABELS[MENU_COUNT] = { "かう", "うる", "やめる" };

	const int LIST_VISIBLE = 8;		// 一覧に一度に出す行数
	const int LIST_ROW_H = 34;		// 一覧の1行の高さ
	const int COUNT_STEP_BIG = 10;	// 左右キーで増減する個数

	// 色（0xRRGGBB）
	const unsigned int BLACK = 0x000000;
	const unsigned int WHITE = 0xFFFFFF;
	const unsigned int GRAY = 0x999999;
	const unsigned int ORANGE = 0xFF8800;

	bool Trg(int key)
	{
		return InputManager::GetInstance()->IsTrgDown(key);
	}

	bool IsDecide(void)
	{
		return Trg(KEY_INPUT_RETURN) || Trg(KEY_INPUT_SPACE);
	}

	bool IsCancel(void)
	{
		return Trg(KEY_INPUT_ESCAPE) || Trg(KEY_INPUT_BACK);
	}

	bool IsUp(void) { return Trg(KEY_INPUT_UP) || Trg(KEY_INPUT_W); }
	bool IsDown(void) { return Trg(KEY_INPUT_DOWN) || Trg(KEY_INPUT_S); }
	bool IsLeft(void) { return Trg(KEY_INPUT_LEFT) || Trg(KEY_INPUT_A); }
	bool IsRight(void) { return Trg(KEY_INPUT_RIGHT) || Trg(KEY_INPUT_D); }
}

ShopScene::ShopScene(void)
{
}

ShopScene::~ShopScene(void)
{
}

void ShopScene::Setup(const ItemData* items, Inventory* inventory, int* money,
	const std::vector<int>& stock)
{
	items_ = items;
	inventory_ = inventory;
	money_ = money;

	// Item.csv にあって、買値がついているものだけを並べる
	stock_.clear();
	for (int id : stock)
	{
		const ItemMasterData* item = (items_ != nullptr) ? items_->GetItem(id) : nullptr;
		if (item != nullptr && item->price > 0)
		{
			stock_.push_back(id);
		}
	}
}

void ShopScene::Init(void)
{
	mode_ = Mode::MENU;
	returnMode_ = Mode::MENU;
	menuCursor_ = 0;
	buyCursor_ = 0;
	sellCursor_ = 0;
	count_ = 1;
	selectedItemId_ = 0;
	sellList_.clear();
	msg_.clear();
	msg_.push_back("いらっしゃいませ！");
	msg_.push_back("なにを おさがしですか？");
}

void ShopScene::Load(void)
{
}

void ShopScene::LoadEnd(void)
{
}

void ShopScene::Update(void)
{
	// 必要なデータが渡されていなければ、何もせず閉じる
	if (items_ == nullptr || inventory_ == nullptr || money_ == nullptr)
	{
		Close();
		return;
	}

	switch (mode_)
	{
	case Mode::MENU:		UpdateMenu();		break;
	case Mode::BUY_LIST:	UpdateBuyList();	break;
	case Mode::BUY_COUNT:	UpdateBuyCount();	break;
	case Mode::SELL_LIST:	UpdateSellList();	break;
	case Mode::SELL_COUNT:	UpdateSellCount();	break;
	case Mode::MESSAGE:		UpdateMessage();	break;
	}
}

void ShopScene::Draw(void)
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

void ShopScene::Release(void)
{
}

// =============================================================
// 更新
// =============================================================

void ShopScene::UpdateMenu(void)
{
	if (IsCancel())
	{
		Close();
		return;
	}

	MoveCursor(menuCursor_, MENU_COUNT);

	if (!IsDecide())
	{
		return;
	}

	switch (menuCursor_)
	{
	case MENU_BUY:
		if (stock_.empty())
		{
			ShowMessage({ "いま うれるものは ありません。" }, Mode::MENU);
		}
		else
		{
			buyCursor_ = std::min(buyCursor_, static_cast<int>(stock_.size()) - 1);
			mode_ = Mode::BUY_LIST;
		}
		break;

	case MENU_SELL:
		RefreshSellList();
		if (sellList_.empty())
		{
			ShowMessage({ "うれるものを もっていないようです。" }, Mode::MENU);
		}
		else
		{
			sellCursor_ = std::min(sellCursor_, static_cast<int>(sellList_.size()) - 1);
			mode_ = Mode::SELL_LIST;
		}
		break;

	case MENU_QUIT:
		Close();
		break;
	}
}

void ShopScene::UpdateBuyList(void)
{
	if (IsCancel())
	{
		mode_ = Mode::MENU;
		return;
	}

	MoveCursor(buyCursor_, static_cast<int>(stock_.size()));

	if (!IsDecide() || stock_.empty())
	{
		return;
	}

	const int id = stock_[buyCursor_];
	const ItemMasterData* item = items_->GetItem(id);
	if (item == nullptr)
	{
		return;
	}

	if (*money_ < item->price)
	{
		ShowMessage({ "おかねが たりません。" }, Mode::BUY_LIST);
	}
	else if (inventory_->GetCount(id) >= Inventory::COUNT_MAX)
	{
		ShowMessage({ "それいじょうは もてません。" }, Mode::BUY_LIST);
	}
	else
	{
		selectedItemId_ = id;
		count_ = 1;
		mode_ = Mode::BUY_COUNT;
	}
}

void ShopScene::UpdateBuyCount(void)
{
	const ItemMasterData* item = items_->GetItem(selectedItemId_);
	if (item == nullptr || IsCancel())
	{
		mode_ = Mode::BUY_LIST;
		return;
	}

	const int maxCount = GetMaxBuyCount(selectedItemId_);
	if (maxCount <= 0)
	{
		mode_ = Mode::BUY_LIST;
		return;
	}
	ChangeCount(maxCount);

	if (!IsDecide())
	{
		return;
	}

	// 買う（所持金と上限は、決定の瞬間にもう一度確かめる）
	const int count = std::min(count_, maxCount);
	const int total = item->price * count;
	if (*money_ < total || !inventory_->Add(selectedItemId_, count))
	{
		ShowMessage({ "かうことが できませんでした。" }, Mode::BUY_LIST);
		return;
	}

	*money_ -= total;
	ShowMessage({
		item->name + "を " + std::to_string(count) + "こ おかいあげ。",
		"ありがとうございました！" }, Mode::BUY_LIST);
}

void ShopScene::UpdateSellList(void)
{
	if (IsCancel())
	{
		mode_ = Mode::MENU;
		return;
	}

	MoveCursor(sellCursor_, static_cast<int>(sellList_.size()));

	if (!IsDecide() || sellList_.empty())
	{
		return;
	}

	const int id = sellList_[sellCursor_];
	if (GetSellPrice(id) <= 0)
	{
		ShowMessage({ "それは かいとれません。" }, Mode::SELL_LIST);
		return;
	}

	selectedItemId_ = id;
	count_ = 1;
	mode_ = Mode::SELL_COUNT;
}

void ShopScene::UpdateSellCount(void)
{
	const ItemMasterData* item = items_->GetItem(selectedItemId_);
	const int owned = inventory_->GetCount(selectedItemId_);
	const int unitPrice = GetSellPrice(selectedItemId_);
	if (item == nullptr || owned <= 0 || unitPrice <= 0 || IsCancel())
	{
		mode_ = Mode::SELL_LIST;
		return;
	}

	ChangeCount(owned);

	if (!IsDecide())
	{
		return;
	}

	const int count = std::min(count_, owned);
	if (!inventory_->Remove(selectedItemId_, count))
	{
		ShowMessage({ "うることが できませんでした。" }, Mode::SELL_LIST);
		return;
	}

	*money_ = std::min(MONEY_MAX, *money_ + unitPrice * count);

	// 売り切れたものは一覧から消える
	RefreshSellList();
	if (sellList_.empty())
	{
		ShowMessage({
			item->name + "を " + std::to_string(count) + "こ かいとりました。",
			"まいどあり！" }, Mode::MENU);
		return;
	}
	sellCursor_ = std::min(sellCursor_, static_cast<int>(sellList_.size()) - 1);
	ShowMessage({
		item->name + "を " + std::to_string(count) + "こ かいとりました。",
		"まいどあり！" }, Mode::SELL_LIST);
}

void ShopScene::UpdateMessage(void)
{
	if (IsDecide() || IsCancel())
	{
		mode_ = returnMode_;

		// メニューに戻るときは、お店の人のことばを入れ替える
		if (mode_ == Mode::MENU)
		{
			msg_.clear();
			msg_.push_back("ほかに ごようは ありますか？");
		}
	}
}

void ShopScene::MoveCursor(int& cursor, int count)
{
	if (count <= 0)
	{
		cursor = 0;
		return;
	}

	if (IsUp())
	{
		cursor = (cursor + count - 1) % count;
	}
	else if (IsDown())
	{
		cursor = (cursor + 1) % count;
	}
}

void ShopScene::ChangeCount(int maxCount)
{
	if (IsUp())
	{
		count_++;
	}
	else if (IsDown())
	{
		count_--;
	}
	else if (IsRight())
	{
		count_ += COUNT_STEP_BIG;
	}
	else if (IsLeft())
	{
		count_ -= COUNT_STEP_BIG;
	}
	count_ = std::max(1, std::min(count_, maxCount));
}

void ShopScene::ShowMessage(const std::vector<std::string>& lines, Mode returnMode)
{
	msg_ = lines;
	returnMode_ = returnMode;
	mode_ = Mode::MESSAGE;
}

void ShopScene::RefreshSellList(void)
{
	// バッグの中身をそのまま並べる（売れないものは灰色で見せる）
	sellList_.clear();
	for (const auto& pair : inventory_->GetAll())
	{
		if (pair.second > 0 && items_->GetItem(pair.first) != nullptr)
		{
			sellList_.push_back(pair.first);
		}
	}
}

void ShopScene::Close(void)
{
	// PopScene では Release が呼ばれないので、ここで先に解放しておく
	Release();
	SceneManager::GetInstance()->PopScene();
}

int ShopScene::GetSellPrice(int itemId) const
{
	const ItemMasterData* item = items_->GetItem(itemId);
	if (item == nullptr || item->category == ItemCategory::KEY)
	{
		return 0;
	}
	return item->price / 2;		// 売値は買値の半分（買値1のものは0＝売れない）
}

int ShopScene::GetMaxBuyCount(int itemId) const
{
	const ItemMasterData* item = items_->GetItem(itemId);
	if (item == nullptr || item->price <= 0)
	{
		return 0;
	}
	const int byMoney = *money_ / item->price;
	const int bySpace = Inventory::COUNT_MAX - inventory_->GetCount(itemId);
	return std::max(0, std::min(byMoney, bySpace));
}

// =============================================================
// 描画
// =============================================================

void ShopScene::DrawScreen(Mode mode)
{
	DrawWindow(SX(20), SY(20), SX(620), SY(460));

	const bool selling = (mode == Mode::SELL_LIST || mode == Mode::SELL_COUNT);
	DrawFormatString(SX(36), SY(30), BLACK, selling ? "ショップ（うる）" : "ショップ");
	DrawFormatString(SX(440), SY(30), BLACK, "おかね %d 円", (money_ != nullptr) ? *money_ : 0);

	switch (mode)
	{
	case Mode::MENU:
		DrawMenu();
		break;
	case Mode::BUY_LIST:
		DrawItemList(stock_, buyCursor_, false);
		break;
	case Mode::BUY_COUNT:
		DrawItemList(stock_, buyCursor_, false);
		DrawCountWindow(false);
		break;
	case Mode::SELL_LIST:
		DrawItemList(sellList_, sellCursor_, true);
		break;
	case Mode::SELL_COUNT:
		DrawItemList(sellList_, sellCursor_, true);
		DrawCountWindow(true);
		break;
	case Mode::MESSAGE:
		break;
	}
}

void ShopScene::DrawWindow(int x1, int y1, int x2, int y2)
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
	DrawBox(x1, y1, x2, y2, WHITE, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawBox(x1, y1, x2, y2, BLACK, false);
}

void ShopScene::DrawMenu(void)
{
	// 左に「かう／うる／やめる」
	DrawWindow(SX(36), SY(70), SX(220), SY(70 + 20 + MENU_COUNT * 38));
	for (int i = 0; i < MENU_COUNT; i++)
	{
		const int y = SY(84 + i * 38);
		if (i == menuCursor_)
		{
			DrawFormatString(SX(50), y, ORANGE, ">");
		}
		DrawFormatString(SX(76), y, BLACK, "%s", MENU_LABELS[i]);
	}

	// 下にお店の人のことば（メッセージ表示中は、あとで MESSAGE 側が描く）
	if (mode_ != Mode::MESSAGE)
	{
		DrawMessage();
	}
}

void ShopScene::DrawItemList(const std::vector<int>& list, int cursor, bool sell)
{
	if (list.empty())
	{
		DrawFormatString(SX(60), SY(80), GRAY, "なにも ありません");
		return;
	}

	// 一覧（カーソルが画面外に出ないようにスクロールさせる）
	const int total = static_cast<int>(list.size());
	int top = cursor - LIST_VISIBLE / 2;
	top = std::max(0, std::min(top, total - LIST_VISIBLE));

	for (int row = 0; row < LIST_VISIBLE && top + row < total; row++)
	{
		const int index = top + row;
		const int id = list[index];
		const ItemMasterData* item = items_->GetItem(id);
		if (item == nullptr)
		{
			continue;
		}

		const int y = SY(62 + row * LIST_ROW_H);

		// 買えない（お金が足りない）／売れないものは灰色
		const bool enabled = sell ? (GetSellPrice(id) > 0) : (*money_ >= item->price);
		const unsigned int color = enabled ? BLACK : GRAY;

		if (index == cursor)
		{
			DrawFormatString(SX(40), y, ORANGE, ">");
		}
		DrawFormatString(SX(64), y, color, "%s", item->name.c_str());
		DrawFormatString(SX(330), y, color, "もっている %2d", inventory_->GetCount(id));

		if (sell)
		{
			const int price = GetSellPrice(id);
			if (price > 0)
			{
				DrawFormatString(SX(500), y, color, "%6d 円", price);
			}
			else
			{
				DrawFormatString(SX(500), y, GRAY, "うれない");
			}
		}
		else
		{
			DrawFormatString(SX(500), y, color, "%6d 円", item->price);
		}
	}

	if (top > 0)
	{
		DrawFormatString(SX(590), SY(56), GRAY, "▲");
	}
	if (top + LIST_VISIBLE < total)
	{
		DrawFormatString(SX(590), SY(62 + LIST_VISIBLE * LIST_ROW_H - 20), GRAY, "▼");
	}

	DrawDescription(list[cursor]);
}

void ShopScene::DrawCountWindow(bool sell)
{
	const ItemMasterData* item = items_->GetItem(selectedItemId_);
	if (item == nullptr)
	{
		return;
	}

	const int unit = sell ? GetSellPrice(selectedItemId_) : item->price;

	DrawWindow(SX(300), SY(170), SX(600), SY(290));
	DrawFormatString(SX(316), SY(182), BLACK, "%s", item->name.c_str());
	DrawFormatString(SX(316), SY(214), BLACK, "%s   x %d", sell ? "うる" : "かう", count_);
	DrawFormatString(SX(316), SY(246), BLACK, "ごうけい %d 円", unit * count_);
	DrawFormatString(SX(316), SY(268), GRAY, "上下:1  左右:10  Enter:けってい");
}

void ShopScene::DrawDescription(int itemId)
{
	const ItemMasterData* item = items_->GetItem(itemId);
	DrawBox(SX(36), SY(350), SX(604), SY(440), BLACK, false);
	if (item != nullptr)
	{
		DrawFormatString(SX(48), SY(362), BLACK, "%s", item->description.c_str());
	}
}

void ShopScene::DrawMessage(void)
{
	DrawWindow(SX(20), SY(360), SX(620), SY(450));

	for (size_t i = 0; i < msg_.size(); i++)
	{
		DrawFormatString(SX(40), SY(376) + static_cast<int>(i) * SY(28), BLACK, "%s", msg_[i].c_str());
	}
}

int ShopScene::SX(int x) const
{
	return x * Application::SCREEN_SIZE_X / BASE_W;
}

int ShopScene::SY(int y) const
{
	return y * Application::SCREEN_SIZE_Y / BASE_H;
}