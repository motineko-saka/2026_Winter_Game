#pragma once
#include <map>
#include <string>

class ItemData;

// 所持アイテムの管理クラス（バッグ）
// ・アイテムIDごとの所持数を持つ（IDの小さい順に並ぶ）
// ・追加／削除／所持数の取得
// ・セーブ／ロード
class Inventory
{
public:

	static const int COUNT_MAX = 99;	// 1種類あたりの所持数の上限

	Inventory();
	~Inventory();

	void Init(void);	// 初期化（空にする）
	void Release(void);	// 解放

public:

	// 追加する。上限を超える分は切り捨て。1個も入らなければfalse
	bool Add(int itemId, int count = 1);

	// 減らす。足りなければ何もせずfalse
	bool Remove(int itemId, int count = 1);

	int GetCount(int itemId) const;						// 所持数（持っていなければ0）
	bool Has(int itemId, int count = 1) const;			// count個以上持っているか

	// 所持品の一覧（アイテムID → 個数）
	const std::map<int, int>& GetAll(void) const { return items_; }

public:

	// ---------- セーブ／ロード ----------

	// バッグをテキストファイルに保存する（保存先のフォルダは先に作っておくこと）
	bool Save(const std::string& path) const;

	// ファイルから読み込む。失敗した場合（ファイルなし・壊れている・存在しないアイテム等）は
	// falseを返し、今の中身は変更しない。
	bool Load(const std::string& path, const ItemData& data);

private:

	std::map<int, int> items_;	// アイテムID → 個数
};
