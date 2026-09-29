#include <DxLib.h>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include "Stage.h"
#include "../../../Utility/Utility.h"
#include "../../../AppSystem/Camera/Camera2D.h"

Stage::Stage()
{
}

Stage::~Stage()
{
}

void Stage::Init(void)
{
	Load();
	LoadGroundData();
	LoadHouseData();

	ChangeMapType(MAP_TYPE::E_TYPE_GROUND);
}

void Stage::Load(void)
{
	int err = LoadDivGraph("Data/Image/Map/Mapchip.png", MAP_CHIP_ALL_NUM,
		MAP_CHIP_NUM_X, MAP_CHIP_NUM_Y, 
		MAP_CHIP_SIZE_X, MAP_CHIP_SIZE_Y, mapChipHandle_);
	if (err == -1) {
		MessageBoxA(NULL, "マップチップ画像の読み込みに失敗しました", "エラー", MB_OK);
	}
}

void Stage::LoadEnd(void)
{
	Init();
}	

void Stage::Update(void)
{
}	

void Stage::Draw(void)
{
	// まず背景を黒で塗りつぶす
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, GetColor(0, 0, 0), true);

	// マップチップ画像を表示する
// マップチップ画像を表示する
// dispMapSize を使うことで、現在切り替わっているマップのサイズに安全に合わせられます
	for (int i = 0; i < dispMapSize.x; i++)
	{
		for (int j = 0; j < dispMapSize.y; j++)
		{
			// マップチップの座標を計算
			int x = i * MAP_CHIP_SIZE_X;
			int y = j * MAP_CHIP_SIZE_Y;

			// マップチップの種類を取得（dispMapDat または groundMapDat）
			int chipNo = dispMapDat[j][i];
			if (chipNo >= 0 && chipNo < MAP_CHIP_ALL_NUM)
			{
				// マップチップを描画
				DrawGraph(
					Camera2D::GetInstance()->WorldToScreenX(x),
					Camera2D::GetInstance()->WorldToScreenY(y),
					mapChipHandle_[chipNo],
					true);
			}
		}
	}
}	

void Stage::Release(void)
{
	// マップチップデータの解放
	for (int i = 0; i < MAP_CHIP_ALL_NUM; i++) {
		if (mapChipHandle_[i] != -1) {
			DeleteGraph(mapChipHandle_[i]);
			mapChipHandle_[i] = -1;
		}
	}
}

bool Stage::LoadGroundData(void)
{
	//マップデータ読み込みバッファの初期化
	for (int yy = 0; yy < MAP_GROUND_NUM_Y; yy++) {
		for (int xx = 0; xx < MAP_GROUND_NUM_X; xx++) {
			groundMapDat[yy][xx] = -1;
		}
	}

	// ファイルストリーム
	std::ifstream ifs = std::ifstream("Data/Map/gg.csv");
	if (!ifs) {
		printf("ファイルが開けませんでした\n");
		return false;
	}

	// 汎用機能Split を使った読み込み処理
	std::string line;
	std::vector<std::string>strSplit;		// 一行分のデータを分割した結果を格納する動的配列
	int chipNo = 0;
	int yy = 0;

	while (getline(ifs, line)) {
		if (yy >= MAP_GROUND_NUM_Y) break; // 行数制限
		strSplit = Utility::Split(line, ',');
		for (int xx = 0; xx < strSplit.size() && xx < MAP_GROUND_NUM_X; xx++) { // 列数制限
			chipNo = stoi(strSplit[xx]);
			groundMapDat[yy][xx] = chipNo;
		}
		yy++;
	}

	return true;
}

bool Stage::LoadHouseData(void)
{
	// マップデータを読み込みバッファの初期化
	for (int yy = 0; yy < MAP_UNDER_HOUSE_NUM_Y; yy++) {
		for (int xx = 0; xx < MAP_UNDER_HOUSE_NUM_X; xx++) {
			houseMapDat[yy][xx] = -1;
		}
	}

	// ファイルストリーム
	std::ifstream ifs = std::ifstream("Data/Map/House.csv");
	if (!ifs) return false;

	// 汎用機能Split を使った読み込み処理
	std::string line;
	std::vector<std::string> strSplit;
	int chipNo = 0;
	int yy = 0;

	while (getline(ifs, line)) {
		if (yy >= MAP_UNDER_HOUSE_NUM_Y) break; // 行数制限
		// １行の文字列をカンマ区切りで分割する
		strSplit = Utility::Split(line, ',');
		for (int xx = 0; xx < strSplit.size() && xx < MAP_UNDER_HOUSE_NUM_X; xx++) {
			// string から int に変換する
			chipNo = stoi(strSplit[xx]);

			// マップデータ(２次元配列)にマップチップ番号を格納する
			houseMapDat[yy][xx] = chipNo;
		}
		yy++;
	}

	return true;
}

Vector2 Stage::GetMapChipPos(int mapChipNumX, int mapChipNumY)
{
	return Vector2(mapChipNumX * MAP_CHIP_SIZE_X, mapChipNumY * MAP_CHIP_SIZE_Y);
}

int Stage::GetMapChipNum(Vector2 pos)
{
	return 0;
}

void Stage::ChangeMapType(MAP_TYPE type)
{
	// マップのクリア
	ClearDispMap();

	mapType = type;

	switch (mapType)
	{
	case MAP_TYPE::E_TYPE_GROUND:
		// マップサイズの設定
		dispMapSize.x = MAP_GROUND_NUM_X;
		dispMapSize.y = MAP_GROUND_NUM_Y;

		// マップデータを設定する
		for (int yy = 0; yy < dispMapSize.y; yy++) {
			for (int xx = 0; xx < dispMapSize.x; xx++) {
				dispMapDat[yy][xx] = groundMapDat[yy][xx];
			}
		}
		break;
	case MAP_TYPE::E_TYPE_HOUSE:
		// マップサイズの設定
		dispMapSize.x = MAP_UNDER_HOUSE_NUM_X;
		dispMapSize.y = MAP_UNDER_HOUSE_NUM_Y;

		// マップデータを設定する
		for (int yy = 0; yy < dispMapSize.y; yy++) {
			for (int xx = 0; xx < dispMapSize.x; xx++) {
				dispMapDat[yy][xx] = houseMapDat[yy][xx];
			}
		}
		break;
	}

}

void Stage::ClearDispMap(void)
{
	for (int yy = 0; yy < MAP_MAX_CHIP_Y; yy++) {
		for (int xx = 0; xx < MAP_MAX_CHIP_X; xx++) {
			dispMapDat[yy][xx] = -1;
		}
	}
}

