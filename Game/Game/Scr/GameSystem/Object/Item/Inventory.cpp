#include "Inventory.h"
#include "ItemData.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace
{
	// ---------- セーブデータの形式 ----------
	//  1行目：INVENTORY,バージョン
	//  2行目：アイテムの種類数
	//  3行目以降（1種類1行）：アイテムID,個数
	const char* const SAVE_MAGIC = "INVENTORY";
	const int SAVE_VERSION = 1;

	// 数字だけの文字列のときだけ true
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
}

Inventory::Inventory()
{
}

Inventory::~Inventory()
{
}

void Inventory::Init(void)
{
	items_.clear();
}

void Inventory::Release(void)
{
	items_.clear();
}

bool Inventory::Add(int itemId, int count)
{
	if (itemId <= 0 || count <= 0)
	{
		return false;
	}

	int& have = items_[itemId];
	const int before = have;
	const int maxCount = COUNT_MAX;
	have = std::min(maxCount, have + count);
	if (have == before)
	{
		if (before == 0)
		{
			items_.erase(itemId);
		}
		return false;	// すでに上限
	}
	return true;
}

bool Inventory::Remove(int itemId, int count)
{
	if (count <= 0)
	{
		return false;
	}
	auto it = items_.find(itemId);
	if (it == items_.end() || it->second < count)
	{
		return false;
	}

	it->second -= count;
	if (it->second <= 0)
	{
		items_.erase(it);	// 0個になったら一覧から消す
	}
	return true;
}

int Inventory::GetCount(int itemId) const
{
	auto it = items_.find(itemId);
	return (it != items_.end()) ? it->second : 0;
}

bool Inventory::Has(int itemId, int count) const
{
	return GetCount(itemId) >= count;
}

bool Inventory::Save(const std::string& path) const
{
	// いったん別名で書いてから差し替える（書き込み途中で失敗しても元のセーブが壊れない）
	const std::string tmpPath = path + ".tmp";
	{
		std::ofstream ofs(tmpPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;
		}

		ofs << SAVE_MAGIC << "," << SAVE_VERSION << "\n";
		ofs << items_.size() << "\n";
		for (const auto& pair : items_)
		{
			ofs << pair.first << "," << pair.second << "\n";
		}

		ofs.flush();
		if (!ofs)
		{
			ofs.close();
			std::remove(tmpPath.c_str());
			return false;
		}
	}

	std::remove(path.c_str());	// Windowsではrenameの前に既存ファイルを消す必要がある
	return std::rename(tmpPath.c_str(), path.c_str()) == 0;
}

bool Inventory::Load(const std::string& path, const ItemData& data)
{
	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		return false;	// セーブデータがない
	}

	std::string line;

	if (!std::getline(ifs, line))
	{
		return false;
	}
	StripCr(line);
	if (line != std::string(SAVE_MAGIC) + "," + std::to_string(SAVE_VERSION))
	{
		return false;
	}

	if (!std::getline(ifs, line))
	{
		return false;
	}
	StripCr(line);
	int count = 0;
	if (!ToIntStrict(line, count) || count < 0)
	{
		return false;
	}

	std::map<int, int> loaded;
	for (int i = 0; i < count; i++)
	{
		if (!std::getline(ifs, line))
		{
			return false;
		}
		StripCr(line);

		const size_t pos = line.find(',');
		if (pos == std::string::npos)
		{
			return false;
		}
		int id = 0;
		int num = 0;
		if (!ToIntStrict(line.substr(0, pos), id) || !ToIntStrict(line.substr(pos + 1), num))
		{
			return false;
		}
		if (data.GetItem(id) == nullptr || num < 1 || num > COUNT_MAX || loaded.count(id) != 0)
		{
			return false;	// 存在しないアイテム・個数が範囲外・IDの重複
		}
		loaded[id] = num;
	}

	items_ = loaded;	// 全部読めてから反映する
	return true;
}
