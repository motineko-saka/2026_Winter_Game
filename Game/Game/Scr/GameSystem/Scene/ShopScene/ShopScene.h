#pragma once

#include <string>
#include <vector>
#include "../SceneBase.h"

class ItemData;
class Inventory;

// ショップ画面（アイテムの購入と売却）
//   かう／うる／やめる
//
// 使い方（フィールド側）：
//   auto shop = std::make_shared<ShopScene>();
//   shop->Setup(&itemData, &inventory, &money, 品ぞろえのアイテムID一覧);
//   SceneManager::GetInstance()->PushScene(shop);
//
// ・所持金（money）はこの画面の中で直接増減する（ポインタで渡す）
// ・売値は買値の半分。買値0のもの・大事なもの（KEY）は売れない
class ShopScene : public SceneBase
{
public:

	ShopScene(void);				// コンストラクタ
	~ShopScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の初期化
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

public:

	// ショップで使うデータを渡す（PushScene の前に呼ぶ）
	//   stock … 売っているアイテムID（Item.csv に無いもの・買値0のものは自動で外す）
	void Setup(const ItemData* items, Inventory* inventory, int* money,
		const std::vector<int>& stock);

private:

	// 今どの画面か
	enum class Mode
	{
		MENU,			// かう／うる／やめる
		BUY_LIST,		// 買うものを選ぶ
		BUY_COUNT,		// 買う個数を決める
		SELL_LIST,		// 売るものを選ぶ
		SELL_COUNT,		// 売る個数を決める
		MESSAGE,		// メッセージ表示（Enterで元の画面へ）
	};

private:

	// ---------- 更新 ----------
	void UpdateMenu(void);
	void UpdateBuyList(void);
	void UpdateBuyCount(void);
	void UpdateSellList(void);
	void UpdateSellCount(void);
	void UpdateMessage(void);

	void MoveCursor(int& cursor, int count);						// 上下キーでカーソルを動かす（ループする）
	void ChangeCount(int maxCount);									// 左右・上下キーで個数を変える（1〜maxCount）
	void ShowMessage(const std::vector<std::string>& lines, Mode returnMode);
	void RefreshSellList(void);										// 売れる一覧（バッグの中身）を作り直す
	void Close(void);												// 画面を閉じる

	int GetSellPrice(int itemId) const;								// 売値（0なら売れない）
	int GetMaxBuyCount(int itemId) const;							// 今買える最大個数（0なら買えない）

	// ---------- 描画 ----------
	void DrawScreen(Mode mode);
	void DrawWindow(int x1, int y1, int x2, int y2);
	void DrawMenu(void);
	void DrawItemList(const std::vector<int>& list, int cursor, bool sell);
	void DrawCountWindow(bool sell);
	void DrawDescription(int itemId);
	void DrawMessage(void);

	// ---------- 補助 ----------
	int SX(int x) const;											// 640x480基準のX座標を実画面に合わせる
	int SY(int y) const;											// 640x480基準のY座標を実画面に合わせる

private:

	static const int BASE_W = 640;
	static const int BASE_H = 480;

	static const int MONEY_MAX = 999999;							// 所持金の上限

	// 外部から渡されるもの
	const ItemData* items_ = nullptr;
	Inventory* inventory_ = nullptr;
	int* money_ = nullptr;
	std::vector<int> stock_;										// 売っているアイテムID

	Mode mode_ = Mode::MENU;
	Mode returnMode_ = Mode::MENU;									// メッセージを閉じたあとに戻る画面

	int menuCursor_ = 0;
	int buyCursor_ = 0;
	int sellCursor_ = 0;
	int count_ = 1;													// 買う／売る個数
	int selectedItemId_ = 0;										// 個数を決めているアイテム

	std::vector<int> sellList_;										// 売り物（バッグの中身）のアイテムID
	std::vector<std::string> msg_;									// 表示中のメッセージ
};