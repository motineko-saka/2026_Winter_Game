#pragma once

#include <functional>
#include <vector>
#include "../SceneBase.h"
#include "SaveSystem.h"

// セーブデータを選ぶ画面（ロード用・セーブ用を兼ねる）
//
// 使い方（タイトルの「つづきから」）：
//   auto scene = std::make_shared<SaveSlotScene>();
//   scene->Setup(SaveSlotScene::Mode::LOAD, [](int slot)
//       {
//           auto game = std::make_shared<GameScene>();
//           game->SetLoadSlot(slot);
//           SceneManager::GetInstance()->ChangeScene(game);
//       });
//   SceneManager::GetInstance()->PushScene(scene);
//
// 使い方（ゲーム中に保存先を選ぶ）：
//   scene->Setup(SaveSlotScene::Mode::SAVE, [this](int slot) { SaveGameTo(slot); currentSlot_ = slot; }, currentSlot_);
//
// 決定すると、この画面を PopScene してから onDecide を呼ぶ（onDecide の中で this を触らないこと）
//
// 操作：↑↓ で選ぶ / ←→ でページ送り / Enter か Z で決定 / Delete で削除 / Esc で戻る
class SaveSlotScene : public SceneBase
{
public:

	enum class Mode
	{
		LOAD,		// セーブ済みのスロットだけ選べる
		SAVE,		// 空きスロットにも選べる（セーブ済みなら上書きの確認が出る）
	};

	using DecideFunc = std::function<void(int slot)>;

	SaveSlotScene(void);
	~SaveSlotScene(void) override;

public:

	void Init(void)		override;
	void Load(void)		override;
	void LoadEnd(void)	override;
	void Update(void)	override;
	void Draw(void)		override;
	void Release(void)	override;

public:

	// PushScene の前に呼ぶ。currentSlot を渡すと、最初にそのスロットにカーソルが合う（-1なら先頭）
	void Setup(Mode mode, DecideFunc onDecide, int currentSlot = -1);

private:

	enum class State
	{
		SELECT,				// スロットを選んでいる
		CONFIRM_OVERWRITE,	// 「上書きしますか？」
		CONFIRM_DELETE,		// 「削除しますか？」
	};

	static const int VISIBLE_ROWS = 6;		// 1画面に出すスロット数

	void Refresh(void);						// 全スロットの情報を読み直す
	void Decide(int slot);					// 決定（画面を閉じて onDecide を呼ぶ）
	void MoveCursor(int delta);				// カーソルを動かす（端でループ）
	void UpdateSelect(void);
	void UpdateConfirm(void);

	Mode mode_ = Mode::LOAD;
	DecideFunc onDecide_;
	int startSlot_ = -1;

	std::vector<SaveSystem::SlotInfo> infos_;	// 全スロットの情報
	int cursor_ = 0;
	int top_ = 0;								// 画面の一番上に出しているスロット
	State state_ = State::SELECT;
	bool confirmYes_ = false;					// 確認画面で「はい」にカーソルがあるか
	int messageTimer_ = 0;						// 下に出す一言メッセージの残りフレーム数
	const char* message_ = "";
};