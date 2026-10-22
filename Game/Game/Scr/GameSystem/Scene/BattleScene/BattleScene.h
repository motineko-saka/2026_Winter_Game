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
#include "../../Object/Monster/BallEffectData.h"

class MonsterParty;
class ItemData;
class Inventory;
struct TrainerMasterData;

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

	// トレーナー戦の準備（PushScene の前に呼ぶ。Setup の代わりに使う）
	//   ・相手の手持ちが順番に出てくる／ボールは使えない／逃げられない
	void SetupTrainer(const MonsterData* data, MonsterParty* party, const TrainerMasterData* trainer);

	// 勝ったときにもらえる賞金（トレーナー戦のみ。WINのあとに読む）
	int GetPrize(void) const { return prize_; }

	// モンスターを出すときのボールの種類（BallEffect.csv の id）。PushScene の前に呼ぶ
	//   MonsterInstance にボールIDを持たせたら、GetBallId の中身をそちらに差し替える
	//   ボールを投げて捕まえるときは、BallEffect.csv の id に「そのボールのアイテムID」を書いた行の演出になる
	//   （無ければここで決めたボール、それも無ければ仮のボール）
	void SetBall(int playerBallId, int enemyBallId = 1) { playerBallId_ = playerBallId; enemyBallId_ = enemyBallId; }

	// ボールを投げるプレイヤーのスプライトシート（任意。PushScene の前に呼ぶ）
	//   投げるモーションを frames コマ並べた画像。未設定ならプレイヤーは描かない
	void SetPlayerSprite(const std::string& path, int frames, int columns, int frameW, int frameH);

	// 投げモーションが1枚ずつ別の画像のとき（任意。順番に再生される）
	void SetPlayerImages(const std::vector<std::string>& paths) { playerImagePaths_ = paths; }

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

	// 技の演出の進行（前に出る → エフェクト → 食らう → 戻る）
	enum class MoveAnimStage
	{
		LUNGE,		// 技を使う側が、少し前に出る
		EFFECT,		// 技エフェクトの再生
		HIT,		// 技を受けた側が、赤く点滅して左右に揺れる
		RETURN,		// 技を使った側が、元の位置に戻る
	};

	// 再生中の技の演出
	struct MoveAnimPlay
	{
		bool active = false;
		MoveAnimStage stage = MoveAnimStage::LUNGE;
		const MoveAnimData* anim = nullptr;			// nullptrなら画像エフェクトなし
		const std::vector<int>* frames = nullptr;	// 分割済みの画像ハンドル
		int attacker = 0;		// 技を使った側（PLAYER / ENEMY）
		int side = 0;			// エフェクトを描く側（PLAYER / ENEMY）
		bool lunge = false;		// 前に出る演出をするか（攻撃技のみ）
		bool hit = false;		// 相手にダメージが入ったか（入ったら食らう演出をする）
		int timer = 0;			// 今のステージの経過フレーム
	};

	// モンスターを出す演出の進行（投げる → バウンド → 開く → 出てくる）
	enum class SendOutStage
	{
		THROW,		// ボールが弧を描いて飛ぶ（プレイヤーは投げモーション）
		BOUNCE,		// 地面で何回かバウンドする
		OPEN,		// ボールが開いてエフェクトが出る
		EMERGE,		// モンスターが中央から光りながら大きくなって出てくる
	};

	struct SendOutPlay
	{
		bool active = false;
		SendOutStage stage = SendOutStage::THROW;
		int side = 0;
		int timer = 0;
		const BallEffectMaster* ball = nullptr;		// このボールの設定（CSV）
		int ballImg = -1;
		int openImg = -1;
		const std::vector<int>* effectFrames = nullptr;	// 開いたときのエフェクト（分割済み）
	};

	// 捕まえる演出の進行（投げる → 吸い込む → 落ちる → ゆれる → 成功／失敗）
	enum class CatchStage
	{
		THROW,		// ボールが手前から飛んでくる
		ABSORB,		// ボールが開いて、モンスターが光になって吸い込まれる
		DROP,		// ボールが地面に落ちてバウンドする
		SHAKE,		// ボールがゆれる（回数は捕獲判定の結果）
		SUCCESS,	// 捕まえた：光と星が広がる
		FAIL,		// 出てきてしまった：ボールが開いてモンスターが出てくる
	};

	struct CatchPlay
	{
		bool active = false;		// 演出中
		bool hold = false;			// 捕まえたあと、ボールが地面に残っている
		CatchStage stage = CatchStage::THROW;
		int timer = 0;				// 今のステージの経過フレーム
		int shakes = 0;				// ゆれる回数（0〜3）
		bool caught = false;		// 捕まえられたか
		const BallEffectMaster* ball = nullptr;			// このボールの設定（CSV）
		int ballImg = -1;
		int openImg = -1;
		const std::vector<int>* openFrames = nullptr;	// 吸い込み・出てくるときのエフェクト（モンスターを出す演出と同じ）
		const std::vector<int>* catchFrames = nullptr;	// 捕まえたときの光
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
	void TryCatch(int itemId, double ballRate, const std::string& ballName);	// ボールを投げて捕獲判定をする
	void UseItem(int itemId, int partyIndex);	// アイテムを使う（ターンの中で実行される）
	void RefreshBagList(void);				// バッグに出す（戦闘中に使える）アイテムの一覧を作り直す
	void ApplyEffect(int atk, const MoveData& move, int dealt);
	void ChangeRank(int side, bool isAttack, int delta);
	void EndOfTurnDamage(int side);
	void OnFaint(int side);
	bool CreateEnemy(void);					// 相手のモンスターを作る（野生なら1匹、トレーナーなら手持ち全部）
	bool HasNextEnemy(void) const;			// トレーナーの手持ちに、次に出せるモンスターがいるか
	void SendNextEnemy(void);				// トレーナーが次のモンスターを出す
	void OnTrainerDefeated(void);			// トレーナーに勝ったときのメッセージと賞金
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
	void StartMoveAnim(const MoveData& move, int atk);	// 技のエフェクトを始める
	void UpdateMoveAnim(void);							// エフェクトの更新
	void DrawMoveAnim(void) const;						// エフェクトの描画
	int MoveAnimStageFrames(void) const;				// 今の演出ステージの長さ（0なら飛ばす）
	void GetMonsterAnimOffset(int side, int& ox, int& oy, bool& red) const;	// モンスター画像のずれ（前に出る・揺れる）と赤点滅
	const std::vector<int>* LoadAnimFrames(const MoveAnimData& anim);	// シートを分割して読み込む（キャッシュあり）

	// ---------- モンスターを出す演出 ----------
	void StartSendOut(int side);				// ボールを投げて出す演出を始める（出るまでモンスターは隠れる）
	void UpdateSendOut(void);
	void DrawSendOut(void) const;
	void DrawBallSprite(int x, int y, int size, double angle, int alpha, int img) const;
	void DrawThrower(void) const;				// ボールを投げるプレイヤー
	int SendOutStageFrames(void) const;			// 今のステージの長さ
	int GetBallId(int side) const { return (side == PLAYER) ? playerBallId_ : enemyBallId_; }
	void GetMonsterLayout(int side, int& cx, int& bottomY, int& size) const;	// モンスター画像の配置（DrawとDrawSendOutで共通）
	const std::vector<int>* LoadBallFrames(const BallEffectMaster& ball);	// エフェクトのシートを分割して読み込む（キャッシュあり）

	// ---------- 捕まえる演出 ----------
	void StartCatch(int itemId, int shakes, bool caught);	// ボールを投げて捕まえる演出を始める（終わるまでEnterは効かない）
	void UpdateCatch(void);
	void DrawCatch(void) const;
	int CatchStageFrames(void) const;						// 今のステージの長さ
	void DrawOpenEffect(int x, int y, int timer, float t, int size, const std::vector<int>* frames, const BallEffectMaster& ball) const;	// ボールが開いたときのエフェクト
	void DrawCatchSparkle(int x, int y, float t, const BallEffectMaster& ball) const;	// 捕まえたときの簡易エフェクト（星と光の輪）
	const BallEffectMaster* GetCatchBall(int itemId) const;	// 投げるボールの設定を決める
	const std::vector<int>* LoadCatchFrames(const BallEffectMaster& ball);	// 捕まえたときの光のシートを読み込む（キャッシュあり）

	// ---------- 補助 ----------
	int LoadMonsterImage(const std::string& path);	// パスから画像を読み込む（同じパスはキャッシュを返す）
	int LoadTrainerPortrait(const std::string& spec);	// Trainer.csv の画像指定から、トレーナーの立ち絵（下向き・立ち）を1枚読み込む
	void DrawTrainerPortrait(void) const;					// トレーナーの立ち絵を、相手モンスターの位置に描く
	std::string Name(int side) const;
	int Rand(int lo, int hi);				// lo〜hi（両端を含む）
	int SX(int x) const { return x * screenW_ / BASE_W; }	// 640x480基準のX座標を実画面に合わせる
	int SY(int y) const { return y * screenH_ / BASE_H; }	// 640x480基準のY座標を実画面に合わせる

private:

	static const int PLAYER = 0;
	static const int ENEMY = 1;

	static const int EVOLVE_FRAMES = 240;	// 進化演出の長さ（60fpsで約4秒）
	static const int MOVE_LUNGE_FRAMES = 10;	// 技を出す前に前へ出る（戻る）フレーム数
	static const int MOVE_HIT_FRAMES = 30;		// 技を食らったときの点滅・揺れのフレーム数
	static const int SENDOUT_THROW_FRAMES = 40;		// ボールが飛ぶ長さ
	static const int SENDOUT_BOUNCE_FRAMES = 14;	// 1バウンドの長さ
	static const int SENDOUT_EMERGE_FRAMES = 24;	// モンスターが出てくる長さ
	static const int SENDOUT_OPEN_MIN_FRAMES = 20;	// ボールが開く演出の最低の長さ
	static const int CATCH_THROW_FRAMES = 36;			// 捕まえる：ボールが飛ぶ長さ
	static const int CATCH_ABSORB_MIN_FRAMES = 40;		// 捕まえる：吸い込む長さ（エフェクトが長ければそちらに合わせる）
	static const int CATCH_DROP_FRAMES = 30;			// 捕まえる：ボールが落ちてバウンドする長さ
	static const int CATCH_FALL_FRAMES = 16;			// 　うち、落ちるまでの長さ
	static const int CATCH_SHAKE_WAIT_FRAMES = 20;		// 捕まえる：落ちてから、ゆれはじめるまでの間
	static const int CATCH_SHAKE_FRAMES = 32;			// 捕まえる：1回ゆれる長さ
	static const int CATCH_SHAKE_END_FRAMES = 16;		// 捕まえる：最後にゆれてから、結果が出るまでの間
	static const int CATCH_SUCCESS_MIN_FRAMES = 60;		// 捕まえる：成功の光の長さ
	static const int CATCH_FAIL_MIN_FRAMES = 40;		// 捕まえる：失敗してモンスターが出てくる長さ
	static constexpr const char* BALL_EFFECT_CSV = "Data/Csv/BallEffect.csv";	// ボール演出の外部ファイル

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

	// トレーナー戦（野生のときは trainer_ が nullptr）
	const TrainerMasterData* trainer_ = nullptr;
	std::vector<MonsterInstance> enemyParty_;	// トレーナーの手持ち
	int enemyIndex_ = 0;						// 今出ている手持ちの番号
	int prize_ = 0;								// もらった賞金
	int trainerImg_ = -1;							// トレーナーの立ち絵（画像ハンドル。images_ が持つので Release で消える）
	bool showTrainer_ = false;						// 立ち絵を表示中か（戦闘の最初と、勝ったあと）

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

	// モンスターを出す演出
	BallEffectData ballData_;
	BallEffectMaster defaultBall_{};				// CSVに無いときの仮のボール
	SendOutPlay sendOut_;
	std::array<bool, 2> hidden_{};					// 出てくるまでモンスターと情報枠を隠す
	int playerBallId_ = 1;
	int enemyBallId_ = 1;
	std::unordered_map<int, std::vector<int>> ballFrames_;	// ボールID→エフェクトの画像ハンドル
	std::string playerSpritePath_;
	int playerSpriteFrames_ = 0;
	int playerSpriteColumns_ = 1;
	int playerSpriteW_ = 0;
	int playerSpriteH_ = 0;
	std::vector<std::string> playerImagePaths_;		// 1枚ずつ別画像で渡されたときのパス
	std::vector<int> playerSprite_;					// 投げモーションの画像ハンドル（シート分割でも個別画像でもここに入る）

	// 捕まえる演出
	CatchPlay catch_;
	std::unordered_map<int, std::vector<int>> catchFrames_;	// ボールID→捕まえたときの光の画像ハンドル

	MoveAnimPlay moveAnim_;
	std::unordered_map<int, std::vector<int>> animFrames_;	// アニメID→分割した画像ハンドル
};