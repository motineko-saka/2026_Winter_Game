#pragma once

#include <memory>
#include "../SceneBase.h"
#include "../../Object/Monster/MonsterData.h"
#include "../../Object/Monster/MonsterParty.h"
#include "../../Object/Item/ItemData.h"
#include "../../Object/Item/Inventory.h"

class Player;
class BattleScene;

class GameScene : public SceneBase
{
public:

	GameScene(void);				// コンストラクタ
	~GameScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の処理
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

private:

	bool SaveGame(void);						// セーブする（成功したらtrue）
	void StartBattle(int monsterId, int level);	// バトルシーンを積む
	void CheckBattleResult(void);				// バトル終了後の結果を受け取って処理する
	void StartDefeat(void);						// 全滅演出を始める
	void UpdateDefeat(void);					// 全滅演出の更新
	void DrawDefeat(void) const;				// 全滅演出の描画
	void RecoverFromDefeat(void);				// 全回復して開始位置に戻す
	void HealParty(void);						// 手持ちを全回復する

private:

	// 全滅演出の進行
	enum class DefeatPhase
	{
		NONE,		// 演出なし
		FADE_OUT,	// 画面が暗くなる
		MESSAGE,	// 暗転中にメッセージ表示（Enterで次へ）
		FADE_IN,	// 画面が明るくなる
	};

	static const int DEFEAT_FADE_FRAMES = 30;	// 暗転・明転にかけるフレーム数

	int mainScreenId_ = 0;

	Player* player_ = nullptr;

	MonsterData monsterData_;
	MonsterParty monsterParty_;

	ItemData itemData_;			// アイテムのマスターデータ
	Inventory inventory_;		// 所持アイテム（バッグ）

	// 進行中のバトル（終了後に結果を読むため、PopSceneされても持っておく）
	std::shared_ptr<BattleScene> battle_;

	// 全滅演出
	DefeatPhase defeatPhase_ = DefeatPhase::NONE;
	int defeatTimer_ = 0;
};