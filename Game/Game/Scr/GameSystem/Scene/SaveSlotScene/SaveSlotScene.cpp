#include "SaveSlotScene.h"

#include <algorithm>
#include <DxLib.h>
#include "../SceneManager.h"
#include "../../../AppSystem/InputManager/InputManager.h"

namespace
{
	const int ROW_HEIGHT = 72;
	const int ROW_TOP = 70;
	const int MARGIN_X = 40;

	bool Trg(int key)
	{
		return InputManager::GetInstance()->IsTrgDown(key);
	}

	bool IsDecideTrg(void)
	{
		return Trg(KEY_INPUT_RETURN) || Trg(KEY_INPUT_Z);
	}
}

SaveSlotScene::SaveSlotScene(void)
{
}

SaveSlotScene::~SaveSlotScene(void)
{
}

void SaveSlotScene::Setup(Mode mode, DecideFunc onDecide, int currentSlot)
{
	mode_ = mode;
	onDecide_ = std::move(onDecide);
	startSlot_ = currentSlot;

	// Init より先に Draw/Update が呼ばれても落ちないよう、ここでも情報を読んでおく
	Refresh();
	cursor_ = SaveSystem::IsValidSlot(startSlot_) ? startSlot_ : 0;
	top_ = (cursor_ / VISIBLE_ROWS) * VISIBLE_ROWS;
}

void SaveSlotScene::Init(void)
{
	Refresh();

	state_ = State::SELECT;
	messageTimer_ = 0;
	cursor_ = SaveSystem::IsValidSlot(startSlot_) ? startSlot_ : 0;
	top_ = (cursor_ / VISIBLE_ROWS) * VISIBLE_ROWS;
}

void SaveSlotScene::Load(void)
{
}

void SaveSlotScene::LoadEnd(void)
{
}

void SaveSlotScene::Release(void)
{
}

void SaveSlotScene::Refresh(void)
{
	infos_.assign(SaveSystem::SLOT_COUNT, SaveSystem::SlotInfo());
	for (int i = 0; i < SaveSystem::SLOT_COUNT; i++)
	{
		SaveSystem::ReadInfo(i, infos_[i]);
	}
}

void SaveSlotScene::MoveCursor(int delta)
{
	const int n = SaveSystem::SLOT_COUNT;
	cursor_ = ((cursor_ + delta) % n + n) % n;

	// カーソルが画面の外に出たら、見えるところまでずらす
	if (cursor_ < top_)
	{
		top_ = cursor_;
	}
	else if (cursor_ >= top_ + VISIBLE_ROWS)
	{
		top_ = cursor_ - VISIBLE_ROWS + 1;
	}
	top_ = std::max(0, std::min(top_, n - VISIBLE_ROWS));
}

void SaveSlotScene::Decide(int slot)
{
	// PopScene すると this が消えるかもしれないので、先にコピーしておく
	const DecideFunc func = onDecide_;
	SceneManager::GetInstance()->PopScene();
	if (func)
	{
		func(slot);
	}
}

void SaveSlotScene::Update(void)
{
	if (infos_.empty())
	{
		return;
	}

	if (messageTimer_ > 0)
	{
		messageTimer_--;
	}

	if (state_ == State::SELECT)
	{
		UpdateSelect();
	}
	else
	{
		UpdateConfirm();
	}
}

void SaveSlotScene::UpdateSelect(void)
{
	if (Trg(KEY_INPUT_ESCAPE))
	{
		SceneManager::GetInstance()->PopScene();
		return;
	}

	if (Trg(KEY_INPUT_UP)) { MoveCursor(-1); }
	if (Trg(KEY_INPUT_DOWN)) { MoveCursor(1); }
	if (Trg(KEY_INPUT_LEFT)) { MoveCursor(-VISIBLE_ROWS); }		// 前のページ
	if (Trg(KEY_INPUT_RIGHT)) { MoveCursor(VISIBLE_ROWS); }		// 次のページ

	const SaveSystem::SlotInfo& info = infos_[cursor_];

	if (Trg(KEY_INPUT_DELETE) && info.exists)
	{
		state_ = State::CONFIRM_DELETE;
		confirmYes_ = false;		// 誤操作を防ぐため、最初は「いいえ」
		return;
	}

	if (IsDecideTrg())
	{
		if (mode_ == Mode::LOAD)
		{
			if (!info.exists)
			{
				message_ = "このスロットには セーブデータがありません";
				messageTimer_ = 90;
				return;
			}
			Decide(cursor_);
			return;
		}

		// SAVE
		if (info.exists)
		{
			state_ = State::CONFIRM_OVERWRITE;
			confirmYes_ = false;
			return;
		}
		Decide(cursor_);
	}
}

