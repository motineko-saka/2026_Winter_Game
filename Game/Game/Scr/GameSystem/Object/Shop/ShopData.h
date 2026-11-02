#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// ショップ1軒分の品ぞろえ
struct ShopMasterData
{
	int id = 0;						// ショップID（1から）
	std::vector<int> itemIds;		// 売っているアイテムID（CSVに書いた順に並ぶ）
};

// ショップの定義を管理するクラス（EventData / TrainerData と同じ作り）
//   Data/CSVData/Shop.csv
//   #ショップID,アイテムID
//   1,1
//   1,2
//   2,5
//   （1行に1品。同じショップIDの行をまとめて、そのショップの品ぞろえにする）
//
// フィールドからは Event.csv の台本欄に「@shop:ショップID」と書くと開く。
class ShopData
{
public:

	ShopData() = default;

	void Init(void);	// 初期化
	void Load(void);	// 読み込み（Data/CSVData/Shop.csv）
	void Release(void);	// 解放

	// CSVから読み込む（失敗したら false。その場合は今の中身を変えない）
	bool LoadShops(const std::string& path);

	// ショップを取得する（なければ nullptr）
	const ShopMasterData* GetShop(int id) const;

private:

	std::unordered_map<int, ShopMasterData> shops_;
};