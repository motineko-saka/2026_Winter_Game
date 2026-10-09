#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "../SceneBase.h"

class MonsterData;
class MonsterParty;
class ItemData;
class Inventory;

// ESCで開くメニュー画面
//   てもち／もちもの／マップ／セーブ／オプション／タイトルへ もどる
//
// 使い方（フィールド側）：
//   auto pause = std::make_shared<PauseScene>();
//   pause->Setup(&monsterData, &monsterParty, &itemData, &inventory, [this]() { return SaveGame(); });
//   SceneManager::GetInstance()->PushScene(pause);
class PauseScene : public SceneBase
{
public:

	PauseScene(void);				// コンストラクタ
	~PauseScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の初期化
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

public:

	// メニューで使うデータを渡す（PushScene の前に呼ぶ）
	// saveFunc … 「セーブ」を選んだときに呼ぶ関数（成功したらtrueを返す）
	void Setup(const MonsterData* data, MonsterParty* party,
		const ItemData* items, Inventory* inventory,
		std::function<bool(void)> saveFunc);

private:

	// 今どの画面か
	enum class Mode
	{
		MENU,			// メインメニュー
		PARTY,			// てもち一覧
		PARTY_ACTION,	// てもち：つよさを みる／いれかえる
		PARTY_DETAIL,	// てもち：つよさ画面
		PARTY_SWAP,		// てもち：入れ替え先を選ぶ
		BAG,			// もちもの一覧
		BAG_TARGET,		// もちもの：使う相手を選ぶ
		OPTION,			// オプション
		CONFIRM_TITLE,	// タイトルへ戻るか確認
		MESSAGE,		// メッセージ表示（Enterで元の画面へ）
	};

private:

	// ---------- 更新 ----------
	void UpdateMenu(void);
	void UpdateParty(void);
	void UpdatePartyAction(void);
	void UpdatePartyDetail(void);
	void UpdatePartySwap(void);
	void UpdateBag(void);
	void UpdateBagTarget(void);
	void UpdateOption(void);
	void UpdateConfirmTitle(void);
	void UpdateMessage(void);

	void MoveCursor(int& cursor, int count);					// 上下キーでカーソルを動かす（ループする）
	void ShowMessage(const std::vector<std::string>& lines, Mode returnMode);
	void RefreshBagList(void);									// もちものの一覧を作り直す
	void UseItemOnTarget(void);									// 選んだアイテムを手持ちに使う

	// ---------- 描画 ----------
	void DrawScreen(Mode mode);
	void DrawWindow(int x1, int y1, int x2, int y2);
	void DrawMenu(void);
	void DrawPartyList(int cursor, int markIndex, const char* title);
	void DrawPartyAction(void);
	void DrawPartyDetail(void);
	void DrawBag(void);
	void DrawOption(void);
	void DrawConfirmTitle(void);
	void DrawMessage(void);
	void DrawHpBar(int x, int y, int w, int h, float ratio);

	// ---------- 補助 ----------
	int GetImage(const std::string& path);					// 画像を読み込む（同じパスはキャッシュを返す）
	void DrawImageFit(int img, int x, int y, int size);		// 正方形の枠に収めて描く
	int SX(int x) const;									// 640x480基準のX座標を実画面に合わせる
	int SY(int y) const;									// 640x480基準のY座標を実画面に合わせる

private:

	static const int BASE_W = 640;
	static const int BASE_H = 480;

	// 外部から渡されるもの
	const MonsterData* data_ = nullptr;
	MonsterParty* party_ = nullptr;
	const ItemData* items_ = nullptr;
	Inventory* inventory_ = nullptr;
	std::function<bool(void)> saveFunc_;

	Mode mode_ = Mode::MENU;
	Mode returnMode_ = Mode::MENU;		// メッセージを閉じたあとに戻る画面

	int menuCursor_ = 0;
	int partyCursor_ = 0;
	int actionCursor_ = 0;
	int swapFrom_ = 0;					// 入れ替え元
	int bagCursor_ = 0;
	int targetCursor_ = 0;				// アイテムを使う相手
	int confirmCursor_ = 1;				// 0:はい 1:いいえ
	int usingItemId_ = 0;

	std::vector<int> bagList_;			// もちものに表示するアイテムID
	std::vector<std::string> msg_;		// 表示中のメッセージ

	std::unordered_map<std::string, int> images_;	// パス→画像ハンドルのキャッシュ
};