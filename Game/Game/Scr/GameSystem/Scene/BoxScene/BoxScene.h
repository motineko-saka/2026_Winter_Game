#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "../SceneBase.h"

class MonsterData;
class MonsterParty;
class MonsterBox;
struct MonsterInstance;

// ボックス画面（モンスターの預かりシステム。ポケモンでいうPC）
//   左：てもち　右：ボックス（Q / E でボックスを切り替え）
//   あずける／てもちに くわえる／いれかえる／にがす
//
// 使い方（フィールド側）：
//   auto box = std::make_shared<BoxScene>();
//   box->Setup(&monsterData, &monsterParty, &monsterBox);
//   SceneManager::GetInstance()->PushScene(box);
//
// 操作：
//   ↑↓←→ / WASD … カーソル移動（ボックスの左端で←を押すと、てもちへ）
//   Q / E        … ボックスの切り替え
//   Enter/Space  … 決定　　ESC/BackSpace … 戻る・閉じる
class BoxScene : public SceneBase
{
public:

	BoxScene(void);					// コンストラクタ
	~BoxScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の初期化
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

public:

	// ボックスで使うデータを渡す（PushScene の前に呼ぶ）
	void Setup(const MonsterData* data, MonsterParty* party, MonsterBox* box);

private:

	// 今どの画面か
	enum class Mode
	{
		SELECT,			// カーソルでモンスターを選ぶ
		ACTION,			// 選んだモンスターへの操作を選ぶ
		SWAP_SELECT,	// いれかえる：てもちの相手を選ぶ
		CONFIRM_RELEASE,// にがす：本当に逃がすか確認
		MESSAGE,		// メッセージ表示（Enterで元の画面へ）
	};

	// カーソルがあるのはどちらか
	enum class Panel
	{
		PARTY,
		BOX,
	};

	// 操作の種類
	enum class ActionId
	{
		DEPOSIT,		// あずける（てもち → ボックス）
		WITHDRAW,		// てもちに くわえる（ボックス → てもち）
		SWAP,			// いれかえる
		RELEASE,		// にがす
		BACK,			// もどる
	};

private:

	// ---------- 更新 ----------
	void UpdateSelect(void);
	void UpdateAction(void);
	void UpdateSwapSelect(void);
	void UpdateConfirmRelease(void);
	void UpdateMessage(void);

	void OpenAction(void);												// 今のカーソルのモンスターに使える操作を並べて、選ばせる
	void DoDeposit(void);
	void DoWithdraw(void);
	void DoSwap(int partyIndex);
	void DoRelease(void);

	void MoveInBox(void);												// ボックスの中のカーソル移動
	void ShowMessage(const std::vector<std::string>& lines, Mode returnMode);
	void ClampCursors(void);											// 手持ちの数が変わったあとにカーソルを直す
	bool HasOtherAlive(int exceptPartyIndex) const;						// 指定の1匹以外に、戦えるモンスターが手持ちにいるか
	void Close(void);													// 画面を閉じる

	const MonsterInstance* GetCursorMonster(void) const;				// 今カーソルがある場所のモンスター（なければnullptr）

	// ---------- 描画 ----------
	void DrawWindow(int x1, int y1, int x2, int y2);
	void DrawPartyPanel(void);
	void DrawBoxPanel(void);
	void DrawInfoPanel(void);
	void DrawActionWindow(void);
	void DrawConfirmWindow(void);
	void DrawMessage(void);
	void DrawHpBar(int x, int y, int w, int h, float ratio);

	// ---------- 補助 ----------
	int GetImage(const std::string& path);								// 画像を読み込む（同じパスはキャッシュを返す）
	void DrawImageFit(int img, int x, int y, int size);					// 正方形の枠に収めて描く
	std::string NameOf(const MonsterInstance& mon) const;				// 表示名（ニックネーム）
	int SX(int x) const;												// 640x480基準のX座標を実画面に合わせる
	int SY(int y) const;												// 640x480基準のY座標を実画面に合わせる

private:

	static const int BASE_W = 640;
	static const int BASE_H = 480;

	static const int GRID_COLS = 6;										// ボックスの列数（30匹 = 6列 x 5行）
	static const int GRID_ROWS = 5;

	// 外部から渡されるもの
	const MonsterData* data_ = nullptr;
	MonsterParty* party_ = nullptr;
	MonsterBox* box_ = nullptr;

	Mode mode_ = Mode::SELECT;
	Mode returnMode_ = Mode::SELECT;									// メッセージを閉じたあとに戻る画面
	Panel panel_ = Panel::PARTY;

	int partyCursor_ = 0;
	int boxIndex_ = 0;													// 開いているボックス
	int boxCursor_ = 0;													// ボックス内のスロット（0〜29）
	int actionCursor_ = 0;
	int swapCursor_ = 0;												// いれかえる相手（てもちの番号）
	int confirmCursor_ = 1;												// 0:はい 1:いいえ

	std::vector<ActionId> actions_;										// 今選べる操作
	std::vector<std::string> msg_;										// 表示中のメッセージ

	std::unordered_map<std::string, int> images_;						// パス→画像ハンドルのキャッシュ
};