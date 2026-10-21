#include "GameFlags.h"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace
{
	std::string Trim(const std::string& s)
	{
		const size_t a = s.find_first_not_of(" \t");
		const size_t b = s.find_last_not_of(" \t");
		return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
	}

	std::string StripDollar(const std::string& s)
	{
		return (!s.empty() && s[0] == '$') ? s.substr(1) : s;
	}

	// 数字だけの文字列か
	bool ParseInt(const std::string& s, int& out)
	{
		if (s.empty())
		{
			return false;
		}
		char* end = nullptr;
		const long v = std::strtol(s.c_str(), &end, 10);
		if (end == nullptr || *end != '\0')
		{
			return false;
		}
		out = static_cast<int>(v);
		return true;
	}
}

int GameFlags::Get(const std::string& name) const
{
	auto it = flags_.find(StripDollar(name));
	return (it != flags_.end()) ? it->second : 0;
}

void GameFlags::Set(const std::string& name, int value)
{
	const std::string key = StripDollar(name);
	if (key.empty())
	{
		return;
	}
	flags_[key] = value;
}

bool GameFlags::Check(const std::string& condition) const
{
	// & で区切った項目を、すべて満たすかを調べる
	size_t start = 0;
	while (start <= condition.size())
	{
		size_t amp = condition.find('&', start);
		if (amp == std::string::npos)
		{
			amp = condition.size();
		}
		const std::string term = Trim(condition.substr(start, amp - start));
		start = amp + 1;

		if (term.empty())
		{
			continue;	// 空の項目（条件なし）は満たしている扱い
		}

		// 比較演算子を探す（2文字のものを先に）
		static const char* const OPS[] = { "==", "!=", ">=", "<=", ">", "<" };
		std::string op;
		size_t pos = std::string::npos;
		for (const char* o : OPS)
		{
			const size_t p = term.find(o);
			if (p != std::string::npos && (pos == std::string::npos || p < pos))
			{
				pos = p;
				op = o;
			}
		}

		bool ok = false;
		if (pos == std::string::npos)
		{
			// フラグ単体： name / !name
			std::string name = term;
			bool neg = false;
			if (name[0] == '!')
			{
				neg = true;
				name = Trim(name.substr(1));
			}
			if (name.empty())
			{
				return false;
			}
			ok = (Get(name) != 0) != neg;
		}
		else
		{
			const std::string lhs = Trim(term.substr(0, pos));
			const std::string rhs = Trim(term.substr(pos + op.size()));
			int rv = 0;
			if (lhs.empty() || !ParseInt(rhs, rv))
			{
				return false;	// 書式が壊れている
			}
			const int lv = Get(lhs);
			if (op == "==")			ok = (lv == rv);
			else if (op == "!=")	ok = (lv != rv);
			else if (op == ">=")	ok = (lv >= rv);
			else if (op == "<=")	ok = (lv <= rv);
			else if (op == ">")		ok = (lv > rv);
			else					ok = (lv < rv);
		}

		if (!ok)
		{
			return false;
		}
	}
	return true;
}

// 1行目：FLAGS,1   2行目以降：名前,値
bool GameFlags::Save(const std::string& path) const
{
	std::ofstream ofs(path, std::ios::trunc);
	if (!ofs.is_open())
	{
		return false;
	}

	ofs << "FLAGS,1\n";
	for (const auto& kv : flags_)
	{
		if (kv.second != 0)		// OFF(0) は持っていないのと同じなので書かない
		{
			ofs << kv.first << "," << kv.second << "\n";
		}
	}
	ofs.flush();
	return static_cast<bool>(ofs);
}

bool GameFlags::Load(const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs.is_open())
	{
		return false;
	}

	std::string line;
	if (!std::getline(ifs, line))
	{
		return false;
	}
	if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
	{
		line.erase(0, 3);
	}
	if (!line.empty() && line.back() == '\r')
	{
		line.pop_back();
	}
	if (line.compare(0, 6, "FLAGS,") != 0)
	{
		return false;
	}

	std::map<std::string, int> loaded;
	while (std::getline(ifs, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		const size_t comma = line.rfind(',');
		if (comma == std::string::npos || comma == 0)
		{
			continue;
		}
		int v = 0;
		if (ParseInt(Trim(line.substr(comma + 1)), v))
		{
			loaded[Trim(line.substr(0, comma))] = v;
		}
	}

	flags_ = loaded;
	return true;
}
