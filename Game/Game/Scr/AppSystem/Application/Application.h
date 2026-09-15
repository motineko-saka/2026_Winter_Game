#pragma once
#include <string>

// クラスの前方宣言
class FpsControl;

class Application
{
public:
	// スクリーンサイズ
	static constexpr int SCREEN_SIZE_X = 1280;
	static constexpr int SCREEN_SIZE_Y = 720;

	// データパス関連
	//-------------------------------------------
	static const std::string PATH_IMAGE;
	static const std::string PATH_DATA;
	static const std::string PATH_MAP_DATA;
	static const std::string PATH_IMG_PLAYER1;
	static const std::string PATH_IMG_PLAYER2;
	//-------------------------------------------

public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new Application(); } }
	static Application* GetInstance(void) { return instance_; }
	static void DeleteInstance(void) { if (instance_ != nullptr) delete instance_; instance_ = nullptr; }

public:

	void Init(void);	// 初期化
	void Run(void);		// ゲームループの開始
	void Release(void);	// 解放

	// 初期化成功／失敗の判定
	bool IsInitFail(void) const { return isInitFail_; };

	// 解放成功／失敗の判定
	bool IsReleaseFail(void) const { return isReleaseFail_; };

private:
	// デフォルトコンストラクタをprivateにして、
	// 外部から生成できない様にする
	Application(void);

	// デストラクタも同様
	~Application(void);

private:

	// 静的インスタンス
	static Application* instance_;

	// 初期化失敗
	bool isInitFail_;

	// 解放失敗
	bool isReleaseFail_;

	// FPS
	FpsControl* fps_;
};
