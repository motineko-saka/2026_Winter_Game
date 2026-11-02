#include "BoxScene.h"

#include <algorithm>
#include <DxLib.h>

#include "../../../AppSystem/Application/Application.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Monster/MonsterData.h"
#include "../../Object/Monster/MonsterParty.h"
#include "../../Object/Monster/Monsterbox.h"
#include "../SceneManager.h"

namespace
{
	// 操作メニューの表示名（BoxScene::ActionId の順）
	const char* const ACTION_LABELS[] =
	{
		"あずける", "てもちに くわえる", "いれかえる", "にがす", "もどる",
	};

	// 色（0xRRGGBB）
	const unsigned int BLACK = 0x000000;
	const unsigned int WHITE = 0xFFFFFF;
	const unsigned int GRAY = 0x999999;
	const unsigned int RED = 0xCC2222;
	const unsigned int ORANGE = 0xFF8800;

	const int PARTY_ROW_H = 44;		// てもち1匹分の高さ
	const int CELL_SIZE = 52;		// ボックスの1マスの大きさ

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
}

BoxScene::BoxScene(void)
{
}

BoxScene::~BoxScene(void)
{
}

void BoxScene::Setup(const MonsterData* data, MonsterParty* party, MonsterBox* box)
{
	data_ = data;
	party_ = party;
	box_ = box;
}

void BoxScene::Init(void)
{
	// ボックスの1画面（列 x 行）が、1ボックスの収容数と合っているか
	static_assert(GRID_COLS * GRID_ROWS == MonsterBox::SLOT_COUNT,
		"ボックスの列数x行数が SLOT_COUNT と合っていません");

	mode_ = Mode::SELECT;
	returnMode_ = Mode::SELECT;
	panel_ = Panel::PARTY;
	partyCursor_ = 0;
	boxIndex_ = 0;
	boxCursor_ = 0;
	actionCursor_ = 0;
	swapCursor_ = 0;
	confirmCursor_ = 1;
	actions_.clear();
	msg_.clear();
}

void BoxScene::Load(void)
{
}

void BoxScene::LoadEnd(void)
{
}

void BoxScene::Update(void)
{
	// 必要なデータが渡されていなければ、何もせず閉じる
	if (data_ == nullptr || party_ == nullptr || box_ == nullptr)
	{
		Close();
		return;
	}

	switch (mode_)
	{
	case Mode::SELECT:			UpdateSelect();			break;
	case Mode::ACTION:			UpdateAction();			break;
	case Mode::SWAP_SELECT:		UpdateSwapSelect();		break;
	case Mode::CONFIRM_RELEASE:	UpdateConfirmRelease();	break;
	case Mode::MESSAGE:			UpdateMessage();		break;
	}
}

void BoxScene::Draw(void)
{
	// 後ろのゲーム画面を暗くする
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 127);
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, BLACK, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

	if (data_ == nullptr || party_ == nullptr || box_ == nullptr)
	{
		return;
	}

	DrawWindow(SX(20), SY(20), SX(620), SY(460));
	DrawFormatString(SX(36), SY(28), BLACK, "ボックス");

	DrawPartyPanel();
	DrawBoxPanel();
	DrawInfoPanel();

	switch (mode_)
	{
	case Mode::ACTION:				DrawActionWindow();		break;
	case Mode::CONFIRM_RELEASE:		DrawConfirmWindow();	break;
	case Mode::MESSAGE:				DrawMessage();			break;
	default:												break;
	}
}

void BoxScene::Release(void)
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

void BoxScene::UpdateSelect(void)
{
	if (IsCancel())
	{
		Close();
		return;
	}

	// ボックスの切り替え
	if (Trg(KEY_INPUT_Q))
	{
		boxIndex_ = (boxIndex_ + MonsterBox::BOX_COUNT - 1) % MonsterBox::BOX_COUNT;
	}
	else if (Trg(KEY_INPUT_E))
	{
		boxIndex_ = (boxIndex_ + 1) % MonsterBox::BOX_COUNT;
	}

	if (panel_ == Panel::PARTY)
	{
		const int count = party_->GetCount();
		if (count > 0)
		{
			if (IsUp())
			{
				partyCursor_ = (partyCursor_ + count - 1) % count;
			}
			else if (IsDown())
			{
				partyCursor_ = (partyCursor_ + 1) % count;
			}
		}

		if (IsRight())
		{
			panel_ = Panel::BOX;
		}
		else if (IsDecide() && party_->Get(partyCursor_) != nullptr)
		{
			OpenAction();
		}
	}
	else
	{
		MoveInBox();

		if (panel_ == Panel::BOX && IsDecide() && box_->Get(boxIndex_, boxCursor_) != nullptr)
		{
			OpenAction();
		}
	}
}

