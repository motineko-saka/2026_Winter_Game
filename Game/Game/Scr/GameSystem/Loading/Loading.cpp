#include <DxLib.h>
#include "Loading.h"

#include "../../AppSystem/Application/Application.h"

// 最低でもロード画面を表示する時間
static constexpr int MIN_LOAD_TIME = 30;	// 60fps(1秒) * x

Loading* Loading::instance_ = nullptr;

// コンストラクタ
Loading::Loading()
	: handle_(-1)
	, pos_(0.0f, 0.0f)
	, isLoading_(false)
	, loadTimer_(0)
{}

// デストラクタ
Loading::~Loading()
{}

// 初期化
void Loading::Init(void)
{
	loadTimer_ = 0;
	isLoading_ = false;
	pos_.x = Application::SCREEN_SIZE_X / 2;
	pos_.y = Application::SCREEN_SIZE_Y / 2;
}

// 読み込み
void Loading::Load(void)
{
	handle_ = LoadGraph("Data/Image/Loading/Image/Imgae.png");
}

// 更新
void Loading::Update(void)
{
	loadTimer_++;

	// 読込中のものがなくなったら and 最低ロード時間経過
	if (GetASyncLoadNum() == 0 && loadTimer_ >= MIN_LOAD_TIME)
	{
		// ロード終了
		Init();
	}
	// 読み込み中
	else
	{
		// ロード画面を動作させるならここに記述
	}
}

// 描画
void Loading::Draw(void)
{
	DrawBox(
		0,
		0,
		Application::SCREEN_SIZE_X,
		Application::SCREEN_SIZE_Y,
		0xFFFFFF,
		true
	);

	DrawRotaGraphF(
		pos_.x, pos_.y,	// 座標
		1.0,			// 拡大値
		0.0,			// 回転値
		handle_,		// ハンドル
		true			// 透過フラグ
	);
}

// 解放
void Loading::Release(void)
{
	DeleteGraph(handle_);
}

// 非同期読み込みに切り替える
void Loading::StartAsyncLoad(void)
{
	isLoading_ = true;
	// 非同期読み込み開始
	SetUseASyncLoadFlag(true);
}

// 同期読み込みに切り替える
void Loading::EndAsyncLoad(void)
{
	// 非同期読み込み終了
	SetUseASyncLoadFlag(false);
}
