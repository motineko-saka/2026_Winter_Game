#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

// タイプ（相性表のインデックスとして使う）
enum class ElementType
{
	NONE = -1,   // タイプ2なしなど
	FIRE,     // 炎
	WATER,    // 水
	WIND,     // 風
	NORMAL,   // ノーマル
	JEWELRY,  // 宝石
	MACHINE,  // 機械
	SPIRIT,   // 霊
	LIGHT,	  // 光
	DARK,     // 闇
	DRAGON,   // 龍
	SOUND,    // 音
	ILLUSION, // 幻
	KARMA,    // 業
	ICE,      // 氷
	POISON,   // 毒
	SOIL,     // 土
	MARTIAL_COMBAT, // 武闘
	GOD,      // 神
	MAX
};

// タイプ相性表（攻撃側×防御側）
using TypeChart = std::array<std::array<float, static_cast<int>(ElementType::MAX)>,
	static_cast<int>(ElementType::MAX)>;

// 技の追加効果の種類
enum class MoveEffect
{
	NONE,
	POISON,         // 毒
	PARALYSIS,      // 麻痺
	ATK_UP,         // 攻撃ランク上昇
	DEF_DOWN,       // 防御ランク低下
	HEAL,           // 回復
	RECOIL,         // 反動
};

// 状態異常
enum class StatusAilment
{
	NONE,
	POISON,    // 毒
	SLEEP,     // 睡眠
	BURN,      // 火傷
	DREAM,     // 夢
	COLD,      // 風邪
};

// 経験値の成長曲線
enum class GrowthRate
{
	FAST,    // 早い
	MEDIUM,  // 普通
	SLOW,    // 遅い
};

// ---------- マスターデータ（変わらない定義） ----------

// 技
struct MoveData
{
	int id;              // 技のID
	std::string name;    // 技名
	ElementType type;    // タイプ
	int power;           // 0なら変化技として扱う
	int accuracy;        // 命中率（%）
	int pp;              // 最大PP
	int priority;        // 優先度（先制技は+1など）
	MoveEffect effect;   // 追加効果
	int effectValue;     // 効果の数値（上昇段階、回復量など）
	int effectChance;    // 効果発動確率（%）
	std::string description; // 説明
};

// レベルアップで覚える技
struct LearnEntry
{
	int level;           // 覚えるレベル
	int moveId;          // MoveDataのid
};

// 進化条件
struct Evolution
{
	int toId;            // 進化先の図鑑番号
	int level;           // 必要レベル（0なら不要）
	int itemId;          // 必要アイテム（0なら不要）
};

// モンスター（種族の設計図）
struct MonsterMasterData
{
	int id;             // 図鑑番号
	std::string name;   // 名前
	ElementType type1;  // タイプ1
	ElementType type2;  // タイプ2（NONEなら1タイプ）

	// 種族値
	int baseHp;         // HP
	int baseAttack;     // 攻撃
	int baseDefense;    // 防御
	int baseSpeed;      // 素早さ

	// 成長・捕獲
	GrowthRate growthRate; // 成長率
	int baseExp;         // 倒したときの基礎経験値
	int catchRate;       // 捕獲率

	// 習得・進化
	std::vector<LearnEntry> learnset;     // レベルアップで覚える技のリスト
	std::vector<Evolution> evolutions;    // 進化のリスト

	// リソース（パスのみ。ロードは別クラスで）
	std::string iconPath;    // アイコン画像のパス
	std::string frontPath;   // 正面画像のパス
	std::string backPath;    // 背面画像のパス
	std::string crySoundPath; // 鳴き声のパス

	// 図鑑用
	std::string description; // 説明
};

// ---------- 個体データ（手持ち1匹分・セーブ対象） ----------

struct MoveSlot
{
	int moveId;          // 技のID
	int currentPp;       // 現在のPP
};

struct MonsterInstance
{
	int monsterId;       // MonsterMasterDataのid
	std::string nickname; // ニックネーム
	int level;           // レベル
	int exp;             // 経験値
	int currentHp;       // 現在のHP
	StatusAilment ailment; // 状態異常
	std::array<MoveSlot, 4> moves; // 技のスロット
};

class MonsterData
{

public:

	MonsterData();
	~MonsterData();

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void LoadEnd(void);	// 読み込み後の処理
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

public:

	// タイプ相性表をCSVから読み込む関数
	bool LoadTypeChart(const std::string& path);

	// タイプ相性を取得する関数（atk:攻撃側、def1:防御側タイプ1、def2:防御側タイプ2）
	float GetTypeEffectiveness(ElementType atk, ElementType def1, ElementType def2) const;

	// 技のマスターデータをCSVから読み込む関数
	bool LoadMoves(const std::string& path);

	// モンスターのマスターデータをCSVから読み込む関数
	bool LoadMonsters(const std::string& path);

	// レベルアップで覚える技のデータをCSVから読み込む関数
	bool LoadLearnset(const std::string& path);

	// 進化条件のデータをCSVから読み込む関数
	bool LoadEvolution(const std::string& path);

	// 技のマスターデータを取得する関数
	const MoveData* GetMove(int id) const;

	// モンスターのマスターデータを取得する関数
	const MonsterMasterData* GetMonster(int id) const;

private:

	TypeChart typeChart_;   // [攻撃側][防御側]

	// マスターデータのコンテナ（IDをキーにして高速検索）
	std::unordered_map<int, MonsterMasterData> monsters_;
	std::unordered_map<int, MoveData> moves_;
};

