#pragma once
#include <string>
#include <unordered_map>
#include "../Monster/MonsterData.h"

// アイテムの分類（バッグのポケット分けや、使える場面の目安に使う）
enum class ItemCategory
{
	MEDICINE,   // 回復薬
	BALL,       // 捕獲用のボール
	BATTLE,     // 戦闘中に能力を上げるもの
	EVOLUTION,  // 進化アイテム
	KEY,        // 大事なもの
};

// アイテムの効果（Item.csv の「効果」列に書く名前と対応）
//   効果値の意味は効果ごとに違う：
//     HEAL_HP      … 回復するHP（9999など大きい値で全回復）
//     REVIVE       … ひんしから復活したときのHP割合（%）。50で半分、100で全快
//     CURE_STATUS  … 使わない（治す状態異常は「状態異常」列で指定。空欄ならすべて）
//     RESTORE_PP   … 各技が回復するPP（999など大きい値で全回復）
//     CATCH        … 捕獲補正（%）。100で等倍、150で1.5倍、255以上でほぼ確実
//     ATK_UP       … 攻撃ランクの上昇段階（戦闘中のみ）
//     DEF_UP       … 防御ランクの上昇段階（戦闘中のみ）
//     EVOLVE       … 使わない（どのモンスターが進化するかは MonsterEvolution.csv の必要アイテムIDで決まる）
enum class ItemEffect
{
	NONE,
	HEAL_HP,       // HP回復
	REVIVE,        // ひんし回復
	CURE_STATUS,   // 状態異常の回復
	RESTORE_PP,    // PP回復
	CATCH,         // 捕獲（ボール）
	ATK_UP,        // 攻撃ランク上昇（戦闘中）
	DEF_UP,        // 防御ランク上昇（戦闘中）
	EVOLVE,        // 進化
};

// ---------- マスターデータ（変わらない定義） ----------

struct ItemMasterData
{
	int id;                  // アイテムID（1から始めること。0は「なし」として使う）
	std::string name;        // 名前
	ItemCategory category;   // 分類
	ItemEffect effect;       // 効果
	int effectValue;         // 効果の数値
	StatusAilment cureTarget;// CURE_STATUS で治す状態異常（NONEなら全部）
	int price;               // 買値（売値は半額などにする想定）
	bool usableInBattle;     // 戦闘中に使えるか
	bool usableInField;      // フィールドで使えるか
	bool consumable;         // 使うと減るか
	std::string iconPath;    // アイコン画像のパス
	std::string description; // 説明
};

// アイテムのマスターデータを管理するクラス（MonsterData と同じ作り）
class ItemData
{

public:

	ItemData();
	~ItemData();

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み（Data/CSVData/Item.csv）
	void LoadEnd(void);	// 読み込み後の処理
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

public:

	// アイテムのマスターデータをCSVから読み込む関数（失敗したらfalse）
	bool LoadItems(const std::string& path);

	// アイテムのマスターデータを取得する関数（なければnullptr）
	const ItemMasterData* GetItem(int id) const;

private:

	std::unordered_map<int, ItemMasterData> items_;
};
