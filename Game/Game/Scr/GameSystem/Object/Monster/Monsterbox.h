#pragma once
#include <string>
#include <vector>
#include "MonsterData.h"

class MonsterParty;

// ボックス（預かりシステム）
// ・BOX_COUNT個のボックスに、それぞれSLOT_COUNT匹まで預けられる
// ・スロットの位置は固定（途中が空いていてもよい）
// ・手持ちとのやりとり（預ける／引き出す／入れ替え）もここで行う
class MonsterBox
{
public:

	static const int BOX_COUNT = 8;		// ボックスの数
	static const int SLOT_COUNT = 30;	// 1ボックスの収容数

	MonsterBox();
	~MonsterBox();

	void Init(void);	// 空のボックスを作る
	void Release(void);	// 解放

public:

	// ---------- 手持ちとのやりとり ----------
	// 手持ちが空、または「戦えるモンスターがいない」状態になる操作は失敗する（falseを返す）

	// 手持ち → ボックス。boxIndexが-1なら最初に空きのあるボックスへ
	bool Deposit(MonsterParty& party, int partyIndex, int boxIndex = -1);

	// ボックス → 手持ち（手持ちが満員ならfalse）
	bool Withdraw(MonsterParty& party, int boxIndex, int slotIndex);

	// 手持ちとボックスのスロットを入れ替える（スロットが空なら預けるのと同じ）
	bool SwapWithParty(MonsterParty& party, int partyIndex, int boxIndex, int slotIndex);

	// ---------- ボックスの操作 ----------

	// 空きスロットの先頭に追加（捕獲時に手持ちが満員のときなど）。boxIndexが-1なら空きのあるボックスへ
	bool Add(const MonsterInstance& monster, int boxIndex = -1);

	bool Remove(int boxIndex, int slotIndex);	// 逃がす・削除

	// ボックス内の移動。移動先が埋まっていれば入れ替える
	bool Move(int fromBox, int fromSlot, int toBox, int toSlot);

	// ---------- 取得 ----------

	MonsterInstance* Get(int boxIndex, int slotIndex);	// 空・範囲外ならnullptr
	const MonsterInstance* Get(int boxIndex, int slotIndex) const;

	int GetCount(int boxIndex) const;		// そのボックスの匹数
	int GetTotalCount(void) const;			// 全ボックスの匹数
	int FindEmptyBox(void) const;			// 空きのある最初のボックス（なければ-1）
	bool IsFull(void) const;				// 全ボックスが満杯か

	const std::string& GetBoxName(int boxIndex) const;
	bool SetBoxName(int boxIndex, const std::string& name);

	// ---------- セーブ／ロード ----------

	bool Save(const std::string& path) const;
	// 失敗した場合はfalseを返し、今のボックスは変更しない
	bool Load(const std::string& path, const MonsterData& data);

private:

	struct BoxSlot
	{
		bool used = false;
		MonsterInstance monster{};
	};

	struct Box
	{
		std::string name;
		std::vector<BoxSlot> slots;
	};

	bool IsValidPosition(int boxIndex, int slotIndex) const;
	int FindEmptySlot(int boxIndex) const;		// ボックス内の空きスロット（なければ-1）

	std::vector<Box> boxes_;
};