void BoxScene::MoveInBox(void)
{
	int col = boxCursor_ % GRID_COLS;
	int row = boxCursor_ / GRID_COLS;

	if (IsUp())
	{
		row = (row + GRID_ROWS - 1) % GRID_ROWS;
	}
	else if (IsDown())
	{
		row = (row + 1) % GRID_ROWS;
	}
	else if (IsLeft())
	{
		if (col == 0)
		{
			// 左端でさらに左：てもちへ
			panel_ = Panel::PARTY;
			ClampCursors();
			return;
		}
		col--;
	}
	else if (IsRight())
	{
		col = (col + 1) % GRID_COLS;
	}

	boxCursor_ = row * GRID_COLS + col;
}

void BoxScene::OpenAction(void)
{
	actions_.clear();
	if (panel_ == Panel::PARTY)
	{
		actions_.push_back(ActionId::DEPOSIT);
	}
	else
	{
		actions_.push_back(ActionId::WITHDRAW);
		actions_.push_back(ActionId::SWAP);
		actions_.push_back(ActionId::RELEASE);
	}
	actions_.push_back(ActionId::BACK);

	actionCursor_ = 0;
	mode_ = Mode::ACTION;
}

void BoxScene::UpdateAction(void)
{
	if (IsCancel())
	{
		mode_ = Mode::SELECT;
		return;
	}

	const int count = static_cast<int>(actions_.size());
	if (IsUp())
	{
		actionCursor_ = (actionCursor_ + count - 1) % count;
	}
	else if (IsDown())
	{
		actionCursor_ = (actionCursor_ + 1) % count;
	}

	if (!IsDecide())
	{
		return;
	}

	switch (actions_[actionCursor_])
	{
	case ActionId::DEPOSIT:
		DoDeposit();
		break;

	case ActionId::WITHDRAW:
		DoWithdraw();
		break;

	case ActionId::SWAP:
		if (party_->GetCount() == 0)
		{
			ShowMessage({ "いれかえる てもちの モンスターが いない！" }, Mode::SELECT);
		}
		else
		{
			swapCursor_ = std::min(partyCursor_, party_->GetCount() - 1);
			mode_ = Mode::SWAP_SELECT;
		}
		break;

	case ActionId::RELEASE:
		confirmCursor_ = 1;		// 最初は「いいえ」
		mode_ = Mode::CONFIRM_RELEASE;
		break;

	case ActionId::BACK:
		mode_ = Mode::SELECT;
		break;
	}
}

void BoxScene::UpdateSwapSelect(void)
{
	if (IsCancel())
	{
		mode_ = Mode::SELECT;
		return;
	}

	const int count = party_->GetCount();
	if (count <= 0)
	{
		mode_ = Mode::SELECT;
		return;
	}

	if (IsUp())
	{
		swapCursor_ = (swapCursor_ + count - 1) % count;
	}
	else if (IsDown())
	{
		swapCursor_ = (swapCursor_ + 1) % count;
	}

	if (IsDecide())
	{
		DoSwap(swapCursor_);
	}
}

void BoxScene::UpdateConfirmRelease(void)
{
	if (IsCancel())
	{
		mode_ = Mode::SELECT;
		return;
	}

	if (IsLeft() || IsRight() || IsUp() || IsDown())
	{
		confirmCursor_ = 1 - confirmCursor_;
	}

	if (!IsDecide())
	{
		return;
	}

	if (confirmCursor_ == 0)
	{
		DoRelease();
	}
	else
	{
		mode_ = Mode::SELECT;
	}
}

void BoxScene::UpdateMessage(void)
{
	if (IsDecide() || IsCancel())
	{
		mode_ = returnMode_;
	}
}

// -------------------------------------------------------------
// 操作の実行（モンスターを動かしたあとはポインタが無効になるので、名前は先に控えておく）
// -------------------------------------------------------------

