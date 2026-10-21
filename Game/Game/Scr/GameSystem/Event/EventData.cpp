#include "EventData.h"

#include <fstream>
#include <set>
#include <stdexcept>

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

	bool ParseTrigger(const std::string& s, EventTrigger& out)
	{
		if (s == "ACTION")	{ out = EventTrigger::ACTION;	return true; }
		if (s == "TOUCH")	{ out = EventTrigger::TOUCH;	return true; }
		if (s == "AUTO")	{ out = EventTrigger::AUTO;		return true; }
		return false;
	}
}

void EventData::Init(void)
{
}

void EventData::Load(void)
{
	// 失敗したらパスやCSVの内容を確認する（なくてもゲームは動く）
	LoadEvents("Data/CSVData/Event.csv");
}

void EventData::Release(void)
{
	events_.clear();
}

std::string EventData::DoneFlagName(int eventId)
{
	return "event_" + std::to_string(eventId) + "_done";
}

bool EventData::LoadEvents(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	events_.clear();
	std::set<int> usedIds;
	for (const auto& r : rows)
	{
		if (r.size() < 7)
		{
			continue;	// 列が足りない行は読み飛ばす（画像の列は省略できる）
		}

		try
		{
			EventSpawn e{};
			e.id = std::stoi(r[0]);
			e.stageId = std::stoi(r[1]);
			e.tileX = std::stoi(r[2]);
			e.tileY = std::stoi(r[3]);
			e.condition = r[5];
			e.script = r[6];
			e.once = (r.size() > 7 && !r[7].empty()) ? (std::stoi(r[7]) != 0) : false;
			e.image = (r.size() > 8) ? r[8] : std::string();

			if (e.id <= 0 || e.script.empty() || !ParseTrigger(r[4], e.trigger))
			{
				continue;
			}
			if (!usedIds.insert(e.id).second)
			{
				continue;	// ID の重複は2つ目以降を無視する
			}
			events_.push_back(e);
		}
		catch (const std::exception&)
		{
			// 数字になっていない行は読み飛ばす
		}
	}
	return true;
}
