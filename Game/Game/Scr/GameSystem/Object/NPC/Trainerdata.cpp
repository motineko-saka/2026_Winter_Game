#include "TrainerData.h"
#include <fstream>
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
			// 先頭のBOMは取り除く
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

	bool ParseDir(const std::string& s, TrainerDir& out)
	{
		if (s == "UP")		{ out = TrainerDir::UP;		return true; }
		if (s == "DOWN")	{ out = TrainerDir::DOWN;	return true; }
		if (s == "LEFT")	{ out = TrainerDir::LEFT;	return true; }
		if (s == "RIGHT")	{ out = TrainerDir::RIGHT;	return true; }
		return false;
	}
}

TrainerData::TrainerData()
{
}

TrainerData::~TrainerData()
{
}

void TrainerData::Init(void)
{
}

void TrainerData::Load(void)
{
	// 失敗したらパスやCSVの内容を確認する
	LoadTrainers("Data/CSVData/Trainer.csv");
	LoadSpawns("Data/CSVData/TrainerSpawn.csv");
}

void TrainerData::Release(void)
{
	trainers_.clear();
	spawns_.clear();
}

// トレーナー：#ID,名前,画像,賞金,出会い台詞,負け台詞,手持ち（図鑑番号:レベル を | でつなぐ）
bool TrainerData::LoadTrainers(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	trainers_.clear();
	for (const auto& r : rows)
	{
		if (r.size() < 7)
		{
			continue;	// 列が足りない行は読み飛ばす
		}

		try
		{
			TrainerMasterData t{};
			t.id = std::stoi(r[0]);
			t.name = r[1];
			t.imagePath = r[2];
			t.prize = r[3].empty() ? 0 : std::stoi(r[3]);
			t.introMessage = r[4];
			t.defeatMessage = r[5];

			for (const std::string& entry : SplitBy(r[6], '|'))
			{
				const std::vector<std::string> kv = SplitBy(entry, ':');
				if (kv.size() >= 2)
				{
					t.party.push_back({ std::stoi(kv[0]), std::stoi(kv[1]) });
				}
			}

			if (t.id > 0 && !t.party.empty())
			{
				trainers_[t.id] = t;
			}
		}
		catch (const std::exception&)
		{
			// 数字になっていない行は読み飛ばす
		}
	}
	return true;
}

// 配置：#ステージID,マスX,マスY,トレーナーID,向き(UP/DOWN/LEFT/RIGHT),視線のマス数
bool TrainerData::LoadSpawns(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	spawns_.clear();
	for (const auto& r : rows)
	{
		if (r.size() < 6)
		{
			continue;
		}

		try
		{
			TrainerSpawn s{};
			s.stageId = std::stoi(r[0]);
			s.tileX = std::stoi(r[1]);
			s.tileY = std::stoi(r[2]);
			s.trainerId = std::stoi(r[3]);
			s.sight = std::stoi(r[5]);
			if (ParseDir(r[4], s.dir))
			{
				spawns_.push_back(s);
			}
		}
		catch (const std::exception&)
		{
			// 数字になっていない行は読み飛ばす
		}
	}
	return true;
}

const TrainerMasterData* TrainerData::GetTrainer(int id) const
{
	auto it = trainers_.find(id);
	return (it != trainers_.end()) ? &it->second : nullptr;
}