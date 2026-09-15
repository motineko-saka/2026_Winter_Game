#include "SceneManager.h"

#include <DxLib.h>

#include "../Loading/Loading.h"
#include "../Scene/TitleScene/TitleScene.h"
#include "../Scene/GameScene/GameScene.h"

SceneManager* SceneManager::instance_ = nullptr;

// コンストラクタ
SceneManager::SceneManager(void)
{
	isGameEnd_ = false;
}

// デストラクタ
SceneManager::~SceneManager(void)
{
}

// 初期化
void SceneManager::Init(void)
{
	// ロード画面生成
	Loading::GetInstance()->CreateInstance();
	Loading::GetInstance()->Init();
	Loading::GetInstance()->Load();

	// 最初はタイトル画面から
	ChangeScene(std::make_shared<TitleScene>());
}

// 更新
void SceneManager::Update(void)
{
	// シーンがなければ終了
	if (scenes_.empty())
		return;

	// ロード中
	if (Loading::GetInstance()->IsLoading())
	{
		// ロード更新
		Loading::GetInstance()->Update();

		// ロードの更新が終了していたら
		if (Loading::GetInstance()->IsLoading() == false)
		{
			// ロード後の初期化
			scenes_.back()->LoadEnd();
		}
	}
	// 通常の更新処理
	else
	{
		// 現在のシーンの更新
		scenes_.back()->Update();
	}
}

// 描画
void SceneManager::Draw(void)
{
	// ロード中ならロード画面を描画
	if (Loading::GetInstance()->IsLoading())
	{
		// ロードの描画
		Loading::GetInstance()->Draw();
	}
	// 通常の更新
	else
	{
		// 積まれているもの全てを描画する
		for (auto& scene : scenes_)
		{
			// シーンの描画
			scene->Draw();
		}
	}
}

// 解放
void SceneManager::Release(void)
{
	//全てのシーンの解放・削除
	for (auto& scene : scenes_)
	{
		scene->Release();
	}
	scenes_.clear();

	// ロード画面の削除
	Loading::GetInstance()->Release();
	Loading::GetInstance()->DeleteInstance();
}

// 状態遷移関数
void SceneManager::ChangeScene(std::shared_ptr<SceneBase> scene)
{
	// シーンが空か？
	if (scenes_.empty())
	{
		//空なので新しく入れる
		scenes_.push_back(scene);
	}
	else
	{
		//末尾のものを新しい物に入れ替える
		scenes_.back() = scene;
	}

	// 読み込み(非同期)
	Loading::GetInstance()->StartAsyncLoad();
	scenes_.back()->Load();
	Loading::GetInstance()->EndAsyncLoad();
}

void SceneManager::PushScene(std::shared_ptr<SceneBase> scene)
{
	//新しく積むのでもともと入っている奴はまだ削除されない
	scene->Init();
	scenes_.push_back(scene);
}

void SceneManager::PopScene(void)
{
	//積んであるものを消して、もともとあったものを末尾にする
	if (scenes_.size() > 1)
	{
		scenes_.pop_back();
	}
}

void SceneManager::JumpScene(std::shared_ptr<SceneBase> scene)
{
	// 全て解放
	scenes_.clear();

	// 新しく積む
	scenes_.push_back(scene);
}
