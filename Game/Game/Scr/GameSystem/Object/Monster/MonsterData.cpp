#include "MonsterData.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
	// ---------- 文字列から enum への変換表 ----------

	const std::unordered_map<std::string, ElementType>& ElementTable(void)
	{
		static const std::unordered_map<std::string, ElementType> table =
		{
			{ "",               ElementType::NONE },	// 空欄はタイプなし（タイプ2用）
			{ "NONE",           ElementType::NONE },
			{ "FIRE",           ElementType::FIRE },
			{ "WATER",          ElementType::WATER },
			{ "WIND",           ElementType::WIND },
			{ "NORMAL",         ElementType::NORMAL },
			{ "JEWELRY",        ElementType::JEWELRY },
			{ "MACHINE",        ElementType::MACHINE },
			{ "SPIRIT",         ElementType::SPIRIT },
			{ "LIGHT",          ElementType::LIGHT },
			{ "DARK",           ElementType::DARK },
			{ "DRAGON",         ElementType::DRAGON },
			{ "SOUND",          ElementType::SOUND },
			{ "ILLUSION",       ElementType::ILLUSION },
			{ "KARMA",          ElementType::KARMA },
			{ "ICE",            ElementType::ICE },
			{ "POISON",         ElementType::POISON },
			{ "SOIL",           ElementType::SOIL },
			{ "MARTIAL_COMBAT", ElementType::MARTIAL_COMBAT },
			{ "GOD",            ElementType::GOD },
		};
		return table;
	}

	const std::unordered_map<std::string, MoveEffect>& EffectTable(void)
	{
		static const std::unordered_map<std::string, MoveEffect> table =
		{
			{ "",          MoveEffect::NONE },
			{ "NONE",      MoveEffect::NONE },
			{ "POISON",    MoveEffect::POISON },
			{ "PARALYSIS", MoveEffect::PARALYSIS },
			{ "ATK_UP",    MoveEffect::ATK_UP },
			{ "DEF_DOWN",  MoveEffect::DEF_DOWN },
			{ "HEAL",      MoveEffect::HEAL },
			{ "RECOIL",    MoveEffect::RECOIL },
		};
		return table;
	}

	const std::unordered_map<std::string, AnimPos>& AnimPosTable(void)
	{
		static const std::unordered_map<std::string, AnimPos> table =
		{
			{ "",       AnimPos::TARGET },
			{ "TARGET", AnimPos::TARGET },
			{ "USER",   AnimPos::USER },
			{ "CENTER", AnimPos::CENTER },
		};
		return table;
	}

	const std::unordered_map<std::string, GrowthRate>& GrowthTable(void)
	{
		static const std::unordered_map<std::string, GrowthRate> table =
		{
			{ "FAST",   GrowthRate::FAST },
			{ "MEDIUM", GrowthRate::MEDIUM },
			{ "SLOW",   GrowthRate::SLOW },
		};
		return table;
	}

	// 変換表から探す。見つからなければ false（CSVの書き間違い）
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

	// ---------- CSVの読み込み ----------
	// 行ごとにセルへ分割して返す。空行と、#で始まる行は飛ばす。
	// 末尾が空欄のセルも残す（"a,b," は3セル）。
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

			// Excelが出力する「,,,,」だけの行は飛ばす
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

MonsterData::MonsterData()
{
}

MonsterData::~MonsterData()
{
}

void MonsterData::Init(void)
{
}

void MonsterData::Load(void)
{
	// 順番に注意：習得技・進化は、モンスターと技を先に読み込んでおく必要がある
	bool ok = true;
	ok &= LoadTypeChart("Data/CSVData/TypeChart.csv");
	ok &= LoadMoves("Data/CSVData/Move.csv");
	ok &= LoadMonsters("Data/CSVData/MonsterMaster.csv");
	ok &= LoadLearnset("Data/CSVData/MonsterLearnset.csv");
	ok &= LoadEvolution("Data/CSVData/MonsterEvolution.csv");
	ok &= LoadMoveAnims("Data/CSVData/MoveAnim.csv");

	//// 失敗したらパスやCSVの内容（タイプの綴りなど）を確認する
	//assert(ok && "CSVの読み込みに失敗しました");
}

