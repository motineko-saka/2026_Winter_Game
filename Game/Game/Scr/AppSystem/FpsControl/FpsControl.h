#pragma once

// フレームレート
static constexpr float FRAME_RATE(1000 / 60);

class FpsControl
{
public:

	FpsControl();	// コンストラクタ
	~FpsControl();	// デストラクタ

public:
	// 初期化
	void Init(void);

	// フレームレート更新
	bool UpdateFrameRate(void);

	// フレームレート計算
	void CalcFrameRate(void);

	// フレームレート表示 (デバッグ表示)
	void DrawFrameRate(void);

private:

	int currentTime;			// 現在の時間
	int prevFrameTime;			// 前回のフレーム実行時の時間

	int frameCnt;				// フレームカウント用
	int updateFrameRateTime;	// フレームレートを更新した時間

	float frameRate;			// フレームレート (表示用)
};
