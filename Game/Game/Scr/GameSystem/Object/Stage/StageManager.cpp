#include <DxLib.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include "StageManager.h"
#include "../../../Utility/Utility.h"
#include "../../../AppSystem/Camera/Camera2D.h"

StageManager* StageManager::instance_ = nullptr;

StageManager::StageManager()
{
}

StageManager::~StageManager()
{
}

void StageManager::Init()
{
	Load();

	// 初期マップを読み込む
	ChangeMap("Data/Map/Mapcsv/gg.csv");
}

void StageManager::Load()
{
	int err = LoadDivGraph("Data/Image/Map/Mapchip.png", MAP_CHIP_ALL_NUM,
		MAP_CHIP_NUM_X, MAP_CHIP_NUM_Y,
		MAP_CHIP_SIZE_X, MAP_CHIP_SIZE_Y, mapChipHandle_);
	if (err == -1) {
		MessageBoxA(NULL, "マップチップ画像の読み込みに失敗しました", "エラー", MB_OK);
	}
}

void StageManager::LoadEnd()
{
	Init();
}

void StageManager::Update()
{

}

void StageManager::Draw()
{
	// 背景を黒で塗りつぶす
	DrawBox(0, 0, Application::SCREEN_SIZE_X, Application::SCREEN_SIZE_Y, GetColor(0, 0, 0), true);

	// 現在のマップのサイズに合わせて描画ループを回す
	for (int y = 0; y < currentMap.height; y++)
	{
		for (int x = 0; x < currentMap.width; x++)
		{
			int index = y * currentMap.width + x;
			int px = x * MAP_CHIP_SIZE_X;
			int py = y * MAP_CHIP_SIZE_Y;

			// まず「背景（地面）」レイヤーを描画
			if (index < currentMap.groundTiles.size())
			{
				int chipNo = currentMap.groundTiles[index];
				if (chipNo >= 0 && chipNo < MAP_CHIP_ALL_NUM)
				{
					DrawGraph(
						Camera2D::GetInstance()->WorldToScreenX(px),
						Camera2D::GetInstance()->WorldToScreenY(py),
						mapChipHandle_[chipNo],
						true);
				}
			}

			// その上から「オブジェクト（木や建物など）」レイヤーを重ねて描画 
			if (index < currentMap.objectTiles.size())
			{
				int objChipNo = currentMap.objectTiles[index];
				// Tiledで何も置いていないマス（空欄）は -1 や 0 になるため、有効なチップのみ描画
				if (objChipNo >= 0 && objChipNo < MAP_CHIP_ALL_NUM)
				{
					DrawGraph(
						Camera2D::GetInstance()->WorldToScreenX(px),
						Camera2D::GetInstance()->WorldToScreenY(py),
						mapChipHandle_[objChipNo],
						true);
				}
			}
		}
	}
}

void StageManager::Release()
{
	for (int i = 0; i < MAP_CHIP_ALL_NUM; i++) {
		if (mapChipHandle_[i] != -1) {
			DeleteGraph(mapChipHandle_[i]);
			mapChipHandle_[i] = -1;
		}
	}
}

bool StageManager::LoadSingleCsv(const std::string& filename, int& outWidth, int& outHeight, std::vector<int>& outTiles)
{
	std::ifstream ifs = std::ifstream(filename);
	if (!ifs) {
		return false; // ファイルが開けない場合はfalseを返す
	}

	std::string line;
	std::vector<std::vector<int>> tempGrid;

	while (getline(ifs, line)) {
		if (line.empty()) continue;

		std::vector<std::string> strSplit = Utility::Split(line, ',');
		std::vector<int> rowData;
		for (const auto& valStr : strSplit) {
			rowData.push_back(std::stoi(valStr));
		}
		tempGrid.push_back(rowData);
	}

	outHeight = static_cast<int>(tempGrid.size());
	if (outHeight == 0) return false;
	outWidth = static_cast<int>(tempGrid[0].size());

	outTiles.clear();
	outTiles.resize(outWidth * outHeight);

	for (int y = 0; y < outHeight; y++) {
		for (int x = 0; x < outWidth; x++) {
			outTiles[y * outWidth + x] = tempGrid[y][x];
		}
	}

	return true;
}

