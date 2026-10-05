#include "MonsterParty.h"
#include "MonsterGrowth.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
	// 種族値とレベルから能力値の元になる値を出す
	int BaseStat(int base, int level)
	{
		return base * 2 * level / 100;
	}

	int ClampLevel(int level)
	{
		if (level < 1)
		{
			return 1;
		}
		if (level > MonsterParty::LEVEL_MAX)
		{
			return MonsterParty::LEVEL_MAX;
		}
		return level;
	}
}

namespace
{
	// ---------- セーブデータの形式 ----------
	//  1行目：MONSTER_PARTY,バージョン
	//  2行目：手持ちの数
	//  3行目以降（1匹1行）：
	//    図鑑番号,レベル,経験値,現在HP,状態異常,技ID1,PP1,技ID2,PP2,技ID3,PP3,技ID4,PP4,ニックネーム
	//  ニックネームは最後なので、カンマが含まれていても読める。
	const char* const SAVE_MAGIC = "MONSTER_PARTY";
	const int SAVE_VERSION = 1;
	const int NUM_FIELDS = 13;	// ニックネームより前の数値の数

	// 状態異常の最大値（StatusAilmentに追加したらここも更新する）
	const int STATUS_MAX = static_cast<int>(StatusAilment::COLD);

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

	// 改行が入るとセーブデータの行が壊れるので、空白に置き換える
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

	// 1行を MonsterInstance に変換する。構造的におかしければ false。
	// HPとPPは、マスターデータの調整で上限が変わっても読めるように範囲内へ丸める。
	bool ParseMonsterLine(const std::string& line, const MonsterData& data, MonsterInstance& out)
	{
		int nums[NUM_FIELDS] = {};
		size_t start = 0;
		for (int i = 0; i < NUM_FIELDS; i++)
		{
			size_t pos = line.find(',', start);
			if (pos == std::string::npos)
			{
				return false;
			}
			if (!ToIntStrict(line.substr(start, pos - start), nums[i]))
			{
				return false;
			}
			start = pos + 1;
		}
		const std::string nickname = line.substr(start);

		const MonsterMasterData* master = data.GetMonster(nums[0]);
		if (master == nullptr)
		{
			return false;	// 存在しない図鑑番号
		}
		const int level = nums[1];
		if (level < 1 || level > MonsterParty::LEVEL_MAX)
		{
			return false;
		}
		if (nums[2] < 0 || nums[4] < 0 || nums[4] > STATUS_MAX)
		{
			return false;
		}

		out = MonsterInstance{};
		out.monsterId = nums[0];
		out.level = level;
		out.exp = nums[2];
		out.nickname = nickname.empty() ? master->name : nickname;

		const int maxHp = MonsterParty::CalcMaxHp(*master, level);
		out.currentHp = std::max(0, std::min(nums[3], maxHp));
		out.ailment = static_cast<StatusAilment>(nums[4]);

		for (size_t i = 0; i < out.moves.size(); i++)
		{
			const int moveId = nums[5 + i * 2];
			const int pp = nums[6 + i * 2];
			if (moveId == MonsterParty::MOVE_NONE)
			{
				continue;	// 空スロット
			}
			const MoveData* move = data.GetMove(moveId);
			if (move == nullptr)
			{
				return false;	// 存在しない技
			}
			out.moves[i].moveId = moveId;
			out.moves[i].currentPp = std::max(0, std::min(pp, move->pp));
		}
		return true;
	}
}

MonsterParty::MonsterParty()
{
}

MonsterParty::~MonsterParty()
{
}

void MonsterParty::Init(void)
{
	party_.clear();
}

void MonsterParty::Release(void)
{
	party_.clear();
}

bool MonsterParty::Add(const MonsterInstance& monster)
{
	if (IsFull())
	{
		return false;
	}
	party_.push_back(monster);
	return true;
}

bool MonsterParty::Remove(int index)
{
	if (index < 0 || index >= GetCount())
	{
		return false;
	}
	party_.erase(party_.begin() + index);
	return true;
}

bool MonsterParty::Swap(int a, int b)
{
	if (a < 0 || a >= GetCount() || b < 0 || b >= GetCount())
	{
		return false;
	}
	std::swap(party_[a], party_[b]);
	return true;
}

int MonsterParty::GetCount(void) const
{
	return static_cast<int>(party_.size());
}

bool MonsterParty::IsFull(void) const
{
	return GetCount() >= PARTY_MAX;
}

MonsterInstance* MonsterParty::Get(int index)
{
	if (index < 0 || index >= GetCount())
	{
		return nullptr;
	}
	return &party_[index];
}

const MonsterInstance* MonsterParty::Get(int index) const
{
	if (index < 0 || index >= GetCount())
	{
		return nullptr;
	}
	return &party_[index];
}

bool MonsterParty::HasAlive(void) const
{
	for (const auto& m : party_)
	{
		if (m.currentHp > 0)
		{
			return true;
		}
	}
	return false;
}

MonsterInstance* MonsterParty::GetFirstAlive(void)
{
	for (auto& m : party_)
	{
		if (m.currentHp > 0)
		{
			return &m;
		}
	}
	return nullptr;
}

void MonsterParty::HealAll(const MonsterData& data)
{
	for (auto& m : party_)
	{
		Heal(data, m);
	}
}

