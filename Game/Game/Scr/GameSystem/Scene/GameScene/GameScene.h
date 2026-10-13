#pragma once

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "../SceneBase.h"
#include "../../Object/Monster/MonsterData.h"
#include "../../Object/Monster/MonsterParty.h"
#include "../../Object/Item/ItemData.h"
#include "../../Object/Item/Inventory.h"
#include "../../Object/NPC/TrainerData.h"

class Player;
class BattleScene;

// トレーナーの画像（向きごとに、立ち絵＋歩きのコマを持つ）
//   frames[向き] … 向きは TrainerDir の順（UP,DOWN,LEFT,RIGHT）。先頭のコマが立ち絵。
struct TrainerSprite
{
	std::array<std::vector<int>, 4> frames;
	std::array<bool, 4> mirror{};		// true の向きは、左右反転して描く（左向きは右向きの画像を反転して使う）
};

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

	// ---------- トレーナー ----------
	void StartTrainerEvent(const TrainerSpawn& spawn, int tileX, int tileY, int gap);	// 見つかった演出（！→歩いてくる）を始める
	void UpdateTrainerEvent(void);							// 見つかった演出の更新
	void FinishTrainerEvent(void);							// 演出が終わったので、トレーナー戦を始める
	void GetTrainerTile(const TrainerSpawn& spawn, int& tileX, int& tileY) const;	// トレーナーが今立っているマス
	void StartTrainerBattle(const TrainerSpawn& spawn);		// トレーナー戦を始める
	bool CheckTrainerSight(void);							// プレイヤーがトレーナーに見つかったか（見つかったら戦闘開始）
	void RebuildTrainerBlocks(void);						// トレーナーが立っているマスを通行不可にする
	void DrawTrainers(void);								// マップ上のトレーナーを描く
	const TrainerSprite& GetTrainerSprite(const std::string& spec);	// 画像の指定から、向きごとの画像を読み込む（同じ指定はキャッシュを返す）

	bool SaveGame(void);						// セーブする（成功したらtrue）
	void StartBattle(int monsterId, int level);	// バトルシーンを積む
	void StartStory(const std::string& scriptPath);	// ストーリー（ノベルパート）を再生する
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

	// トレーナー
	TrainerData trainerData_;						// トレーナーの定義と配置
	std::set<int> defeatedTrainers_;				// 倒したトレーナーのID
	int battleTrainerId_ = -1;						// 戦闘中のトレーナーのID（野生なら-1）
	int lastStageId_ = -1;							// 通行不可マスを設定したステージ
	std::map<std::string, TrainerSprite> trainerImages_;		// 画像の指定→向きごとの画像のキャッシュ

	// トレーナーに見つかったときの演出
	enum class TrainerEventPhase
	{
		NONE,		// 演出なし
		ALERT,		// 頭の上に「！」が出る
		WALK,		// プレイヤーの前まで歩いてくる
	};

	static const int TRAINER_ALERT_FRAMES = 40;		// 「！」を出しておくフレーム数
	static const int TRAINER_WALK_SPEED = 3;		// 歩く速さ（1フレームあたりのピクセル）

	TrainerEventPhase trainerEventPhase_ = TrainerEventPhase::NONE;
	TrainerSpawn eventSpawn_{};						// 見つけたトレーナー
	int eventStartX_ = 0;							// 歩き始めるマス(X)
	int eventStartY_ = 0;							// 歩き始めるマス(Y)
	int eventGap_ = 0;								// 歩くマス数
	int eventWalked_ = 0;							// 歩いたピクセル数
	int eventTimer_ = 0;

	// 歩いて動いたトレーナーの立っているマス（トレーナーID → マス）
	std::map<int, std::pair<int, int>> trainerTileOverride_;

	// 所持金（トレーナーに勝つと増える）
	int money_ = 0;

	// 全滅演出
	DefeatPhase defeatPhase_ = DefeatPhase::NONE;
	int defeatTimer_ = 0;
};