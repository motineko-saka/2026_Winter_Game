#pragma once

#include "../../Common/Vector2.h"

class Loading
{
public:
	// シングルトン（生成・取得・削除）
	static void CreateInstance(void) { if (instance_ == nullptr) { instance_ = new Loading(); } };
	static Loading* GetInstance(void) { return instance_; };
	static void DeleteInstance(void) { if (instance_ != nullptr) { delete instance_; instance_ = nullptr; } }

private:
	// デフォルトコンストラクタをprivateにして、
	// 外部から生成できない様にする
	Loading();
	~Loading();

	// コピー・ムーブ操作を禁止
	Loading(const Loading&) = delete;
	Loading& operator=(const Loading&) = delete;
	Loading(Loading&&) = delete;
	Loading& operator=(Loading&&) = delete;

	// 下記をコンパイルエラーさせるため 上記を追加
	// Loading copy = *Loading::GetInstance();
	// Loading copied(*Loading::GetInstance());
	// Loading moved = std::move(*Loading::GetInstance());
public:

	void Init(void);		// 初期化
	void Load(void);		// 読み込み
	void Update(void);		// 更新
	void Draw(void);		// 描画
	void Release(void);		// 解放

	void StartAsyncLoad(void);	// 非同期ロードの開始
	void EndAsyncLoad(void);	// 非同期ロードの終了

	// ロード中かを返す。
	bool IsLoading(void) { return isLoading_; }

private:

	// 静的インスタンス
	static Loading* instance_;

	// 最低でもロード画面を表示する時間の範囲
	int loadTimer_;

	// 画像ハンドル
	int handle_;

	// 座標
	Vector2 pos_;

	// ロード中の判定用
	bool isLoading_;

};
