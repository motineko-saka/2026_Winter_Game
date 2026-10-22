#pragma once

#include "../SceneBase.h"

class TitleScene : public SceneBase
{
public:

	TitleScene(void);				// コンストラクタ
	~TitleScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の処理
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

private:

	enum class Menu
	{
		NEW_GAME,		// はじめから
		CONTINUE,		// つづきから
		MAX
	};

	void Decide(void);	// 選んだ項目を実行する

	int image_ = -1;
	int cursor_ = 0;	// 選んでいる項目（Menu の値）
};