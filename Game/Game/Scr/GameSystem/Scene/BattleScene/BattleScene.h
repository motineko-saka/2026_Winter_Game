#pragma once

#include <array>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include "../SceneBase.h"
#include "../../Object/Monster/MonsterData.h"	

class MonsterParty;

// 野生モンスターとの1対1バトルシーン
//
// 使い方（フィールド側）：
//   auto battle = std::make_shared<BattleScene>();
//   battle->Setup(&monsterData, &monsterParty, 野生の図鑑番号, レベル);
//   SceneManager::GetInstance()->PushScene(battle);
//   ※終了後の結果は battle->GetResult() で取れる（PopScene後もshared_ptrを持っていれば）
class BattleScene : public SceneBase
{
public:

	// バトルの結果
	enum class Result
	{
		NONE,		// 戦闘中
		WIN,		// 勝った
		LOSE,		// 全滅した
		ESCAPED,	// 逃げた（開始できなかった場合も含む）
	};

	BattleScene(void);				// コンストラクタ
	~BattleScene(void) override;	// デストラクタ

public:

	void Init(void)		override;	// 初期化
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の処理
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

public:

	// バトルの準備（PushScene の前に呼ぶ）
	void Setup(const MonsterData* data, MonsterParty* party, int wildMonsterId, int wildLevel);

	Result GetResult(void) const { return result_; }

private:

	// ---------- 内部で使う型 ----------

	enum class Phase
	{
		MESSAGE,		// メッセージ表示中（Enterで次へ）
		COMMAND,		// たたかう／いれかえ／にげる
		MOVE_SELECT,	// 技選択
		PARTY_SELECT,	// 交代先選択
		LEARN_SELECT,	// 技を忘れて新しい技を覚えるか選ぶ
		END,			// 終了（Enterでシーンを閉じる）
	};

	enum class ActionType
	{
		MOVE,
		SWITCH,
		RUN,
	};

	// 戦闘中だけ持つ情報（ランク補正など）
	struct Combatant
	{
		MonsterInstance* mon = nullptr;
		const MonsterMasterData* master = nullptr;
		int atkRank = 0;		// -6〜+6
		int defRank = 0;		// -6〜+6
		int sleepTurns = 0;		// 睡眠の残りターン（戦闘中のみ）
		float dispHp = 0.0f;	// HPバーの表示用（なめらかに減らす）
	};

	// 1ステップ＝1メッセージ分の処理
	using Step = std::function<void(void)>;

private:

	// ---------- 進行管理 ----------
	void Say(const std::string& text);		// メッセージを表示する（このステップはここで止まる）
	void Push(Step step);					// 今実行中のステップの直後に処理を差し込む
	void PushSay(const std::string& text);
	void PushFaint(int side);
	void RunNextStep(void);					// 次のステップを実行（メッセージが出るまで進める）
	void StartMessages(void);				// MESSAGEフェーズに入って最初のステップを実行
	void Finish(void);						// ステップが尽きたとき、次のフェーズを決める

	// ---------- ターン処理 ----------
	void BeginTurn(ActionType type, int arg);
	int ChooseEnemyMove(void);				// -1ならわるあがき
	int GetPriority(int side, int slot) const;
	void ExecMove(int atk, int slot);
	void ApplyEffect(int atk, const MoveData& move, int dealt);
	void ChangeRank(int side, bool isAttack, int delta);
	void EndOfTurnDamage(int side);
	void OnFaint(int side);
	void GainPlayerExp(int amount);
	void SwitchPlayer(int partyIndex);
	void SetCombatant(int side, MonsterInstance* mon);

	// ---------- 入力 ----------
	void UpdateCommand(void);
	void UpdateMoveSelect(void);
	void UpdatePartySelect(void);
	void UpdateLearnSelect(void);
	void MoveCursor(int count);
	bool HasUsableMove(void) const;
	void UpdateHpBars(void);

	// ---------- 描画 ----------
	void DrawStatusBox(int side, int x, int y) const;
	void DrawHpBar(int x, int y, int w, int h, float ratio) const;
	void DrawMessageBox(const std::string& text) const;
	void DrawCommandMenu(void) const;
	void DrawMoveMenu(void) const;
	void DrawPartyMenu(void) const;
	void DrawLearnMenu(void) const;

	// ---------- 補助 ----------
	std::string Name(int side) const;
	int Rand(int lo, int hi);				// lo〜hi（両端を含む）

private:

	static const int PLAYER = 0;
	static const int ENEMY = 1;

	// 外部から渡されるもの
	const MonsterData* data_ = nullptr;
	MonsterParty* party_ = nullptr;
	int wildId_ = 0;
	int wildLevel_ = 1;

	// 戦闘の状態
	MonsterInstance enemy_{};
	std::array<Combatant, 2> side_{};
	int activeIndex_ = 0;			// 場に出ている手持ちの番号
	MoveData struggle_{};			// わるあがき（PPが尽きたとき）
	Phase phase_ = Phase::MESSAGE;
	Result result_ = Result::NONE;
	int runAttempts_ = 0;

	// メッセージ進行
	std::string msg_;
	bool said_ = false;
	std::deque<Step> steps_;
	size_t insertIdx_ = 0;
	bool needSwitch_ = false;		// ひんしになったので交代先を選ばせる
	std::deque<int> learnQueue_;	// 覚えたいが枠が埋まっている技

	int cursor_ = 0;

	std::mt19937 rng_;
};