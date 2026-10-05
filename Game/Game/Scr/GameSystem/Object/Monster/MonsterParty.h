#pragma once
#include <vector>
#include "MonsterData.h"

// 手持ちモンスターの管理クラス
// ・手持ち（最大6匹）の追加／削除／入れ替え
// ・マスターデータからの個体生成（Create）
// ・能力値の計算、全回復
class MonsterParty
{
public:

	static const int PARTY_MAX = 6;		// 手持ちの最大数
	static const int LEVEL_MAX = 100;	// レベルの上限
	static const int MOVE_NONE = 0;		// 技スロットが空のときのID（技IDは1から始めること）

	MonsterParty();
	~MonsterParty();

	void Init(void);	// 初期化（手持ちを空にする）
	void Release(void);	// 解放

public:

	// ---------- 手持ちの操作 ----------

	bool Add(const MonsterInstance& monster);	// 末尾に追加（満員ならfalse）
	bool Remove(int index);						// 指定の位置を削除（後ろは詰める）
	bool Swap(int a, int b);					// 2匹の位置を入れ替える

	// ---------- 取得 ----------

	int GetCount(void) const;					// 手持ちの数
	bool IsFull(void) const;					// 満員か
	MonsterInstance* Get(int index);			// 範囲外ならnullptr
	const MonsterInstance* Get(int index) const;
	bool HasAlive(void) const;					// 戦えるモンスターがいるか
	MonsterInstance* GetFirstAlive(void);		// 先頭の戦えるモンスター（いなければnullptr）

	// ---------- 回復 ----------

	void HealAll(const MonsterData& data);		// 手持ち全員を全回復

public:

	// ---------- 個体の生成・能力値（状態を持たないのでstatic） ----------

	// マスターデータから個体を作る（IDが存在しなければfalse）
	// 覚えている技は「そのレベルまでに習得する技のうち、最後の4つ」
	static bool Create(const MonsterData& data, int monsterId, int level, MonsterInstance& out);

	// 1匹を全回復（HP・状態異常・PP）
	static void Heal(const MonsterData& data, MonsterInstance& monster);

	// 能力値の計算（個体値・努力値なしの簡易式）
	static int CalcMaxHp(const MonsterMasterData& master, int level);
	static int CalcAttack(const MonsterMasterData& master, int level);
	static int CalcDefense(const MonsterMasterData& master, int level);
	static int CalcSpeed(const MonsterMasterData& master, int level);

	// 個体の最大HP（マスターが見つからなければ0）
	static int GetMaxHp(const MonsterData& data, const MonsterInstance& monster);

public:

	// ---------- セーブ／ロード ----------

	// 手持ちをテキストファイルに保存する（保存先のフォルダは先に作っておくこと）
	bool Save(const std::string& path) const;

	// 手持ちをファイルから読み込む。
	// 失敗した場合（ファイルなし・壊れている等）はfalseを返し、今の手持ちは変更しない。
	bool Load(const std::string& path, const MonsterData& data);

	// 1匹分のセーブ用の1行（ボックスのセーブなど、他のクラスからも使う）
	static std::string ToSaveLine(const MonsterInstance& monster);
	static bool FromSaveLine(const std::string& line, const MonsterData& data, MonsterInstance& out);

private:

	std::vector<MonsterInstance> party_;	// 手持ち（先頭が先発）
};