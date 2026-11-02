#include "ShopData.h"

#include <algorithm>
#include <fstream>

namespace
{
	// 文字列を区切り文字で分ける（空の要素も残す）
	std::vector<std::string> SplitBy(const std::string& s, char sep)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (true)
		{
			const size_t pos = s.find(sep, start);
			if (pos == std::string::npos)
			{
				out.push_back(s.substr(start));
				break;
			}
			out.push_back(s.substr(start, pos - start));
			start = pos + 1;
		}
		return out;
	}

	// 行ごとにセルへ分割して返す。空行と、#で始まる行は飛ばす。
	bool ReadCsv(const std::string& path, std::vector<std::vector<std::string>>& rows)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			return false;
		}

		rows.clear();
		std::string line;
		bool first = true;
		while (std::getline(ifs, line))
		{
			if (first && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
			{
				line.erase(0, 3);
			}
			first = false;

			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			if (line.empty() || line[0] == '#')
			{
				continue;
			}
			rows.push_back(SplitBy(line, ','));
		}
		return true;
	}
}

void ShopData::Init(void)
{
}

void ShopData::Load(void)
{
	// なくてもゲームは動く（ショップが空になるだけ）。失敗したらパスやCSVの内容を確認する
	LoadShops("Data/CSVData/Shop.csv");
}

void ShopData::Release(void)
{
	shops_.clear();
}

bool ShopData::LoadShops(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	std::unordered_map<int, ShopMasterData> loaded;
	for (const auto& r : rows)
	{
		if (r.size() < 2)
		{
			continue;	// 列が足りない行は読み飛ばす
		}

		try
		{
			const int shopId = std::stoi(r[0]);
			const int itemId = std::stoi(r[1]);
			if (shopId <= 0 || itemId <= 0)
			{
				continue;
			}

			ShopMasterData& shop = loaded[shopId];
			shop.id = shopId;

			// 同じ品を二重に並べない
			if (std::find(shop.itemIds.begin(), shop.itemIds.end(), itemId) == shop.itemIds.end())
			{
				shop.itemIds.push_back(itemId);
			}
		}
		catch (const std::exception&)
		{
			// 数字になっていない行は読み飛ばす
		}
	}

	shops_ = loaded;
	return true;
}

const ShopMasterData* ShopData::GetShop(int id) const
{
	auto it = shops_.find(id);
	return (it != shops_.end()) ? &it->second : nullptr;
}