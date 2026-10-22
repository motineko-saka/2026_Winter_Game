#pragma once

#include <string>
#include <unordered_map>

// ボール1種類ぶんの演出設定（BallEffect.csv の1行）
// 画像を作っていない項目は空欄にしておけば、仮の見た目（円やフラッシュ）で動く
struct BallEffectMaster
{
	int id = 0;
	std::string name;

	// ボール本体
	std::string ballImage;		// 閉じているボール画像（空なら円で代用）
	std::string openImage;		// 開いたボール画像（空なら閉じた画像のまま）
	int ballSize = 28;			// 表示サイズ（640x480基準のピクセル）
	int bounces = 2;			// 地面に落ちてからバウンドする回数

	// ボールが開いたときのエフェクト（スプライトシート）
	std::string effectImage;	// 空ならエフェクトなし（簡易フラッシュ）
	int columns = 1;
	int frameCount = 1;
	int frameW = 0;
	int frameH = 0;
	int frameTime = 3;			// 1コマあたりのフレーム数
	int scalePercent = 100;
	bool additive = true;		// 加算合成で描くか

	// モンスターが出てくるときの光の色
	int glowR = 255;
	int glowG = 255;
	int glowB = 255;

	// 捕まえたときの光のエフェクト（スプライトシート）。空なら簡易の星と光の輪
	std::string catchImage;
	int catchColumns = 1;
	int catchFrameCount = 1;
	int catchFrameW = 0;
	int catchFrameH = 0;
	int catchFrameTime = 3;
	int catchScalePercent = 100;
};

// ボール演出のマスターデータ（CSVから読み込む）
//
// CSV形式（1行目はヘッダー、#で始まる行はコメント）
//   id,name,ballImage,openImage,ballSize,bounces,effectImage,columns,frameCount,frameW,frameH,frameTime,scalePercent,additive,glowR,glowG,glowB,
//     catchImage,catchColumns,catchFrameCount,catchFrameW,catchFrameH,catchFrameTime,catchScalePercent
//   ・捕まえる演出では、id にボールのアイテムIDを書くと、そのボールを投げたときの演出になる
//   ・捕まえるときの吸い込みと、失敗して出てくるときは、effectImage（モンスターを出す演出と同じ）を使う
//   ・追加した列（catch〜）は空欄でも動く（簡易の星と光の輪になる）
class BallEffectData
{
public:

	// 読み込む（失敗したらfalse。その場合は Get が nullptr を返し、仮の演出で進む）
	bool Load(const std::string& path);

	const BallEffectMaster* Get(int id) const;

private:

	std::unordered_map<int, BallEffectMaster> balls_;
};