void BoxScene::DoDeposit(void)
{
	const MonsterInstance* mon = party_->Get(partyCursor_);
	if (mon == nullptr)
	{
		mode_ = Mode::SELECT;
		return;
	}
	const std::string name = NameOf(*mon);

	if (!HasOtherAlive(partyCursor_))
	{
		ShowMessage({ "そのモンスターを あずけると",
			"たたかえる モンスターが いなくなる！" }, Mode::SELECT);
		return;
	}

	// 開いているボックスに空きがあればそこへ、満杯なら空きのあるボックスへ
	int destBox = boxIndex_;
	if (box_->GetCount(boxIndex_) >= MonsterBox::SLOT_COUNT)
	{
		destBox = box_->FindEmptyBox();
	}
	if (destBox < 0)
	{
		ShowMessage({ "ボックスが すべて いっぱいだ！" }, Mode::SELECT);
		return;
	}

	if (!box_->Deposit(*party_, partyCursor_, destBox))
	{
		ShowMessage({ "あずけることが できなかった！" }, Mode::SELECT);
		return;
	}

	ClampCursors();
	ShowMessage({ name + "を", box_->GetBoxName(destBox) + "に あずけた！" }, Mode::SELECT);
}

void BoxScene::DoWithdraw(void)
{
	const MonsterInstance* mon = box_->Get(boxIndex_, boxCursor_);
	if (mon == nullptr)
	{
		mode_ = Mode::SELECT;
		return;
	}
	const std::string name = NameOf(*mon);

	if (party_->IsFull())
	{
		ShowMessage({ "てもちが いっぱいだ！" }, Mode::SELECT);
		return;
	}

	if (!box_->Withdraw(*party_, boxIndex_, boxCursor_))
	{
		ShowMessage({ "てもちに くわえられなかった！" }, Mode::SELECT);
		return;
	}

	ShowMessage({ name + "を てもちに くわえた！" }, Mode::SELECT);
}

void BoxScene::DoSwap(int partyIndex)
{
	const MonsterInstance* partyMon = party_->Get(partyIndex);
	const MonsterInstance* boxMon = box_->Get(boxIndex_, boxCursor_);
	if (partyMon == nullptr || boxMon == nullptr)
	{
		mode_ = Mode::SELECT;
		return;
	}
	const std::string partyName = NameOf(*partyMon);
	const std::string boxName = NameOf(*boxMon);

	if (!box_->SwapWithParty(*party_, partyIndex, boxIndex_, boxCursor_))
	{
		ShowMessage({ "いれかえると たたかえる モンスターが",
			"いなくなってしまう！" }, Mode::SELECT);
		return;
	}

	ShowMessage({ partyName + "と " + boxName + "を",
		"いれかえた！" }, Mode::SELECT);
}

void BoxScene::DoRelease(void)
{
	const MonsterInstance* mon = box_->Get(boxIndex_, boxCursor_);
	if (mon == nullptr)
	{
		mode_ = Mode::SELECT;
		return;
	}
	const std::string name = NameOf(*mon);

	if (!box_->Remove(boxIndex_, boxCursor_))
	{
		ShowMessage({ "にがすことが できなかった！" }, Mode::SELECT);
		return;
	}

	ShowMessage({ name + "を にがした。", "さようなら、" + name + "！" }, Mode::SELECT);
}

void BoxScene::ShowMessage(const std::vector<std::string>& lines, Mode returnMode)
{
	msg_ = lines;
	returnMode_ = returnMode;
	mode_ = Mode::MESSAGE;
}

void BoxScene::ClampCursors(void)
{
	const int last = std::max(0, party_->GetCount() - 1);
	partyCursor_ = std::max(0, std::min(partyCursor_, last));
	swapCursor_ = std::max(0, std::min(swapCursor_, last));
}

bool BoxScene::HasOtherAlive(int exceptPartyIndex) const
{
	for (int i = 0; i < party_->GetCount(); i++)
	{
		const MonsterInstance* mon = party_->Get(i);
		if (i != exceptPartyIndex && mon != nullptr && mon->currentHp > 0)
		{
			return true;
		}
	}
	return false;
}

void BoxScene::Close(void)
{
	// PopScene では Release が呼ばれないので、ここで先に解放しておく
	// （PopScene のあとは、この画面が消えている可能性があるので何も触らない）
	Release();
	SceneManager::GetInstance()->PopScene();
}

const MonsterInstance* BoxScene::GetCursorMonster(void) const
{
	if (mode_ == Mode::SWAP_SELECT)
	{
		return party_->Get(swapCursor_);
	}
	if (panel_ == Panel::PARTY)
	{
		return party_->Get(partyCursor_);
	}
	return box_->Get(boxIndex_, boxCursor_);
}

// =============================================================
// 描画
// =============================================================

