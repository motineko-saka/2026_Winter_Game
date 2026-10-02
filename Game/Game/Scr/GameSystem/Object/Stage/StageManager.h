#pragma once
#pragma once
#include <vector>
#include <string>
#include <map>
#include "../../../Common/Vector2.h"
#include "../../../AppSystem/Application/Application.h"

// 1つのマップデータを表す構造体
struct MapData {
	int width = 0;                          // マップの横マス数
	int height = 0;                         // マップの縦マス数
	std::vector<int> tiles;                 // チップIDの配列（サイズは width * height）
	std::vector<int> groundTiles;		    // 背景（地面）レイヤーのタイル配列
	std::vector<int> objectTiles;		    // オブジェクトレイヤーのタイル配列
};

// ワープ情報を表す構造体
struct WarpData {
	int stageId;
	int x, y;               // 発動するマップのマス座標 (X, Y)
	int nextStageId;	    // 移動先のCSVファイルパス
	int destX, destY;       // 移動先のマップでの出現マス座標 (X, Y)
};

// ステージ情報を表す構造体
struct StageInfo {
	int id;
	std::string groundCsv;
	std::string objectCsv;
	std::string chipImagePath;
	int chipSizeX;
	int chipSizeY;
	int chipNumX;
	int chipNumY;
};

class StageManager
{
public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new StageManager(); } };
	static StageManager* GetInstance(void) { return instance_; };
	static void DeleteInstance(void) { if (instance_ != nullptr) { delete instance_; instance_ = nullptr; } }

private:
	// デフォルトコンストラクタをprivateにして、
	// 外部から生成できない様にする
	StageManager(void);
	// デストラクタも同様
	~StageManager(void);

	// コピー・ムーブ操作を禁止
	StageManager(const StageManager&) = delete;
	StageManager& operator=(const StageManager&) = delete;
	StageManager(StageManager&&) = delete;
	StageManager& operator=(StageManager&&) = delete;

	// 下記をコンパイルエラーさせるため 上記を追加
	// SceneManager copy = *SceneManager::GetInstance();
	// SceneManager copied(*SceneManager::GetInstance());
	// SceneManager moved = std::move(*SceneManager::GetInstance());
public:

	void Init(void);								// 初期化
	void Load(void);								// 読み込み
	void LoadEnd(void);							// 読み込み後の処理
	void Update(void);								// 更新
	void Draw(void);								// 描画
	void Release(void);								// 解放

public:

	// ステージリスト（CSV）を読み込む関数
	bool LoadStageList(const std::string& listFilename);

	// 任意のCSVファイルから1層分のマップデータを読み込むヘルパー関数
	bool LoadSingleCsv(const std::string& filename, int& outWidth, int& outHeight, std::vector<int>& outTiles);

	// 任意のCSVファイルから任意のサイズのマップを読み込む関数
	bool LoadMapData(const std::string& filename, MapData& outMap);

	// マップのワープするとこをcsvから読み込む関数
	bool LoadWarpData(const std::string& filename);

	// ステージIDを指定してマップを切り替える
	void ChangeStage(int stageId);

	// プレイヤーの現在位置（ピクセル）からワープを判定する関数
	// 引数に「次のマップファイル名」「移動先の新ピクセル座標」が格納される
	bool CheckWarp(float playerX, float playerY, int& outNextStageId, float& outNewPx, float& outNewPy);
private:

	// 静的インスタンス
	static StageManager* instance_;

	std::vector<int> mapChipHandle_;        // マップチップのハンドル配列

	MapData currentMap;                     // 現在表示・管理しているマップデータ
	std::vector<WarpData> warpList;         // 現在のマップに存在するワープのリスト

	// 全ステージの情報を保持するマップ（IDをキーにする）
	std::map<int, StageInfo> stageListMap;

	int currentChipSizeX = 32;
	int currentChipSizeY = 32;
	int currentChipNumX = 32;
	int currentChipNumY = 32;

	int currentStageId = 0; // 現在のステージID
};