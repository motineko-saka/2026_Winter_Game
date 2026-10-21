#include "BallEffectData.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
	std::vector<std::string> Split(const std::string& line)
	{
		std::vector<std::string> out;
		std::stringstream ss(line);
		std::string cell;
		while (std::getline(ss, cell, ','))
		{
			// 行末の改行コード(\r)を取り除く
			while (!cell.empty() && (cell.back() == '\r' || cell.back() == '\n'))
			{
				cell.pop_back();
			}
			out.push_back(cell);
		}
		return out;
	}

	int ToInt(const std::vector<std::string>& c, size_t i, int def)
	{
		if (i >= c.size() || c[i].empty())
		{
			return def;
		}
		try
		{
			return std::stoi(c[i]);
		}
		catch (...)
		{
			return def;
		}
	}

	std::string ToStr(const std::vector<std::string>& c, size_t i)
	{
		return (i < c.size()) ? c[i] : std::string();
	}
}

bool BallEffectData::Load(const std::string& path)
{
	balls_.clear();

	std::ifstream ifs(path);
	if (!ifs)
	{
		return false;
	}

	std::string line;
	bool header = true;
	while (std::getline(ifs, line))
	{
		// UTF-8 BOM を取り除く
		if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
		{
			line.erase(0, 3);
		}
		if (header)
		{
			header = false;	// 1行目はヘッダー
			continue;
		}
		if (line.empty() || line[0] == '#' || line[0] == '\r')
		{
			continue;
		}

		const std::vector<std::string> c = Split(line);
		BallEffectMaster b;
		b.id = ToInt(c, 0, 0);
		if (b.id <= 0)
		{
			continue;
		}
		b.name = ToStr(c, 1);
		b.ballImage = ToStr(c, 2);
		b.openImage = ToStr(c, 3);
		b.ballSize = ToInt(c, 4, 28);
		b.bounces = ToInt(c, 5, 2);
		b.effectImage = ToStr(c, 6);
		b.columns = std::max(1, ToInt(c, 7, 1));
		b.frameCount = std::max(1, ToInt(c, 8, 1));
		b.frameW = ToInt(c, 9, 0);
		b.frameH = ToInt(c, 10, 0);
		b.frameTime = std::max(1, ToInt(c, 11, 3));
		b.scalePercent = ToInt(c, 12, 100);
		b.additive = ToInt(c, 13, 1) != 0;
		b.glowR = ToInt(c, 14, 255);
		b.glowG = ToInt(c, 15, 255);
		b.glowB = ToInt(c, 16, 255);
		balls_[b.id] = b;
	}
	return !balls_.empty();
}

const BallEffectMaster* BallEffectData::Get(int id) const
{
	auto it = balls_.find(id);
	return (it != balls_.end()) ? &it->second : nullptr;
}