void BoxScene::DrawWindow(int x1, int y1, int x2, int y2)
{
	SetDrawBlendMode(DX_BLENDMODE_ALPHA, 235);
	DrawBox(x1, y1, x2, y2, WHITE, true);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
	DrawBox(x1, y1, x2, y2, BLACK, false);
}

void BoxScene::DrawHpBar(int x, int y, int w, int h, float ratio)
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

void BoxScene::DrawPartyPanel(void)
{
	const int x1 = SX(32);
	const int y1 = SY(52);
	DrawWindow(x1, y1, SX(262), SY(350));
	DrawFormatString(x1 + SX(8), y1 + SY(4), BLACK, "てもち  %d / %d", party_->GetCount(), MonsterParty::PARTY_MAX);

	const bool cursorVisible = (panel_ == Panel::PARTY) || (mode_ == Mode::SWAP_SELECT);
	const int cursor = (mode_ == Mode::SWAP_SELECT) ? swapCursor_ : partyCursor_;

	for (int i = 0; i < MonsterParty::PARTY_MAX; i++)
	{
		const int y = y1 + SY(28 + i * PARTY_ROW_H);
		const MonsterInstance* mon = party_->Get(i);
		if (mon == nullptr)
		{
			DrawFormatString(x1 + SX(40), y + SY(8), GRAY, "---");
			continue;
		}

		if (cursorVisible && i == cursor)
		{
			DrawFormatString(x1 + SX(6), y + SY(8), ORANGE, ">");
		}

		const MonsterMasterData* master = data_->GetMonster(mon->monsterId);
		if (master != nullptr)
		{
			DrawImageFit(GetImage(master->iconPath), x1 + SX(22), y, SY(40));
		}

		const int maxHp = std::max(1, MonsterParty::GetMaxHp(*data_, *mon));
		const int hp = std::max(0, mon->currentHp);

		DrawFormatString(x1 + SX(72), y + SY(2), BLACK, "%s  Lv%d", NameOf(*mon).c_str(), mon->level);
		DrawHpBar(x1 + SX(72), y + SY(24), SX(130), SY(8), static_cast<float>(hp) / static_cast<float>(maxHp));
		if (hp <= 0)
		{
			DrawFormatString(x1 + SX(150), y + SY(2), RED, "ひんし");
		}
	}
}

void BoxScene::DrawBoxPanel(void)
{
	const int x1 = SX(274);
	const int y1 = SY(52);
	DrawWindow(x1, y1, SX(608), SY(350));

	DrawFormatString(x1 + SX(8), y1 + SY(4), BLACK, "< %s >  %d / %d",
		box_->GetBoxName(boxIndex_).c_str(), box_->GetCount(boxIndex_), MonsterBox::SLOT_COUNT);
	DrawFormatString(x1 + SX(250), y1 + SY(4), GRAY, "Q / E");

	const int gridX = x1 + SX(8);
	const int gridY = y1 + SY(30);
	const bool cursorVisible = (panel_ == Panel::BOX) || (mode_ == Mode::SWAP_SELECT);

	for (int i = 0; i < MonsterBox::SLOT_COUNT; i++)
	{
		const int cx = gridX + SX((i % GRID_COLS) * CELL_SIZE);
		const int cy = gridY + SY((i / GRID_COLS) * CELL_SIZE);
		const int cw = SX(CELL_SIZE);
		const int ch = SY(CELL_SIZE);

		DrawBox(cx, cy, cx + cw, cy + ch, 0xCCCCCC, false);

		const MonsterInstance* mon = box_->Get(boxIndex_, i);
		if (mon != nullptr)
		{
			const MonsterMasterData* master = data_->GetMonster(mon->monsterId);
			if (master != nullptr)
			{
				DrawImageFit(GetImage(master->iconPath), cx + SX(4), cy + SY(2), SY(44));
			}

			// ひんしのモンスターは暗くする
			if (mon->currentHp <= 0)
			{
				SetDrawBlendMode(DX_BLENDMODE_ALPHA, 110);
				DrawBox(cx + 1, cy + 1, cx + cw - 1, cy + ch - 1, BLACK, true);
				SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
			}
		}

		if (cursorVisible && i == boxCursor_)
		{
			DrawBox(cx, cy, cx + cw, cy + ch, ORANGE, false);
			DrawBox(cx + 1, cy + 1, cx + cw - 1, cy + ch - 1, ORANGE, false);
		}
	}
}