void MonsterData::LoadEnd(void)
{
}

void MonsterData::Update(void)
{
}

void MonsterData::Draw(void)
{
}

void MonsterData::Release(void)
{
	moves_.clear();
	monsters_.clear();
	moveAnims_.clear();
}

bool MonsterData::LoadTypeChart(const std::string& path)
{
	// 未記入の部分は等倍(1.0)にしておく
	for (auto& row : typeChart_)
	{
		row.fill(1.0f);
	}

	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		return false;	// ファイルが開けない
	}

	const int max = static_cast<int>(ElementType::MAX);
	std::string line;
	for (int a = 0; a < max && std::getline(ifs, line); a++)
	{
		std::stringstream ss(line);
		std::string cell;
		for (int d = 0; d < max && std::getline(ss, cell, ','); d++)
		{
			typeChart_[a][d] = std::stof(cell);
		}
	}
	return true;
}

float MonsterData::GetTypeEffectiveness(ElementType atk, ElementType def1, ElementType def2) const
{
	if (atk == ElementType::NONE || def1 == ElementType::NONE)
	{
		return 1.0f;
	}

	float rate = typeChart_[static_cast<int>(atk)][static_cast<int>(def1)];
	if (def2 != ElementType::NONE)	// NONEは-1なので添字に使わない
	{
		rate *= typeChart_[static_cast<int>(atk)][static_cast<int>(def2)];
	}
	return rate;
}

// 技：#技ID,技名,タイプ,威力,命中率,最大PP,優先度,追加効果,効果の数値,効果の発動確率,説明,アニメID(省略可),相手用アニメID(省略可)
bool MonsterData::LoadMoves(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	moves_.clear();
	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 11)
			{
				return false;	// 列が足りない
			}

			MoveData m{};
			m.id = ToInt(r[0]);
			m.name = r[1];
			if (!ParseEnum(ElementTable(), r[2], m.type))
			{
				return false;	// タイプの綴り間違い
			}
			m.power = ToInt(r[3]);
			m.accuracy = ToInt(r[4]);
			m.pp = ToInt(r[5]);
			m.priority = ToInt(r[6]);
			if (!ParseEnum(EffectTable(), r[7], m.effect))
			{
				return false;	// 追加効果の綴り間違い
			}
			m.effectValue = ToInt(r[8]);
			m.effectChance = ToInt(r[9]);
			m.description = r[10];
			m.animId = (r.size() > 11) ? ToInt(r[11]) : 0;	// 技エフェクトのID（省略可）
			m.enemyAnimId = (r.size() > 12) ? ToInt(r[12]) : 0;	// 相手が使うときのエフェクトID（省略可）

			moves_[m.id] = m;
		}
	}
	catch (const std::exception&)
	{
		return false;	// 数字の列に数字以外が入っている
	}
	return true;
}

// モンスター：#図鑑番号,名前,タイプ1,タイプ2,HP,攻撃,防御,素早さ,成長率,基礎経験値,捕獲率,
//             アイコン,正面,背面,鳴き声,説明
bool MonsterData::LoadMonsters(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	monsters_.clear();
	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 16)
			{
				return false;
			}

			MonsterMasterData m{};
			m.id = ToInt(r[0]);
			m.name = r[1];
			if (!ParseEnum(ElementTable(), r[2], m.type1)
				|| !ParseEnum(ElementTable(), r[3], m.type2)
				|| !ParseEnum(GrowthTable(), r[8], m.growthRate))
			{
				return false;
			}
			m.baseHp = ToInt(r[4]);
			m.baseAttack = ToInt(r[5]);
			m.baseDefense = ToInt(r[6]);
			m.baseSpeed = ToInt(r[7]);
			m.baseExp = ToInt(r[9]);
			m.catchRate = ToInt(r[10]);
			m.iconPath = r[11];
			m.frontPath = r[12];
			m.backPath = r[13];
			m.crySoundPath = r[14];
			m.description = r[15];

			monsters_[m.id] = m;
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

