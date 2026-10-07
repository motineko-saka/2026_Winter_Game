#include "ItemUse.h"
#include <algorithm>
#include "../Monster/MonsterData.h"
#include "../Monster/MonsterParty.h"

bool ItemUse::NeedsTarget(const ItemMasterData& item)
{
	switch (item.effect)
	{
	case ItemEffect::HEAL_HP:
	case ItemEffect::REVIVE:
	case ItemEffect::CURE_STATUS:
	case ItemEffect::RESTORE_PP:
	case ItemEffect::EVOLVE:
		return true;
	default:
		return false;
	}
}

bool ItemUse::CanUseOnMonster(const MonsterData& data, const ItemMasterData& item,
	const MonsterInstance& mon, std::string& reason)
{
	reason = "つかっても こうかがない！";
	const int maxHp = MonsterParty::GetMaxHp(data, mon);

	switch (item.effect)
	{
	case ItemEffect::HEAL_HP:
		// ひんし中は回復薬では治らない。HPが満タンなら意味がない
		return mon.currentHp > 0 && mon.currentHp < maxHp;

	case ItemEffect::REVIVE:
		return mon.currentHp <= 0;

	case ItemEffect::CURE_STATUS:
		if (mon.currentHp <= 0 || mon.ailment == StatusAilment::NONE)
		{
			return false;
		}
		// 治す状態異常が決まっているなら、一致しているときだけ
		return item.cureTarget == StatusAilment::NONE || item.cureTarget == mon.ailment;

	case ItemEffect::RESTORE_PP:
		if (mon.currentHp <= 0)
		{
			return false;
		}
		for (const MoveSlot& slot : mon.moves)
		{
			if (slot.moveId == MonsterParty::MOVE_NONE)
			{
				continue;
			}
			const MoveData* move = data.GetMove(slot.moveId);
			if (move != nullptr && slot.currentPp < move->pp)
			{
				return true;
			}
		}
		return false;

	case ItemEffect::EVOLVE:
		return GetItemEvolution(data, mon, item.id) != 0;

	default:
		return false;
	}
}

bool ItemUse::ApplyToMonster(const MonsterData& data, const ItemMasterData& item,
	MonsterInstance& mon, std::string& message)
{
	if (!CanUseOnMonster(data, item, mon, message))
	{
		return false;
	}

	const int maxHp = MonsterParty::GetMaxHp(data, mon);

	switch (item.effect)
	{
	case ItemEffect::HEAL_HP:
	{
		const int before = mon.currentHp;
		mon.currentHp = std::min(maxHp, before + std::max(0, item.effectValue));
		message = mon.nickname + "の HPが " + std::to_string(mon.currentHp - before) + " かいふくした！";
		return true;
	}

	case ItemEffect::REVIVE:
	{
		const int rate = std::max(1, std::min(item.effectValue, 100));
		mon.currentHp = std::max(1, maxHp * rate / 100);
		mon.ailment = StatusAilment::NONE;
		message = mon.nickname + "は げんきを とりもどした！";
		return true;
	}

	case ItemEffect::CURE_STATUS:
		mon.ailment = StatusAilment::NONE;
		message = mon.nickname + "の じょうたいが なおった！";
		return true;

	case ItemEffect::RESTORE_PP:
		for (MoveSlot& slot : mon.moves)
		{
			if (slot.moveId == MonsterParty::MOVE_NONE)
			{
				continue;
			}
			const MoveData* move = data.GetMove(slot.moveId);
			if (move != nullptr)
			{
				slot.currentPp = std::min(move->pp, slot.currentPp + std::max(0, item.effectValue));
			}
		}
		message = mon.nickname + "の わざの PPが かいふくした！";
		return true;

	default:
		message = "つかっても こうかがない！";
		return false;
	}
}

int ItemUse::GetItemEvolution(const MonsterData& data, const MonsterInstance& mon, int itemId)
{
	const MonsterMasterData* master = data.GetMonster(mon.monsterId);
	if (master == nullptr || itemId <= 0)
	{
		return 0;
	}

	for (const Evolution& e : master->evolutions)
	{
		if (e.itemId == itemId && mon.level >= e.level)
		{
			return e.toId;
		}
	}
	return 0;
}
