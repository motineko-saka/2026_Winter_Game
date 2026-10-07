#include "ItemData.h"
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
	// ---------- 文字列から enum への変換表 ----------

	const std::unordered_map<std::string, ItemCategory>& CategoryTable(void)
	{
		static const std::unordered_map<std::string, ItemCategory> table =
		{
			{ "MEDICINE",  ItemCategory::MEDICINE },
			{ "BALL",      ItemCategory::BALL },
			{ "BATTLE",    ItemCategory::BATTLE },
			{ "EVOLUTION", ItemCategory::EVOLUTION },
			{ "KEY",       ItemCategory::KEY },
		};
		return table;
	}

	const std::unordered_map<std::string, ItemEffect>& EffectTable(void)
	{
		static const std::unordered_map<std::string, ItemEffect> table =
		{
			{ "",            ItemEffect::NONE },
			{ "NONE",        ItemEffect::NONE },
			{ "HEAL_HP",     ItemEffect::HEAL_HP },
			{ "REVIVE",      ItemEffect::REVIVE },
			{ "CURE_STATUS", ItemEffect::CURE_STATUS },
			{ "RESTORE_PP",  ItemEffect::RESTORE_PP },
			{ "CATCH",       ItemEffect::CATCH },
			{ "ATK_UP",      ItemEffect::ATK_UP },
			{ "DEF_UP",      ItemEffect::DEF_UP },
			{ "EVOLVE",      ItemEffect::EVOLVE },
		};
		return table;
	}

	// 空欄は「すべて」（NONE）として扱う
	const std::unordered_map<std::string, StatusAilment>& AilmentTable(void)
	{
		static const std::unordered_map<std::string, StatusAilment> table =
		{
			{ "",       StatusAilment::NONE },
			{ "ALL",    StatusAilment::NONE },
			{ "POISON", StatusAilment::POISON },
			{ "SLEEP",  StatusAilment::SLEEP },
			{ "BURN",   StatusAilment::BURN },
			{ "DREAM",  StatusAilment::DREAM },
			{ "COLD",   StatusAilment::COLD },
		};
		return table;
	}

	template <typename T>
	bool ParseEnum(const std::unordered_map<std::string, T>& table, const std::string& key, T& out)
	{
		auto it = table.find(key);
		if (it == table.end())
		{
			return false;
		}
		out = it->second;
		return true;
	}

	// 空欄は0、数字以外が入っていれば例外（呼び出し側でcatchする）
	int ToInt(const std::string& s)
	{
		return s.empty() ? 0 : std::stoi(s);
	}

	// 行ごとにセルへ分割して返す。空行と、#で始まる行は飛ばす。
	// （MonsterData.cpp と同じ処理。共通化したくなったら別ファイルへ移す）
	bool ReadCsv(const std::string& path, std::vector<std::vector<std::string>>& rows)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			return false;
		}

		rows.clear();
		std::string line;
		while (std::getline(ifs, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			if (line.empty() || line[0] == '#')
			{
				continue;
			}

			std::vector<std::string> cells;
			size_t start = 0;
			while (true)
			{
				size_t pos = line.find(',', start);
				if (pos == std::string::npos)
				{
					cells.push_back(line.substr(start));
					break;
				}
				cells.push_back(line.substr(start, pos - start));
				start = pos + 1;
			}

			bool allEmpty = true;
			for (const auto& c : cells)
			{
				if (!c.empty())
				{
					allEmpty = false;
					break;
				}
			}
			if (allEmpty)
			{
				continue;
			}

			rows.push_back(cells);
		}
		return true;
	}
}

ItemData::ItemData()
{
}

ItemData::~ItemData()
{
}

void ItemData::Init(void)
{
}

void ItemData::Load(void)
{
	// 失敗したらパスやCSVの内容（効果名の綴りなど）を確認する
	LoadItems("Data/CSVData/Item.csv");
}

void ItemData::LoadEnd(void)
{
}

void ItemData::Update(void)
{
}

void ItemData::Draw(void)
{
}

void ItemData::Release(void)
{
	items_.clear();
}

// アイテム：#アイテムID,名前,分類,効果,効果値,状態異常,価格,バトル中,フィールド,消費,アイコン,説明
bool ItemData::LoadItems(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	items_.clear();
	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 12)
			{
				return false;	// 列が足りない
			}

			ItemMasterData m{};
			m.id = ToInt(r[0]);
			if (m.id <= 0)
			{
				return false;	// IDは1から（0は「なし」に使う）
			}
			m.name = r[1];
			if (!ParseEnum(CategoryTable(), r[2], m.category)
				|| !ParseEnum(EffectTable(), r[3], m.effect)
				|| !ParseEnum(AilmentTable(), r[5], m.cureTarget))
			{
				return false;	// 綴り間違い
			}
			m.effectValue = ToInt(r[4]);
			m.price = ToInt(r[6]);
			m.usableInBattle = ToInt(r[7]) != 0;
			m.usableInField = ToInt(r[8]) != 0;
			m.consumable = ToInt(r[9]) != 0;
			m.iconPath = r[10];
			m.description = r[11];

			items_[m.id] = m;
		}
	}
	catch (const std::exception&)
	{
		return false;	// 数字の列に数字以外が入っている
	}
	return true;
}

const ItemMasterData* ItemData::GetItem(int id) const
{
	auto it = items_.find(id);
	return (it != items_.end()) ? &it->second : nullptr;
}