void BoxScene::DrawInfoPanel(void)
{
	const int x1 = SX(32);
	const int y1 = SY(358);
	DrawWindow(x1, y1, SX(608), SY(448));

	const MonsterInstance* mon = GetCursorMonster();
	if (mon == nullptr)
	{
		DrawFormatString(x1 + SX(12), y1 + SY(10), GRAY, "モンスターを えらんでください");
	}
	else
	{
		const MonsterMasterData* master = data_->GetMonster(mon->monsterId);
		if (master != nullptr)
		{
			DrawImageFit(GetImage(master->iconPath), x1 + SX(10), y1 + SY(8), SY(56));
		}

		const int maxHp = std::max(1, MonsterParty::GetMaxHp(*data_, *mon));
		const int hp = std::max(0, mon->currentHp);

		DrawFormatString(x1 + SX(80), y1 + SY(8), BLACK, "%s   Lv%d", NameOf(*mon).c_str(), mon->level);

		if (master != nullptr)
		{
			if (master->type2 != ElementType::NONE)
			{
				DrawFormatString(x1 + SX(80), y1 + SY(36), BLACK, "タイプ %s / %s", TypeName(master->type1), TypeName(master->type2));
			}
			else
			{
				DrawFormatString(x1 + SX(80), y1 + SY(36), BLACK, "タイプ %s", TypeName(master->type1));
			}
		}

		if (hp <= 0)
		{
			DrawFormatString(x1 + SX(330), y1 + SY(8), RED, "ひんし");
		}
		else
		{
			DrawFormatString(x1 + SX(330), y1 + SY(8), BLACK, "HP %d / %d", hp, maxHp);
		}

		const char* ailment = AilmentName(mon->ailment);
		if (ailment[0] != '\0')
		{
			DrawFormatString(x1 + SX(330), y1 + SY(36), RED, "%s", ailment);
		}
	}

	DrawFormatString(x1 + SX(12), y1 + SY(68), GRAY, "Enter:けってい  ESC:とじる  Q/E:ボックスきりかえ");
}

void BoxScene::DrawActionWindow(void)
{
	const int count = static_cast<int>(actions_.size());
	const int x1 = (panel_ == Panel::PARTY) ? SX(120) : SX(380);
	const int y1 = SY(120);
	DrawWindow(x1, y1, x1 + SX(190), y1 + SY(20 + count * 34));

	for (int i = 0; i < count; i++)
	{
		const int y = y1 + SY(12 + i * 34);
		if (i == actionCursor_)
		{
			DrawFormatString(x1 + SX(10), y, ORANGE, ">");
		}
		DrawFormatString(x1 + SX(32), y, BLACK, "%s", ACTION_LABELS[static_cast<int>(actions_[i])]);
	}
}

void BoxScene::DrawConfirmWindow(void)
{
	const MonsterInstance* mon = box_->Get(boxIndex_, boxCursor_);
	const std::string name = (mon != nullptr) ? NameOf(*mon) : std::string();

	DrawWindow(SX(150), SY(170), SX(490), SY(290));
	DrawFormatString(SX(170), SY(184), BLACK, "%s を にがしますか？", name.c_str());
	DrawFormatString(SX(170), SY(212), RED, "にがすと もとには もどりません。");

	const char* labels[2] = { "はい", "いいえ" };
	for (int i = 0; i < 2; i++)
	{
		const int x = SX(210 + i * 140);
		if (i == confirmCursor_)
		{
			DrawFormatString(x - SX(22), SY(252), ORANGE, ">");
		}
		DrawFormatString(x, SY(252), BLACK, "%s", labels[i]);
	}
}

void BoxScene::DrawMessage(void)
{
	DrawWindow(SX(20), SY(360), SX(620), SY(450));

	for (size_t i = 0; i < msg_.size(); i++)
	{
		DrawFormatString(SX(40), SY(376) + static_cast<int>(i) * SY(28), BLACK, "%s", msg_[i].c_str());
	}
}

// =============================================================
// 補助
// =============================================================

int BoxScene::GetImage(const std::string& path)
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

void BoxScene::DrawImageFit(int img, int x, int y, int size)
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

std::string BoxScene::NameOf(const MonsterInstance& mon) const
{
	if (!mon.nickname.empty())
	{
		return mon.nickname;
	}
	const MonsterMasterData* master = data_->GetMonster(mon.monsterId);
	return (master != nullptr) ? master->name : std::string("???");
}

int BoxScene::SX(int x) const
{
	return x * Application::SCREEN_SIZE_X / BASE_W;
}

int BoxScene::SY(int y) const
{
	return y * Application::SCREEN_SIZE_Y / BASE_H;
}