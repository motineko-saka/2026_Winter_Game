#pragma once

#include <array>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "../SceneBase.h"
#include "../../Object/Monster/MonsterData.h"	

class MonsterParty;
class ItemData;
class Inventory;

// 野生モンスターとの1対1バトルシーン
//
// 使い方（フィールド側）：
//   auto battle = std::make_shared<BattleScene>();
//   battle->Setup(&monsterData, &monsterParty, 野生の図鑑番号, レベル);
//   battle->SetItems(&itemData, &inventory);	// バッグを使うなら（省略するとバッグは空扱い）
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
		CAUGHT,		// 捕まえた（野生モンスターは手持ちに加わっている）
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

	// バッグ（アイテム）を使えるようにする（PushScene の前に呼ぶ）
	void SetItems(const ItemData* items, Inventory* inventory);

private:

	// ---------- 内部で使う型 ----------

	enum class Phase
	{
		MESSAGE,		// メッセージ表示中（Enterで次へ）
		COMMAND,		// たたかう／いれかえ／にげる
		MOVE_SELECT,	// 技選択
		PARTY_SELECT,	// 交代先選択
		LEARN_SELECT,	// 技を忘れて新しい技を覚えるか選ぶ
		BAG,			// バッグ（使うアイテムを選ぶ）
		ITEM_TARGET,	// アイテムを使う手持ちを選ぶ
		EVOLVING,		// 進化演出中（BACKでキャンセルできる）
		EVOLVE_SELECT,	// 進化先を選ぶ（分岐進化）
		END,			// 終了（Enterでシーンを閉じる）
	};

	enum class ActionType
	{
		MOVE,
		SWITCH,
		RUN,
		ITEM,
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
		int image = -1;			// 表示する画像ハンドル（敵＝正面、味方＝背面）
	};

	// 進化の進行状況
	enum class EvolveState
	{
		NONE,		// 進化していない
		INTRO,		// 「おや…？」のメッセージ中
		SELECT,		// 進化先を選択中（分岐進化のとき）
		ANIM,		// 進化演出中（BACKでキャンセルできる）
		RESULT,		// 進化した／止まったのメッセージ中
	};

	// 進化待ちの個体
	struct EvolveEntry
	{
		MonsterInstance* mon = nullptr;
		int toId = 0;			// 進化先の図鑑番号（選ばれたもの）
		std::vector<int> candidates;	// 進化できる先の候補（2つ以上なら選ばせる）
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
	bool CanCatch(std::string& reason) const;	// 今ボールを投げられるか（だめなら理由が入る）
	void TryCatch(double ballRate, const std::string& ballName);	// ボールを投げて捕獲判定をする
	void UseItem(int itemId, int partyIndex);	// アイテムを使う（ターンの中で実行される）
	void RefreshBagList(void);				// バッグに出す（戦闘中に使える）アイテムの一覧を作り直す
	void ApplyEffect(int atk, const MoveData& move, int dealt);
	void ChangeRank(int side, bool isAttack, int delta);
	void EndOfTurnDamage(int side);
	void OnFaint(int side);
	void GainPlayerExp(int amount);
	void CheckEvolution(MonsterInstance& mon, int level);	// レベルアップ後に進化できるか調べて、できれば待ち行列へ
	bool StartNextEvolve(void);				// 待ち行列から、始められる進化を探して始める（なければfalse）
	bool StartEvolve(void);					// 待ち行列の先頭の進化を始める（始められなければfalse）
	void ApplyEvolution(MonsterInstance& mon, int toId);	// 種族・HP・名前などを進化後に書き換える
	void BeginEvolveAnim(void);				// 進化演出を始める
	void CancelEvolve(void);				// 進化をキャンセルする
	void LearnEvolutionMoves(MonsterInstance& mon);	// 進化後の種族がそのレベルで覚える技を習得させる
	void SwitchPlayer(int partyIndex);
	void SetCombatant(int side, MonsterInstance* mon);

	// ---------- 入力 ----------
	void UpdateCommand(void);
	void UpdateMoveSelect(void);
	void UpdatePartySelect(void);
	void UpdateLearnSelect(void);
	void UpdateBag(void);
	void UpdateItemTarget(void);
	void UpdateEvolve(void);
	void UpdateEvolveSelect(void);
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
	void DrawBagMenu(void) const;
	void DrawEvolve(void) const;
	void DrawMonsterImage(int side, int cx, int bottomY, int size) const;
	void DrawImageFit(int img, int cx, int bottomY, int size) const;	// 画像ハンドルを指定して描く

	// ---------- 補助 ----------
	int LoadMonsterImage(const std::string& path);	// パスから画像を読み込む（同じパスはキャッシュを返す）
	std::string Name(int side) const;
	int Rand(int lo, int hi);				// lo〜hi（両端を含む）
	int SX(int x) const { return x * screenW_ / BASE_W; }	// 640x480基準のX座標を実画面に合わせる
	int SY(int y) const { return y * screenH_ / BASE_H; }	// 640x480基準のY座標を実画面に合わせる

private:

	static const int PLAYER = 0;
	static const int ENEMY = 1;

	static const int EVOLVE_FRAMES = 240;	// 進化演出の長さ（60fpsで約4秒）

	// レイアウトの設計基準サイズ（この値を基準に実画面へ拡縮する）
	static const int BASE_W = 640;
	static const int BASE_H = 480;
	int screenW_ = BASE_W;			// 実際の画面サイズ（Drawの頭で更新）
	int screenH_ = BASE_H;

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
	const ItemData* items_ = nullptr;	// アイテムのマスターデータ（外部から渡される）
	Inventory* inventory_ = nullptr;	// 所持アイテム（外部から渡される）
	std::vector<int> bagList_;			// バッグに表示するアイテムID（戦闘中に使えるものだけ）
	int usingItemId_ = 0;				// 使おうとしているアイテム（対象選択中）
	int itemTarget_ = 0;				// アイテムを使う手持ちの番号
	bool enemyInBall_ = false;		// 野生モンスターがボールに入っている間は画像を消す

	// メッセージ進行
	std::string msg_;
	bool said_ = false;
	std::deque<Step> steps_;
	size_t insertIdx_ = 0;
	bool needSwitch_ = false;		// ひんしになったので交代先を選ばせる
	std::deque<int> learnQueue_;	// 覚えたいが枠が埋まっている技

	// 進化
	std::deque<EvolveEntry> evolveQueue_;			// 進化待ち（戦闘後に順番に処理する）
	EvolveState evolveState_ = EvolveState::NONE;
	bool evolveBegun_ = false;						// 戦闘終了後の進化の処理に入ったか
	EvolveEntry evolveCur_{};						// 今進化させようとしている個体
	int evolveTimer_ = 0;							// 演出の経過フレーム
	int evolveFlip_ = 0;							// 次に画像を切り替えるまでのフレーム
	bool evolveShowNew_ = false;					// 演出中、進化後の画像を出しているか
	bool evolved_ = false;							// 進化が確定したか（結果表示用）
	int evolveOldImg_ = -1;							// 進化前の正面画像
	int evolveNewImg_ = -1;							// 進化後の正面画像
	std::vector<int> evolveCandImgs_;				// 進化先候補の正面画像（選択画面のプレビュー用）

	int cursor_ = 0;

	std::unordered_map<std::string, int> images_;	// パス→画像ハンドルのキャッシュ

	std::mt19937 rng_;
};