void SaveSlotScene::UpdateConfirm(void)
{
	if (Trg(KEY_INPUT_LEFT) || Trg(KEY_INPUT_RIGHT) || Trg(KEY_INPUT_UP) || Trg(KEY_INPUT_DOWN))
	{
		confirmYes_ = !confirmYes_;
	}

	if (Trg(KEY_INPUT_ESCAPE))
	{
		state_ = State::SELECT;
		return;
	}

	if (!IsDecideTrg())
	{
		return;
	}

	if (!confirmYes_)
	{
		state_ = State::SELECT;
		return;
	}

	if (state_ == State::CONFIRM_OVERWRITE)
	{
		Decide(cursor_);
		return;
	}

	// 削除
	if (SaveSystem::Delete(cursor_))
	{
		message_ = "削除しました";
	}
	else
	{
		message_ = "削除できませんでした";
	}
	messageTimer_ = 90;
	infos_[cursor_] = SaveSystem::SlotInfo();
	state_ = State::SELECT;
}

void SaveSlotScene::Draw(void)
{
	if (infos_.empty())
	{
		return;
	}

	int sw = 0;
	int sh = 0;
	GetDrawScreenSize(&sw, &sh);

	const int white = GetColor(255, 255, 255);
	const int gray = GetColor(150, 150, 150);
	const int yellow = GetColor(255, 230, 120);

	// 背景
	DrawBox(0, 0, sw, sh, GetColor(20, 24, 40), true);

	// タイトル
	const char* title = (mode_ == Mode::LOAD) ? "つづきから　（セーブデータを えらぶ）" : "セーブ　（ほぞんさきを えらぶ）";
	DrawFormatString(MARGIN_X, 24, white, "%s", title);

	const int pageCount = (SaveSystem::SLOT_COUNT + VISIBLE_ROWS - 1) / VISIBLE_ROWS;
	const int page = top_ / VISIBLE_ROWS + 1;
	DrawFormatString(sw - MARGIN_X - 140, 24, gray, "%d / %d ページ", page, pageCount);

	// スロット一覧
	for (int row = 0; row < VISIBLE_ROWS; row++)
	{
		const int slot = top_ + row;
		if (slot >= SaveSystem::SLOT_COUNT)
		{
			break;
		}

		const SaveSystem::SlotInfo& info = infos_[slot];
		const int y1 = ROW_TOP + row * ROW_HEIGHT;
		const int y2 = y1 + ROW_HEIGHT - 8;
		const bool selected = (slot == cursor_);

		DrawBox(MARGIN_X, y1, sw - MARGIN_X, y2, selected ? GetColor(60, 70, 120) : GetColor(35, 40, 65), true);
		DrawBox(MARGIN_X, y1, sw - MARGIN_X, y2, selected ? yellow : gray, false);

		DrawFormatString(MARGIN_X + 16, y1 + 10, selected ? yellow : white, "スロット %03d", slot + 1);

		if (!info.exists)
		{
			DrawFormatString(MARGIN_X + 16, y1 + 36, gray, "― あき ―");
			continue;
		}

		const int h = info.playSeconds / 3600;
		const int m = (info.playSeconds / 60) % 60;
		const int s = info.playSeconds % 60;
		DrawFormatString(MARGIN_X + 180, y1 + 10, white, "%s", info.savedAt.c_str());
		DrawFormatString(MARGIN_X + 16, y1 + 36, white,
			"プレイ %d:%02d:%02d　　ステージ %d　　てもち %d匹　　おかね %d",
			h, m, s, info.stageId, info.partyCount, info.money);
	}

	// 操作説明・メッセージ
	DrawFormatString(MARGIN_X, sh - 60, gray, "↑↓:えらぶ　←→:ページ　Enter/Z:けってい　Delete:さくじょ　Esc:もどる");
	if (messageTimer_ > 0)
	{
		DrawFormatString(MARGIN_X, sh - 36, yellow, "%s", message_);
	}

	// 確認ダイアログ
	if (state_ != State::SELECT)
	{
		const int bx1 = sw / 2 - 200;
		const int bx2 = sw / 2 + 200;
		const int by1 = sh / 2 - 70;
		const int by2 = sh / 2 + 70;
		DrawBox(bx1, by1, bx2, by2, GetColor(0, 0, 0), true);
		DrawBox(bx1, by1, bx2, by2, white, false);

		const char* text = (state_ == State::CONFIRM_OVERWRITE)
			? "このスロットに うわがきしますか？"
			: "このセーブデータを さくじょしますか？";
		DrawFormatString(bx1 + 24, by1 + 28, white, "%s", text);
		DrawFormatString(bx1 + 90, by1 + 90, confirmYes_ ? yellow : gray, "%s はい", confirmYes_ ? "▶" : "　");
		DrawFormatString(bx1 + 230, by1 + 90, confirmYes_ ? gray : yellow, "%s いいえ", confirmYes_ ? "　" : "▶");
	}
}