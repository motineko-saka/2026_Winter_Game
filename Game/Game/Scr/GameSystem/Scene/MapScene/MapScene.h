#pragma once

#include "../SceneBase.h"

class MapScene : public SceneBase
{
public:

	MapScene(void);				// コンストラクタ
	~MapScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の処理
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

private:

	int TITLE_SIZE_WID = 800;		// タイトル画像の横サイズ
	int TITLE_SIZE_HIG = 600;		// タイトル画像の縦サイズ

	int map_handle_ = -1;	// マップ画像のハンドル
};