bool StageManager::LoadMapData(const std::string& filename, MapData& outMap)
{
	// 背景（地面）レイヤーのCSVを読み込む
	if (!LoadSingleCsv(filename, outMap.width, outMap.height, outMap.groundTiles)) {
		std::string err_msg = "背景マップファイルが開けませんでした: " + filename;
		MessageBoxA(NULL, err_msg.c_str(), "エラー", MB_OK);
		return false;
	}

	// オブジェクトレイヤーのCSVファイルパスを自動生成して読み込む
	std::string objFilename = filename;
	size_t dotPos = objFilename.find_last_of('.');
	if (dotPos != std::string::npos) {
		objFilename.insert(dotPos, "_オブジェクト");
	}

	// オブジェクト用CSVの読み込み（ファイルがなくてもクラッシュしないようチェック）
	int objW, objH;
	LoadSingleCsv(objFilename, objW, objH, outMap.objectTiles);

	return true;
}

bool StageManager::LoadWarpData(const std::string& filename)
{
	warpList.clear(); // 前のマップのワープ情報をクリア

	std::ifstream ifs = std::ifstream(filename);
	if (!ifs) {
		// ワープが存在しないマップ（普通の家の中など）ならファイルがなくてもエラーにせず終了
		return false;
	}

	std::string line;
	while (getline(ifs, line)) {
		// 空行や、# から始まるコメント行はスキップ
		if (line.empty() || line[0] == '#') continue;

		std::vector<std::string> strSplit = Utility::Split(line, ',');
		if (strSplit.size() >= 5) {
			WarpData warp;
			warp.x = std::stoi(strSplit[0]);
			warp.y = std::stoi(strSplit[1]);
			warp.nextMap = strSplit[2];
			warp.destX = std::stoi(strSplit[3]);
			warp.destY = std::stoi(strSplit[4]);

			warpList.push_back(warp);
		}
	}
	return true;
}

void StageManager::ChangeMap(const std::string& filename)
{
	MapData newMap;
	if (LoadMapData(filename, newMap)) {
		currentMap = newMap; // マップデータの更新

		// マップファイル名から自動的に対応するワープファイル名を生成して読み込む
		// 例: "Data/Map/gg.csv" -> "Data/Map/gg_warp.csv"
		std::string warpFilename = filename;
		size_t dotPos = warpFilename.find_last_of('.');
		if (dotPos != std::string::npos) {
			warpFilename.insert(dotPos, "_warp");
		}

		// ワープデータの読み込みを試みる（ファイルが無くてもクラッシュしないようにする）
		LoadWarpData(warpFilename);
	}
}

bool StageManager::CheckWarp(float playerX, float playerY, std::string& outNextMap, float& outNewPx, float& outNewPy)
{
	// プレイヤーのピクセル座標を「何マス目か」に変換
	int tileX = static_cast<int>(playerX) / MAP_CHIP_SIZE_X;
	int tileY = static_cast<int>(playerY) / MAP_CHIP_SIZE_Y;

	// 現在のマップのワープリストを総チェック
	for (const auto& warp : warpList) {
		if (warp.x == tileX && warp.y == tileY) {
			// 一致した！ 移動先情報を出力
			outNextMap = warp.nextMap;
			// マス座標をピクセル座標（ワールド座標）に変換して返す
			outNewPx = static_cast<float>(warp.destX * MAP_CHIP_SIZE_X);
			outNewPy = static_cast<float>(warp.destY * MAP_CHIP_SIZE_Y);
			return true;
		}
	}
	return false;
}
