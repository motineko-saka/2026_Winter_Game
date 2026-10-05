#include "MonsterGrowth.h"
#include <algorithm>
#include "MonsterParty.h"

namespace
{
	// 空いている技スロットの番号（なければ-1）
	int FindEmptySlot(const MonsterInstance& monster)
	{
		for (size_t i = 0; i < monster.moves.size(); i++)
		{
			if (monster.moves[i].moveId == MonsterParty::MOVE_NONE)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	bool KnowsMove(const MonsterInstance& monster, int moveId)
	{
		for (const auto& slot : monster.moves)
		{
			if (slot.moveId == moveId)
			{
				return true;
			}
		}
		return false;
	}

	bool Contains(const std::vector<int>& list, int value)
	{
		return std::find(list.begin(), list.end(), value) != list.end();
	}

	void SetMove(const MonsterData& data, MoveSlot& slot, int moveId)
	{
		const MoveData* move = data.GetMove(moveId);
		slot.moveId = moveId;
		slot.currentPp = (move != nullptr) ? move->pp : 0;
	}

	// 最大HPが変わった分だけ現在HPを増やす（ひんしは0のまま）
	void ApplyMaxHpChange(MonsterInstance& monster, int oldMaxHp, int newMaxHp)
	{
		if (monster.currentHp <= 0)
		{
			return;
		}
		monster.currentHp += newMaxHp - oldMaxHp;
		monster.currentHp = std::max(1, std::min(monster.currentHp, newMaxHp));
	}
}

int MonsterGrowth::GetExpForLevel(GrowthRate rate, int level)
{
	if (level <= 1)
	{
		return 0;
	}

	const int cube = level * level * level;
	switch (rate)
	{
	case GrowthRate::FAST:
		return cube * 4 / 5;
	case GrowthRate::SLOW:
		return cube * 5 / 4;
	case GrowthRate::MEDIUM:
	default:
		return cube;
	}
}

int MonsterGrowth::GetExpToNextLevel(const MonsterData& data, const MonsterInstance& monster)
{
	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	if (master == nullptr || monster.level >= MonsterParty::LEVEL_MAX)
	{
		return 0;
	}
	const int next = GetExpForLevel(master->growthRate, monster.level + 1);
	return std::max(0, next - monster.exp);
}

float MonsterGrowth::GetExpRatio(const MonsterData& data, const MonsterInstance& monster)
{
	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	if (master == nullptr)
	{
		return 0.0f;
	}
	if (monster.level >= MonsterParty::LEVEL_MAX)
	{
		return 1.0f;
	}

	const int cur = GetExpForLevel(master->growthRate, monster.level);
	const int next = GetExpForLevel(master->growthRate, monster.level + 1);
	if (next <= cur)
	{
		return 0.0f;
	}
	const float ratio = static_cast<float>(monster.exp - cur) / static_cast<float>(next - cur);
	return std::max(0.0f, std::min(ratio, 1.0f));
}

int MonsterGrowth::CalcGainExp(const MonsterMasterData& defeated, int defeatedLevel, bool isTrainer)
{
	int exp = defeated.baseExp * defeatedLevel / 7;
	if (isTrainer)
	{
		exp = exp * 3 / 2;
	}
	return std::max(1, exp);
}

GrowthResult MonsterGrowth::GainExp(const MonsterData& data, MonsterInstance& monster, int amount)
{
	GrowthResult result;
	result.oldLevel = monster.level;
	result.newLevel = monster.level;

	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	if (master == nullptr || amount <= 0 || monster.level >= MonsterParty::LEVEL_MAX)
	{
		return result;
	}

	// 経験値は最大レベルに必要な量までで止める
	const int maxLevel = MonsterParty::LEVEL_MAX;
	const int maxExp = GetExpForLevel(master->growthRate, maxLevel);
	const long long total = static_cast<long long>(monster.exp) + amount;
	const int newExp = static_cast<int>(std::min(total, static_cast<long long>(maxExp)));
	result.gainedExp = std::max(0, newExp - monster.exp);
	monster.exp = std::max(monster.exp, newExp);

	const int oldMaxHp = MonsterParty::CalcMaxHp(*master, monster.level);

	// 必要経験値を超えている間、レベルを上げる（一気に複数上がることもある）
	while (monster.level < maxLevel
		&& monster.exp >= GetExpForLevel(master->growthRate, monster.level + 1))
	{
		monster.level++;

		// このレベルで覚える技
		for (const auto& entry : master->learnset)
		{
			if (entry.level != monster.level)
			{
				continue;
			}
			if (KnowsMove(monster, entry.moveId)
				|| Contains(result.pendingMoves, entry.moveId))
			{
				continue;
			}

			const int slot = FindEmptySlot(monster);
			if (slot >= 0)
			{
				SetMove(data, monster.moves[slot], entry.moveId);
				result.learnedMoves.push_back(entry.moveId);
			}
			else
			{
				result.pendingMoves.push_back(entry.moveId);
			}
		}
	}

	result.newLevel = monster.level;
	if (result.newLevel > result.oldLevel)
	{
		ApplyMaxHpChange(monster, oldMaxHp, MonsterParty::CalcMaxHp(*master, monster.level));
	}
	return result;
}

bool MonsterGrowth::ReplaceMove(const MonsterData& data, MonsterInstance& monster, int slotIndex, int newMoveId)
{
	if (slotIndex < 0 || slotIndex >= static_cast<int>(monster.moves.size()))
	{
		return false;
	}
	if (data.GetMove(newMoveId) == nullptr || KnowsMove(monster, newMoveId))
	{
		return false;
	}
	SetMove(data, monster.moves[slotIndex], newMoveId);
	return true;
}

const Evolution* MonsterGrowth::FindEvolution(const MonsterData& data, const MonsterInstance& monster, int itemId)
{
	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	if (master == nullptr)
	{
		return nullptr;
	}

	for (const auto& evo : master->evolutions)
	{
		if (evo.itemId != itemId)
		{
			continue;	// 必要なアイテムが違う（レベル進化はitemId=0同士で一致）
		}
		if (monster.level < evo.level)
		{
			continue;	// レベル不足
		}
		return &evo;
	}
	return nullptr;
}

bool MonsterGrowth::Evolve(const MonsterData& data, MonsterInstance& monster, int toId)
{
	const MonsterMasterData* from = data.GetMonster(monster.monsterId);
	const MonsterMasterData* to = data.GetMonster(toId);
	if (from == nullptr || to == nullptr)
	{
		return false;
	}

	const int oldMaxHp = MonsterParty::CalcMaxHp(*from, monster.level);

	// ニックネームを付けていなければ、新しい種族名にする
	if (monster.nickname == from->name)
	{
		monster.nickname = to->name;
	}
	monster.monsterId = toId;

	// 成長率が違う種族になった場合に備え、今のレベルの範囲内に経験値を収める
	const int maxLevel = MonsterParty::LEVEL_MAX;
	const int lo = GetExpForLevel(to->growthRate, monster.level);
	const int hi = (monster.level < maxLevel)
		? GetExpForLevel(to->growthRate, monster.level + 1) - 1
		: lo;
	monster.exp = std::max(lo, std::min(monster.exp, hi));

	ApplyMaxHpChange(monster, oldMaxHp, MonsterParty::CalcMaxHp(*to, monster.level));
	return true;
}