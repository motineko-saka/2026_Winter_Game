#pragma once
#include <vector>
#include "MonsterData.h"

// 経験値を加算したときの結果（演出やメッセージ表示に使う）
struct GrowthResult
{
	int gainedExp = 0;					// 実際に加算された経験値
	int oldLevel = 0;					// 加算前のレベル
	int newLevel = 0;					// 加算後のレベル
	std::vector<int> learnedMoves;		// 自動で覚えた技ID
	std::vector<int> pendingMoves;		// 覚えたいが技が4つ埋まっていて覚えられなかった技ID
	// （プレイヤーに忘れる技を選ばせて ReplaceMove を呼ぶ）
};

// 経験値・レベルアップ・進化を扱うクラス（状態を持たないのでstaticのみ）
class MonsterGrowth
{
public:

	// ---------- 経験値 ----------

	// そのレベルに到達するのに必要な累計経験値（レベル1は0）
	static int GetExpForLevel(GrowthRate rate, int level);

	// 次のレベルまでの残り経験値（最大レベルなら0）
	static int GetExpToNextLevel(const MonsterData& data, const MonsterInstance& monster);

	// 現在のレベル内での経験値の進み具合 0.0〜1.0（経験値バー用。最大レベルは1.0）
	static float GetExpRatio(const MonsterData& data, const MonsterInstance& monster);

	// 倒した相手から得られる経験値（isTrainerがtrueなら1.5倍）
	static int CalcGainExp(const MonsterMasterData& defeated, int defeatedLevel, bool isTrainer);

	// ---------- レベルアップ ----------

	// 経験値を加算し、必要ならレベルアップする。
	// ・レベルが上がるとHPの増えた分だけ現在HPも増える（ひんし状態は0のまま）
	// ・そのレベルで覚える技は、空きスロットがあれば自動で覚える
	// ※ひんしの個体に経験値を入れるかどうかは呼び出し側で決める
	static GrowthResult GainExp(const MonsterData& data, MonsterInstance& monster, int amount);

	// 技の入れ替え（slotIndex: 0〜3）。すでに覚えている技や存在しない技はfalse
	static bool ReplaceMove(const MonsterData& data, MonsterInstance& monster, int slotIndex, int newMoveId);

	// ---------- 進化 ----------

	// 今の状態で進化できる先を探す（なければnullptr）。
	// itemIdが0ならレベル進化、アイテムを使った場合はそのアイテムIDを渡す。
	static const Evolution* FindEvolution(const MonsterData& data, const MonsterInstance& monster, int itemId = 0);

	// 進化させる（図鑑番号の変更。ニックネームが種族名のままなら新しい種族名にする）
	static bool Evolve(const MonsterData& data, MonsterInstance& monster, int toId);
};