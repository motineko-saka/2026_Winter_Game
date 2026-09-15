#pragma once

#include<memory>
#include<list>

class SceneBase;

class SceneManager
{
public:
	static constexpr float DEFAULT_FPS = 60.0f;
public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new SceneManager(); } };
	static SceneManager* GetInstance(void) { return instance_; };
	static void DeleteInstance(void) { if (instance_ != nullptr) { delete instance_; instance_ = nullptr; } }

private:
	// デフォルトコンストラクタをprivateにして、
	// 外部から生成できない様にする
	SceneManager(void);
	// デストラクタも同様
	~SceneManager(void);

	// コピー・ムーブ操作を禁止
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;
	SceneManager(SceneManager&&) = delete;
	SceneManager& operator=(SceneManager&&) = delete;

	// 下記をコンパイルエラーさせるため 上記を追加
	// SceneManager copy = *SceneManager::GetInstance();
	// SceneManager copied(*SceneManager::GetInstance());
	// SceneManager moved = std::move(*SceneManager::GetInstance());
public:

	void Init(void);	// 初期化
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

	// 状態遷移
	void ChangeScene(std::shared_ptr<SceneBase> scene);

	// シーンを新しく積む
	void PushScene(std::shared_ptr<SceneBase> scene);

	// 最後に追加したシーンを削除する。
	void PopScene(void);

	// 強制的に特定のシーンに飛ぶ。リセットをかけ特定のシーンのみにする。
	void JumpScene(std::shared_ptr<SceneBase> scene);

	// ゲーム終了
	void GameEnd(void) { isGameEnd_ = true; }

	// ゲーム終了取得
	bool GetGameEnd(void) { return isGameEnd_; }

private:

	// 静的インスタンス
	static SceneManager* instance_;

	//Drawの関係上Backを最新のシーンとする
	//基本的には要素は一つだけだがポーズシーンなどが積み重なる形
	std::list<std::shared_ptr<SceneBase>>scenes_;

	// ゲーム終了
	bool isGameEnd_;
};
