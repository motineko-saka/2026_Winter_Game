#include "MonsterBox.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include "MonsterParty.h"

namespace
{
	// ---------- セーブデータの形式 ----------
	//  1行目：MONSTER_BOX,バージョン
	//  2行目：ボックス数,1ボックスのスロット数
	//  続くボックス数行：各ボックスの名前（1行まるごと名前）
	//  以降（預けているモンスター1匹につき1行）：ボックス番号,スロット番号,（MonsterParty::ToSaveLineの内容）
	const char* const SAVE_MAGIC = "MONSTER_BOX";
	const int SAVE_VERSION = 1;

	std::string DefaultName(int boxIndex)
	{
		return std::string("ボックス") + std::to_string(boxIndex + 1);
	}

	bool ToIntStrict(const std::string& s, int& out)
	{
		if (s.empty())
		{
			return false;
		}
		try
		{
			size_t pos = 0;
			out = std::stoi(s, &pos);
			return pos == s.size();
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	void StripCr(std::string& s)
	{
		if (!s.empty() && s.back() == '\r')
		{
			s.pop_back();
		}
	}

	std::string SanitizeName(std::string name)
	{
		for (auto& ch : name)
		{
			if (ch == '\r' || ch == '\n')
			{
				ch = ' ';
			}
		}
		return name;
	}

	// exceptIndex以外に、戦えるモンスターが手持ちにいるか
	bool HasAliveExcept(const MonsterParty& party, int exceptIndex)
	{
		for (int i = 0; i < party.GetCount(); i++)
		{
			if (i != exceptIndex && party.Get(i)->currentHp > 0)
			{
				return true;
			}
		}
		return false;
	}
}

MonsterBox::MonsterBox()
{
}

MonsterBox::~MonsterBox()
{
}

void MonsterBox::Init(void)
{
	boxes_.clear();
	boxes_.resize(BOX_COUNT);
	for (int i = 0; i < BOX_COUNT; i++)
	{
		boxes_[i].name = DefaultName(i);
		boxes_[i].slots.resize(SLOT_COUNT);
	}
}

void MonsterBox::Release(void)
{
	boxes_.clear();
}

bool MonsterBox::IsValidPosition(int boxIndex, int slotIndex) const
{
	return boxIndex >= 0 && boxIndex < static_cast<int>(boxes_.size())
		&& slotIndex >= 0 && slotIndex < static_cast<int>(boxes_[boxIndex].slots.size());
}

int MonsterBox::FindEmptySlot(int boxIndex) const
{
	if (boxIndex < 0 || boxIndex >= static_cast<int>(boxes_.size()))
	{
		return -1;
	}
	const auto& slots = boxes_[boxIndex].slots;
	for (size_t i = 0; i < slots.size(); i++)
	{
		if (!slots[i].used)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

int MonsterBox::FindEmptyBox(void) const
{
	for (int i = 0; i < static_cast<int>(boxes_.size()); i++)
	{
		if (FindEmptySlot(i) >= 0)
		{
			return i;
		}
	}
	return -1;
}

bool MonsterBox::IsFull(void) const
{
	return FindEmptyBox() < 0;
}

bool MonsterBox::Add(const MonsterInstance& monster, int boxIndex)
{
	if (boxIndex < 0)
	{
		boxIndex = FindEmptyBox();
	}
	const int slot = FindEmptySlot(boxIndex);
	if (slot < 0)
	{
		return false;	// 満杯、または範囲外
	}

	boxes_[boxIndex].slots[slot].used = true;
	boxes_[boxIndex].slots[slot].monster = monster;
	return true;
}

bool MonsterBox::Remove(int boxIndex, int slotIndex)
{
	if (Get(boxIndex, slotIndex) == nullptr)
	{
		return false;
	}
	boxes_[boxIndex].slots[slotIndex] = BoxSlot{};
	return true;
}

bool MonsterBox::Move(int fromBox, int fromSlot, int toBox, int toSlot)
{
	if (!IsValidPosition(toBox, toSlot) || Get(fromBox, fromSlot) == nullptr)
	{
		return false;
	}
	if (fromBox == toBox && fromSlot == toSlot)
	{
		return false;	// 同じ場所
	}

	// 空でも埋まっていても、スロットの中身ごと入れ替えれば済む
	std::swap(boxes_[fromBox].slots[fromSlot], boxes_[toBox].slots[toSlot]);
	return true;
}

MonsterInstance* MonsterBox::Get(int boxIndex, int slotIndex)
{
	if (!IsValidPosition(boxIndex, slotIndex) || !boxes_[boxIndex].slots[slotIndex].used)
	{
		return nullptr;
	}
	return &boxes_[boxIndex].slots[slotIndex].monster;
}

const MonsterInstance* MonsterBox::Get(int boxIndex, int slotIndex) const
{
	if (!IsValidPosition(boxIndex, slotIndex) || !boxes_[boxIndex].slots[slotIndex].used)
	{
		return nullptr;
	}
	return &boxes_[boxIndex].slots[slotIndex].monster;
}

int MonsterBox::GetCount(int boxIndex) const
{
	if (boxIndex < 0 || boxIndex >= static_cast<int>(boxes_.size()))
	{
		return 0;
	}
	int count = 0;
	for (const auto& s : boxes_[boxIndex].slots)
	{
		if (s.used)
		{
			count++;
		}
	}
	return count;
}

int MonsterBox::GetTotalCount(void) const
{
	int total = 0;
	for (int i = 0; i < static_cast<int>(boxes_.size()); i++)
	{
		total += GetCount(i);
	}
	return total;
}

const std::string& MonsterBox::GetBoxName(int boxIndex) const
{
	static const std::string empty;
	if (boxIndex < 0 || boxIndex >= static_cast<int>(boxes_.size()))
	{
		return empty;
	}
	return boxes_[boxIndex].name;
}

bool MonsterBox::SetBoxName(int boxIndex, const std::string& name)
{
	if (boxIndex < 0 || boxIndex >= static_cast<int>(boxes_.size()))
	{
		return false;
	}
	boxes_[boxIndex].name = name.empty() ? DefaultName(boxIndex) : SanitizeName(name);
	return true;
}

bool MonsterBox::Deposit(MonsterParty& party, int partyIndex, int boxIndex)
{
	const MonsterInstance* target = party.Get(partyIndex);
	if (target == nullptr)
	{
		return false;
	}
	if (!HasAliveExcept(party, partyIndex))
	{
		return false;	// 戦えるモンスターが手持ちからいなくなってしまう
	}

	const MonsterInstance monster = *target;
	if (!Add(monster, boxIndex))
	{
		return false;	// ボックスが満杯
	}
	party.Remove(partyIndex);
	return true;
}

bool MonsterBox::Withdraw(MonsterParty& party, int boxIndex, int slotIndex)
{
	const MonsterInstance* target = Get(boxIndex, slotIndex);
	if (target == nullptr || party.IsFull())
	{
		return false;
	}

	if (!party.Add(*target))
	{
		return false;
	}
	return Remove(boxIndex, slotIndex);
}

bool MonsterBox::SwapWithParty(MonsterParty& party, int partyIndex, int boxIndex, int slotIndex)
{
	MonsterInstance* partyMonster = party.Get(partyIndex);
	if (partyMonster == nullptr || !IsValidPosition(boxIndex, slotIndex))
	{
		return false;
	}

	BoxSlot& slot = boxes_[boxIndex].slots[slotIndex];

	// 入れ替えたあとの手持ちに、戦えるモンスターが残るか
	const bool incomingAlive = slot.used && slot.monster.currentHp > 0;
	if (!HasAliveExcept(party, partyIndex) && !incomingAlive)
	{
		return false;
	}

	if (!slot.used)
	{
		// 空きスロットへ預ける
		slot.monster = *partyMonster;
		slot.used = true;
		party.Remove(partyIndex);
		return true;
	}

	std::swap(*partyMonster, slot.monster);
	return true;
}

bool MonsterBox::Save(const std::string& path) const
{
	const std::string tmpPath = path + ".tmp";
	{
		std::ofstream ofs(tmpPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;
		}

		ofs << SAVE_MAGIC << "," << SAVE_VERSION << "\n";
		ofs << boxes_.size() << "," << SLOT_COUNT << "\n";
		for (const auto& box : boxes_)
		{
			ofs << SanitizeName(box.name) << "\n";
		}
		for (size_t b = 0; b < boxes_.size(); b++)
		{
			for (size_t s = 0; s < boxes_[b].slots.size(); s++)
			{
				if (boxes_[b].slots[s].used)
				{
					ofs << b << "," << s << "," << MonsterParty::ToSaveLine(boxes_[b].slots[s].monster) << "\n";
				}
			}
		}

		ofs.flush();
		if (!ofs)
		{
			ofs.close();
			std::remove(tmpPath.c_str());
			return false;
		}
	}

	std::remove(path.c_str());
	return std::rename(tmpPath.c_str(), path.c_str()) == 0;
}

bool MonsterBox::Load(const std::string& path, const MonsterData& data)
{
	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		return false;
	}

	std::string line;

	// 1行目：識別子とバージョン
	if (!std::getline(ifs, line))
	{
		return false;
	}
	StripCr(line);
	if (line != std::string(SAVE_MAGIC) + "," + std::to_string(SAVE_VERSION))
	{
		return false;
	}

	// 2行目：保存時のボックス数とスロット数（今の設定以下ならOK）
	if (!std::getline(ifs, line))
	{
		return false;
	}
	StripCr(line);
	const size_t comma = line.find(',');
	int savedBoxes = 0;
	int savedSlots = 0;
	if (comma == std::string::npos
		|| !ToIntStrict(line.substr(0, comma), savedBoxes)
		|| !ToIntStrict(line.substr(comma + 1), savedSlots)
		|| savedBoxes < 0 || savedBoxes > BOX_COUNT
		|| savedSlots < 0 || savedSlots > SLOT_COUNT)
	{
		return false;
	}

	std::vector<Box> loaded(BOX_COUNT);
	for (int i = 0; i < BOX_COUNT; i++)
	{
		loaded[i].name = DefaultName(i);
		loaded[i].slots.resize(SLOT_COUNT);
	}

	// ボックス名
	for (int i = 0; i < savedBoxes; i++)
	{
		if (!std::getline(ifs, line))
		{
			return false;
		}
		StripCr(line);
		if (!line.empty())
		{
			loaded[i].name = line;
		}
	}

	// 預けているモンスター
	while (std::getline(ifs, line))
	{
		StripCr(line);
		if (line.empty())
		{
			continue;
		}

		const size_t c1 = line.find(',');
		const size_t c2 = (c1 == std::string::npos) ? std::string::npos : line.find(',', c1 + 1);
		int boxIndex = 0;
		int slotIndex = 0;
		if (c2 == std::string::npos
			|| !ToIntStrict(line.substr(0, c1), boxIndex)
			|| !ToIntStrict(line.substr(c1 + 1, c2 - c1 - 1), slotIndex)
			|| boxIndex < 0 || boxIndex >= savedBoxes
			|| slotIndex < 0 || slotIndex >= savedSlots)
		{
			return false;
		}
		if (loaded[boxIndex].slots[slotIndex].used)
		{
			return false;	// 同じ場所に2匹
		}

		MonsterInstance monster;
		if (!MonsterParty::FromSaveLine(line.substr(c2 + 1), data, monster))
		{
			return false;
		}
		loaded[boxIndex].slots[slotIndex].used = true;
		loaded[boxIndex].slots[slotIndex].monster = monster;
	}

	boxes_ = loaded;	// 全部読めてから反映する
	return true;
}