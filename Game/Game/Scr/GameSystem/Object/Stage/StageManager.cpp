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
	// 初期状態では空にしておく
	mapChipHandle_.clear();
}

StageManager::~StageManager()
{
}

void StageManager::Init()
{
	Load();

	// 初期マップを読み込む
	LoadStageList("Data/Map/Mapcsv/StageList.csv");

	ChangeStage(0);
}

void StageManager::Load()
{
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
			// 定数ではなく現在のチップサイズを使う
			int px = x * currentChipSizeX;
			int py = y * currentChipSizeY;

			// まず「背景（地面）」レイヤーを描画
			if (index < currentMap.groundTiles.size())
			{
				int chipNo = currentMap.groundTiles[index];
				if (chipNo >= 0 && chipNo < mapChipHandle_.size())
				{
					if (mapChipHandle_[chipNo] != -1) {
						DrawGraph(
							Camera2D::GetInstance()->WorldToScreenX(px),
							Camera2D::GetInstance()->WorldToScreenY(py),
							mapChipHandle_[chipNo],
							true);
					}
				}
			}

			// その上から「オブジェクト」レイヤーを重ねて描画 
			if (index < currentMap.objectTiles.size())
			{
				int objChipNo = currentMap.objectTiles[index];
				if (objChipNo >= 0 && objChipNo < mapChipHandle_.size())
				{
					if (mapChipHandle_[objChipNo] != -1) {
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
}

void StageManager::Release()
{
	for (int handle : mapChipHandle_) {
		if (handle != -1) {
			DeleteGraph(handle);
		}
	}
	mapChipHandle_.clear();
}

bool StageManager::LoadStageList(const std::string& listFilename)
{
	std::ifstream ifs(listFilename);
	if (!ifs) {
		MessageBoxA(NULL, "ステージリストファイルが開けませんでした", "エラー", MB_OK);
		return false;
	}

	stageListMap.clear();
	std::string line;
	while (getline(ifs, line)) {
		if (line.empty() || line[0] == '#') continue; // コメント行スキップ

		std::vector<std::string> strSplit = Utility::Split(line, ',');
		if (strSplit.size() >= 4) {
			StageInfo info;
			info.id = std::stoi(strSplit[0]);
			info.groundCsv = strSplit[1];
			info.objectCsv = strSplit[2];
			info.chipImagePath = strSplit[3];
			info.chipSizeX = std::stoi(strSplit[4]);
			info.chipSizeY = std::stoi(strSplit[5]);
			info.chipNumX = std::stoi(strSplit[6]);
			info.chipNumY = std::stoi(strSplit[7]);

			stageListMap[info.id] = info;
		}
	}
	return true;
}

bool StageManager::LoadSingleCsv(const std::string& filename, int& outWidth, int& outHeight, std::vector<int>& outTiles)
{
	std::ifstream ifs(filename);
	if (!ifs) return false;

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
	warpList.clear();
	std::ifstream ifs = std::ifstream(filename);
	if (!ifs) return false;

	std::string line;
	while (getline(ifs, line)) {
		if (line.empty() || line[0] == '#') continue;

		std::vector<std::string> strSplit = Utility::Split(line, ',');
		if (strSplit.size() >= 5) {
			WarpData warp;
			warp.x = std::stoi(strSplit[0]);
			warp.y = std::stoi(strSplit[1]);
			warp.nextStageId = std::stoi(strSplit[2]); // ステージIDとして数値で格納
			warp.destX = std::stoi(strSplit[3]);
			warp.destY = std::stoi(strSplit[4]);
			warpList.push_back(warp);
		}
	}
	return true;
}

void StageManager::ChangeStage(int stageId)
{
	if (stageListMap.find(stageId) == stageListMap.end()) {
		MessageBoxA(NULL, "指定されたステージIDが見つかりません", "エラー", MB_OK);
		return;
	}

	StageInfo info = stageListMap[stageId];

	// ステージごとのチップ情報を更新
	currentChipSizeX = info.chipSizeX;
	currentChipSizeY = info.chipSizeY;
	currentChipNumX = info.chipNumX;
	currentChipNumY = info.chipNumY;

	// 古い画像を解放し、新しい分割数に合わせて配列サイズを確保
	Release();
	int allNum = currentChipNumX * currentChipNumY;
	mapChipHandle_.resize(allNum, -1);

	// 動的なサイズと分割数で画像をロード
	int err = LoadDivGraph(info.chipImagePath.c_str(), allNum,
		currentChipNumX, currentChipNumY,
		currentChipSizeX, currentChipSizeY, mapChipHandle_.data());
	if (err == -1) {
		MessageBoxA(NULL, "マップチップ画像の読み込みに失敗しました", "エラー", MB_OK);
	}

	// 背景（地面）CSVの読み込み
	MapData newMap;
	if (!LoadSingleCsv(info.groundCsv, newMap.width, newMap.height, newMap.groundTiles)) {
		MessageBoxA(NULL, "背景マップCSVの読み込みに失敗しました", "エラー", MB_OK);
		return;
	}

	// オブジェクトCSVの読み込み
	int objW, objH;
	LoadSingleCsv(info.objectCsv, objW, objH, newMap.objectTiles);

	currentMap = newMap;

	// ワープデータの読み込み
	std::string warpFilename = info.groundCsv;
	size_t dotPos = warpFilename.find_last_of('.');
	if (dotPos != std::string::npos) {
		warpFilename.insert(dotPos, "_warp");
	}
	LoadWarpData(warpFilename);
}

bool StageManager::CheckWarp(float playerX, float playerY, int& outNextStageId, float& outNewPx, float& outNewPy)
{
	// 現在のチップサイズを使って判定
	int tileX = static_cast<int>(playerX) / currentChipSizeX;
	int tileY = static_cast<int>(playerY) / currentChipSizeY;

	for (const auto& warp : warpList) {
		if (warp.x == tileX && warp.y == tileY) {
			outNextStageId = warp.nextStageId;
			outNewPx = static_cast<float>(warp.destX * currentChipSizeX);
			outNewPy = static_cast<float>(warp.destY * currentChipSizeY);
			return true;
		}
	}
	return false;
}