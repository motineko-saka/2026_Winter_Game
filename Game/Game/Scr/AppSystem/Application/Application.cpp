#include "Application.h"

#include <DxLib.h>

#include "../FpsControl/FpsControl.h"
#include "../InputManager/InputManager.h"
#include "../../GameSystem/Scene/SceneManager.h"

Application* Application::instance_ = nullptr;

const std::string Application::PATH_IMAGE = "Image/";
const std::string Application::PATH_DATA = "Data/";
const std::string Application::PATH_MAP_DATA = PATH_DATA + "GameAsset/GameScene/Map/Map.csv";
const std::string Application::PATH_IMG_PLAYER1 = PATH_DATA + "GameAsset/GameScene/Image/P1/";
const std::string Application::PATH_IMG_PLAYER2 = PATH_IMAGE + "GameAsset/GameScene/Image/P2/";

// コンストラクタ
Application::Application(void)
{
	isInitFail_ = false;
	isReleaseFail_ = false;
	fps_ = nullptr;
}

// デストラクタ
Application::~Application(void)
{
}

// 初期化
void Application::Init(void)
{
	// アプリケーションの初期設定
	SetWindowText("2DBase");

	// ウィンドウ関連
	SetGraphMode(SCREEN_SIZE_X, SCREEN_SIZE_Y, 32);	// サイズ変更
	ChangeWindowMode(true);	// false = フルスクリーン

	// DxLibの初期化
	SetUseDirect3DVersion(DX_DIRECT3D_11);
	isInitFail_ = false;
	if (DxLib_Init() == -1)
	{
		// 初期化失敗
		isInitFail_ = true;
		return;
	}

	// 描画先画面を裏にする
	SetDrawScreen(DX_SCREEN_BACK);

	// 背景色を白に設定
	SetBackgroundColor(0, 255, 255);

	// キー制御初期化
	SetUseDirectInputFlag(true);
	InputManager::CreateInstance();
	InputManager::GetInstance()->Init();

	// シーン管理初期化
	SceneManager::CreateInstance();
	SceneManager::GetInstance()->Init();

	// FPS初期化
	fps_ = new FpsControl;
	fps_->Init();
}

// ゲームループ
void Application::Run(void)
{
	// ゲームループ
	while (ProcessMessage() == 0)
	{
		// シーンがゲーム終了を選んだ
		if (SceneManager::GetInstance()->GetGameEnd()) return;

		// フレームレート更新
		// 1/60秒経過していないなら再ループさせる
		if (!fps_->UpdateFrameRate()) continue;

		// 画面を初期化
		ClearDrawScreen();

		InputManager::GetInstance()->Update();	// 入力制御更新
		SceneManager::GetInstance()->Update();	// シーン管理更新
		SceneManager::GetInstance()->Draw();	// シーン管理描画

		fps_->CalcFrameRate();	// フレームレート計算
		fps_->DrawFrameRate();	// フレームレート描画

		ScreenFlip();
	}
}

// 解放
void Application::Release(void)
{
	// 入力制御削除
	InputManager::GetInstance()->DeleteInstance();

	// シーン管理解放・削除
	SceneManager::GetInstance()->Release();
	SceneManager::GetInstance()->DeleteInstance();

	// フレームレート解放
	delete fps_;

	// DxLib終了
	if (DxLib_End() == -1)
	{
		isReleaseFail_ = true;
	}
}
