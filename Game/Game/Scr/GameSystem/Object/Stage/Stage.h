#pragma once

class Stage
{
public:

	Stage();            // コンストラクタ
	~Stage();           // デストラクタ

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void LoadEnd(void);	// 読み込み後の処理
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

private:

};
