#pragma once
#include "../../../Common/Vector2.h"
#include "../../../AppSystem/Application/Application.h"


class Stage
{
public:

	static const int MAP_CHIP_SIZE_X = 32;	// マップチップの横幅
	static const int MAP_CHIP_SIZE_Y = 32;	// マップチップの縦幅
	static const int MAP_CHIP_NUM_X = 32;	// マップチップの横数
	static const int MAP_CHIP_NUM_Y = 32;	// マップチップの縦数
	static const int MAP_CHIP_ALL_NUM = (MAP_CHIP_NUM_X * MAP_CHIP_NUM_Y);
											// マップチップの総数
	static constexpr int MAP_MAX_SIZE_X = (MAP_CHIP_SIZE_X * MAP_CHIP_NUM_X);	// マップの横幅
	static constexpr int MAP_MAX_SIZE_Y = (MAP_CHIP_SIZE_Y * MAP_CHIP_NUM_Y);	// マップの縦幅

	static constexpr int MAP_GROUND_NUM_X = 200;				// 地上のマップサイズ横
	static constexpr int MAP_GROUND_NUM_Y = 200;				// 地上のマップサイズ縦

	static constexpr int MAP_UNDER_HOUSE_NUM_X = 10;		// 地下のマップサイズ横
	static constexpr int MAP_UNDER_HOUSE_NUM_Y = 10;		// 地下のマップサイズ縦

	static constexpr int DSP_CHIP_NUM_X = Application::SCREEN_SIZE_X / MAP_CHIP_SIZE_X;
	static constexpr int DSP_CHIP_NUM_Y = Application::SCREEN_SIZE_Y / MAP_CHIP_SIZE_Y + 1;

	static constexpr int MAP_MAX_CHIP_X = 200;
	static constexpr int MAP_MAX_CHIP_Y = 200;


	enum class MAP_TYPE
	{
		E_TYPE_NONE = 0,	// なし
		E_TYPE_GROUND,      // 地上
		E_TYPE_HOUSE,       // 家
		E_TYPE_MAX,
	};

public:

	Stage();            // コンストラクタ
	~Stage();           // デストラクタ

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void LoadEnd(void);	// 読み込み後の処理
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

public:

	bool LoadGroundData(void);	// 地上データ読み込み
	bool LoadHouseData(void);	// 家データ読み込み

	Vector2 GetMapChipPos(int mapChipNumX, int mapChipNumY);	// マップチップの座標を取得
	Vector2 GetDispMapSize(void) { return dispMapSize; }		// 表示マップサイズを取得
	int GetMapChipNum(Vector2 pos);							    // マップチップ番号を取得
	MAP_TYPE GetMapType(void) { return mapType; }			    // 表示マップ種別を取得
	void ChangeMapType(MAP_TYPE type);                          // 表示マップ種別を変更

private:
	int mapChipHandle_[MAP_CHIP_ALL_NUM];						// マップチップのハンドル

	int dispMapDat[MAP_MAX_CHIP_Y][MAP_MAX_CHIP_X];				// 表示しているマップデータ
	Vector2 dispMapSize;										// 表示しているマップのサイズ 
	MAP_TYPE mapType;											// 表示しているマップの種別
	Vector2 mapDispStPos;										// マップ表示開始座標

	int groundMapDat[MAP_GROUND_NUM_Y][MAP_GROUND_NUM_X];       // 地上マップデータ
	int houseMapDat[MAP_UNDER_HOUSE_NUM_Y][MAP_UNDER_HOUSE_NUM_X]; // 家マップデータ

	void ClearDispMap(void);                                    // 表示マップデータのクリア
};
