#include <DxLib.h>
#include "FpsControl.h"
#include "../Application/Application.h"

// コンストラクタ
FpsControl::FpsControl()
	: currentTime(0)
	, prevFrameTime(0)
	, frameCnt(0)
	, updateFrameRateTime(0)
	, frameRate(0.0f)
{
}

// デストラクタ
FpsControl::~FpsControl()
{
}

// 初期化
void FpsControl::Init(void)
{
	currentTime = 0;
	prevFrameTime = 0;
	frameCnt = 0;
	updateFrameRateTime = 0;
	frameRate = 0.0f;
}

// フレームレート更新
bool FpsControl::UpdateFrameRate(void)
{
	Sleep(1);	// システムに処理を返す

	// 現在の時刻を取得
	currentTime = GetNowCount();

	// 現在の時刻が、前回のフレーム実行時より
	// 1/60秒経過していたら処理を実行する
	if (currentTime - prevFrameTime >= FRAME_RATE)
	{
		// フレーム実行時の時間を更新
		prevFrameTime = currentTime;

		// フレーム数をカウント
		frameCnt++;

		// 1/60経過した
		return true;
	}

	return false;
}

// フレームレート計算
void FpsControl::CalcFrameRate()
{
	// 前回のフレームレート更新からの経過時間を求める
	int difTime = currentTime - updateFrameRateTime;

	// 前回のフレームレートを更新から
	// 1秒以上経過していたらフレームレートを更新する
	if (difTime > 1000)
	{
		// フレーム回数をミリ秒に合わせる
		// 小数まで出したのでfloatにキャスト
		float castFrameCnt = (float)(frameCnt * 1000);

		// フレームレートを求める
		// 理想通りなら 60000 / 1000 で 60 となる
		frameRate = castFrameCnt / difTime;

		// フレームカウントをクリア
		frameCnt = 0;

		// フレームレート更新時間を更新
		updateFrameRateTime = currentTime;
	}
}

// フレームレートを表示(デバッグ用)
void FpsControl::DrawFrameRate()
{
	// スクリーンの右端に来るように設定
	DrawFormatString(
		Application::SCREEN_SIZE_X - 90,
		0,
		0xFF0000,
		"FPS[%.2f]",
		frameRate
	);
}