bool MonsterParty::Create(const MonsterData& data, int monsterId, int level, MonsterInstance& out)
{
	const MonsterMasterData* master = data.GetMonster(monsterId);
	if (master == nullptr)
	{
		return false;
	}

	level = ClampLevel(level);

	out = MonsterInstance{};
	out.monsterId = monsterId;
	out.nickname = master->name;	// 初期のニックネームは種族名
	out.level = level;
	out.exp = MonsterGrowth::GetExpForLevel(master->growthRate, level);	// そのレベルの最低経験値
	out.currentHp = CalcMaxHp(*master, level);
	out.ailment = StatusAilment::NONE;

	// 習得レベル順に並べ、今のレベルまでに覚える技を集める
	std::vector<LearnEntry> learnset = master->learnset;
	std::stable_sort(learnset.begin(), learnset.end(),
		[](const LearnEntry& a, const LearnEntry& b) { return a.level < b.level; });

	std::vector<int> learned;
	for (const auto& e : learnset)
	{
		if (e.level > level)
		{
			break;
		}
		if (std::find(learned.begin(), learned.end(), e.moveId) != learned.end())
		{
			continue;	// 同じ技の重複は無視
		}
		learned.push_back(e.moveId);
	}

	// スロット数（4）を超えたら古い技から外す
	while (learned.size() > out.moves.size())
	{
		learned.erase(learned.begin());
	}

	for (size_t i = 0; i < learned.size(); i++)
	{
		const MoveData* move = data.GetMove(learned[i]);
		out.moves[i].moveId = learned[i];
		out.moves[i].currentPp = (move != nullptr) ? move->pp : 0;
	}

	return true;
}

void MonsterParty::Heal(const MonsterData& data, MonsterInstance& monster)
{
	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	if (master == nullptr)
	{
		return;
	}

	monster.currentHp = CalcMaxHp(*master, monster.level);
	monster.ailment = StatusAilment::NONE;

	for (auto& slot : monster.moves)
	{
		if (slot.moveId == MOVE_NONE)
		{
			continue;
		}
		const MoveData* move = data.GetMove(slot.moveId);
		if (move != nullptr)
		{
			slot.currentPp = move->pp;
		}
	}
}

int MonsterParty::CalcMaxHp(const MonsterMasterData& master, int level)
{
	level = ClampLevel(level);
	return BaseStat(master.baseHp, level) + level + 10;
}

int MonsterParty::CalcAttack(const MonsterMasterData& master, int level)
{
	level = ClampLevel(level);
	return BaseStat(master.baseAttack, level) + 5;
}

int MonsterParty::CalcDefense(const MonsterMasterData& master, int level)
{
	level = ClampLevel(level);
	return BaseStat(master.baseDefense, level) + 5;
}

int MonsterParty::CalcSpeed(const MonsterMasterData& master, int level)
{
	level = ClampLevel(level);
	return BaseStat(master.baseSpeed, level) + 5;
}

int MonsterParty::GetMaxHp(const MonsterData& data, const MonsterInstance& monster)
{
	const MonsterMasterData* master = data.GetMonster(monster.monsterId);
	return (master != nullptr) ? CalcMaxHp(*master, monster.level) : 0;
}

bool MonsterParty::Save(const std::string& path) const
{
	// いったん別名で書いてから差し替える（書き込み途中で失敗しても元のセーブが壊れない）
	const std::string tmpPath = path + ".tmp";
	{
		std::ofstream ofs(tmpPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;	// フォルダがない等
		}

		ofs << SAVE_MAGIC << "," << SAVE_VERSION << "\n";
		ofs << party_.size() << "\n";
		for (const auto& m : party_)
		{
			ofs << ToSaveLine(m) << "\n";
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

bool MonsterParty::Load(const std::string& path, const MonsterData& data)
{
	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		return false;	// セーブデータがない
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

	// 2行目：手持ちの数
	if (!std::getline(ifs, line))
	{
		return false;
	}
	StripCr(line);
	int count = 0;
	if (!ToIntStrict(line, count) || count < 0 || count > PARTY_MAX)
	{
		return false;
	}

	// 3行目以降：1匹ずつ
	std::vector<MonsterInstance> loaded;
	for (int i = 0; i < count; i++)
	{
		if (!std::getline(ifs, line))
		{
			return false;
		}
		StripCr(line);

		MonsterInstance m;
		if (!ParseMonsterLine(line, data, m))
		{
			return false;
		}
		loaded.push_back(m);
	}

	party_ = loaded;	// 全部読めてから反映する
	return true;
}

std::string MonsterParty::ToSaveLine(const MonsterInstance& monster)
{
	std::ostringstream oss;
	oss << monster.monsterId << "," << monster.level << "," << monster.exp << ","
		<< monster.currentHp << "," << static_cast<int>(monster.ailment);
	for (const auto& slot : monster.moves)
	{
		oss << "," << slot.moveId << "," << slot.currentPp;
	}
	oss << "," << SanitizeName(monster.nickname);
	return oss.str();
}

bool MonsterParty::FromSaveLine(const std::string& line, const MonsterData& data, MonsterInstance& out)
{
	return ParseMonsterLine(line, data, out);
}