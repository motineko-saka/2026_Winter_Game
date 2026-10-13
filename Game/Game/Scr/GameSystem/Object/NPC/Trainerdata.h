#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// トレーナーの手持ち1匹分
struct TrainerPartyEntry
{
	int monsterId;		// 図鑑番号
	int level;			// レベル
};

// ---------- マスターデータ（変わらない定義） ----------

// トレーナー
struct TrainerMasterData
{
	int id;							// トレーナーID（1から。マップ上で1人につき1つ）
	std::string name;				// 名前（「たんぱんこぞう ケン」など）
	// マップに出す画像の指定（空なら仮の四角を出す）
	//   ken.png                … 1枚（どの向きでも同じ絵）
	//   ken.png@4              … 1枚を横に4等分（左から 上・下・左・右。歩きなし）
	//   ken.png@3x4            … 横が歩きのコマ、縦が 上・下・左・右（3コマ×4向き）
	//   up.png|down.png|left.png|right.png
	//                          … 向きごとに1枚ずつ（上|下|左|右の順）
	//   up.png@3|down.png@3|left.png@3|right.png@3
	//                          … 向きごとの画像を、それぞれ横にコマ分割
	//   Data/Image/Trainer/Ken/  … フォルダ指定（末尾に / ）。プレイヤーと同じ作り方
	//       Idle.png / Walk_1.png, Walk_2.png / Back_Walk_1.png, Back_Walk_2.png / Beside_1.png, Beside_2.png
	//       （右向きの Beside を左右反転して左向きに使う。Back_Idle.png, Beside_Idle.png は省略可）
	//   歩きのコマ：先頭のコマが立ち絵。歩くときは、2コマなら交互、3コマ以上なら立ち絵を除いて順番に
	std::string imagePath;
	int prize;						// 勝ったときの賞金
	std::string introMessage;		// 勝負を仕掛けてくるときのセリフ（空なら出さない）
	std::string defeatMessage;		// 負けたときのセリフ（空なら出さない）
	std::vector<TrainerPartyEntry> party;	// 出してくるモンスター（先頭から順に）
};

// トレーナーの向き
enum class TrainerDir
{
	UP,
	DOWN,
	LEFT,
	RIGHT,
};

// マップ上のトレーナーの配置
struct TrainerSpawn
{
	int stageId;		// 配置するステージ
	int tileX;			// 立っているマス(X)
	int tileY;			// 立っているマス(Y)
	int trainerId;		// TrainerMasterData の id
	TrainerDir dir;		// 向き（この方向にまっすぐ見える）
	int sight;			// 何マス先まで見えるか
};

// トレーナーのデータを管理するクラス（ItemData と同じ作り）
//   Data/CSVData/Trainer.csv       … トレーナーの定義
//   Data/CSVData/TrainerSpawn.csv  … マップ上の配置
class TrainerData
{

public:

	TrainerData();
	~TrainerData();

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void Release(void);	// 解放

public:

	// トレーナーの定義を読み込む（失敗したらfalse）
	bool LoadTrainers(const std::string& path);

	// マップ上の配置を読み込む（失敗したらfalse）
	bool LoadSpawns(const std::string& path);

	// トレーナーの定義を取得する（なければnullptr）
	const TrainerMasterData* GetTrainer(int id) const;

	// 配置の一覧
	const std::vector<TrainerSpawn>& GetSpawns(void) const { return spawns_; }

private:

	std::unordered_map<int, TrainerMasterData> trainers_;
	std::vector<TrainerSpawn> spawns_;
};