// 習得技：#図鑑番号,習得レベル,技ID （モンスター・技を先に読み込むこと）
bool MonsterData::LoadLearnset(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	for (auto& pair : monsters_)
	{
		pair.second.learnset.clear();
	}

	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 3)
			{
				return false;
			}

			auto it = monsters_.find(ToInt(r[0]));
			if (it == monsters_.end() || GetMove(ToInt(r[2])) == nullptr)
			{
				return false;	// 存在しないモンスターか技を指している
			}

			LearnEntry e{};
			e.level = ToInt(r[1]);
			e.moveId = ToInt(r[2]);
			it->second.learnset.push_back(e);
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

// 進化：#図鑑番号,進化先図鑑番号,必要レベル,必要アイテムID
bool MonsterData::LoadEvolution(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	for (auto& pair : monsters_)
	{
		pair.second.evolutions.clear();
	}

	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 4)
			{
				return false;
			}

			auto it = monsters_.find(ToInt(r[0]));
			if (it == monsters_.end() || GetMonster(ToInt(r[1])) == nullptr)
			{
				return false;
			}

			Evolution e{};
			e.toId = ToInt(r[1]);
			e.level = ToInt(r[2]);
			e.itemId = ToInt(r[3]);
			it->second.evolutions.push_back(e);
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

// 技エフェクト：#アニメID,画像パス,1コマ幅,1コマ高さ,コマ数,横のコマ数,1コマのフレーム数,拡大率(%),繰り返し,合成(NORMAL/ADD),位置(TARGET/USER/CENTER)
bool MonsterData::LoadMoveAnims(const std::string& path)
{
	std::vector<std::vector<std::string>> rows;
	if (!ReadCsv(path, rows))
	{
		return false;
	}

	moveAnims_.clear();
	try
	{
		for (const auto& r : rows)
		{
			if (r.size() < 11)
			{
				return false;
			}

			MoveAnimData a{};
			a.id = ToInt(r[0]);
			a.imagePath = r[1];
			a.frameW = ToInt(r[2]);
			a.frameH = ToInt(r[3]);
			a.frameCount = ToInt(r[4]);
			a.columns = ToInt(r[5]);
			a.frameTime = ToInt(r[6]);
			a.scalePercent = ToInt(r[7]);
			a.loops = ToInt(r[8]);

			if (r[9] == "ADD")
			{
				a.additive = true;
			}
			else if (!r[9].empty() && r[9] != "NORMAL")
			{
				return false;	// 合成の綴り間違い
			}
			if (!ParseEnum(AnimPosTable(), r[10], a.pos))
			{
				return false;	// 位置の綴り間違い
			}

			// 空欄・0は既定値にする
			if (a.frameW <= 0 || a.frameH <= 0)
			{
				return false;	// コマのサイズは必須
			}
			if (a.frameCount <= 0) { a.frameCount = 1; }
			if (a.columns <= 0) { a.columns = a.frameCount; }
			if (a.frameTime <= 0) { a.frameTime = 3; }
			if (a.scalePercent <= 0) { a.scalePercent = 100; }
			if (a.loops <= 0) { a.loops = 1; }

			moveAnims_[a.id] = a;
		}
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}

const MoveAnimData* MonsterData::GetMoveAnim(int id) const
{
	auto it = moveAnims_.find(id);
	return (it != moveAnims_.end()) ? &it->second : nullptr;
}

const MoveData* MonsterData::GetMove(int id) const
{
	auto it = moves_.find(id);
	return (it != moves_.end()) ? &it->second : nullptr;
}

const MonsterMasterData* MonsterData::GetMonster(int id) const
{
	auto it = monsters_.find(id);
	return (it != monsters_.end()) ? &it->second : nullptr;
}