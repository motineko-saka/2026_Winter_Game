//--------------------------------
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
// メモリリーク検出のため、_CRTDBG_MAP_ALLOCを定義しておく
#include <DxLib.h>

#include "AppSystem/Application/Application.h"
#include "Common/Debug.h"

// デバッグビルドの時は、new演算子をオーバーライドして、メモリリークの検出を行う
#ifdef _DEBUG
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

// NVIDIA Optimus / AMD Dual Graphics 対応用
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;  // NVIDIA用
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // AMD用
}

// WinMain関数
//---------------------------------
int WINAPI WinMain(
	_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// インスタンスの生成
	Application::CreateInstance();

	// インスタンスの取得
	Application::GetInstance()->Init();

	if (Application::GetInstance()->IsInitFail())
	{
		// 初期化失敗
		return -1;
	}

	// 実行
	Application::GetInstance()->Run();

	// 解放
	Application::GetInstance()->Release();

	if (Application::GetInstance()->IsReleaseFail())
	{
		// 解放失敗
		return -1;
	}

	// インスタンスの削除
	Application::DeleteInstance();

	return 0;
}
