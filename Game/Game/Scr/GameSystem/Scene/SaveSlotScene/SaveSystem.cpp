#include "SaveSystem.h"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <DxLib.h>

namespace
{
	const char* SAVE_ROOT = "Data/Save";

	// スロットの中にあるファイル（削除用）。meta.csv を最初に消す＝先に「未セーブ」扱いにする
	const char* SLOT_FILES[] = { "meta.csv", "party.csv", "inventory.csv", "flags.csv", "game.csv", "game.csv.tmp" };
}

namespace SaveSystem
{
	bool IsValidSlot(int slot)
	{
		return slot >= 0 && slot < SLOT_COUNT;
	}

	std::string SlotDir(int slot)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%s/slot_%03d", SAVE_ROOT, slot);
		return buf;
	}

	std::string SlotFile(int slot, const char* fileName)
	{
		return SlotDir(slot) + "/" + fileName;
	}

	bool PrepareSlot(int slot)
	{
		if (!IsValidSlot(slot))
		{
			return false;
		}
		CreateDirectoryA(SAVE_ROOT, NULL);		// 既にあれば失敗するだけなので無視してよい
		CreateDirectoryA(SlotDir(slot).c_str(), NULL);
		// 作れたか（または既にあるか）は、フォルダ属性で確かめる
		const DWORD attr = GetFileAttributesA(SlotDir(slot).c_str());
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
	}

	bool ReadInfo(int slot, SlotInfo& out)
	{
		out = SlotInfo();
		if (!IsValidSlot(slot))
		{
			return false;
		}

		std::ifstream ifs(SlotFile(slot, "meta.csv"));
		if (!ifs.is_open())
		{
			return false;
		}

		// 1行目：META,1  2行目：日時  3行目：プレイ秒,ステージ,手持ち数,所持金
		std::string header, date, nums;
		std::getline(ifs, header);
		std::getline(ifs, date);
		std::getline(ifs, nums);
		if (header.compare(0, 4, "META") != 0)
		{
			return false;
		}
		while (!date.empty() && (date.back() == '\r' || date.back() == '\n'))
		{
			date.pop_back();
		}

		SlotInfo info;
		if (sscanf_s(nums.c_str(), "%d,%d,%d,%d",
			&info.playSeconds, &info.stageId, &info.partyCount, &info.money) != 4)
		{
			return false;
		}
		info.savedAt = date;
		info.exists = true;
		out = info;
		return true;
	}

	bool Exists(int slot)
	{
		SlotInfo info;
		return ReadInfo(slot, info);
	}

	bool WriteInfo(int slot, const SlotInfo& info)
	{
		if (!IsValidSlot(slot))
		{
			return false;
		}

		// 別名で書いてから差し替える（書き込み途中で失敗しても、元の meta が壊れない）
		const std::string path = SlotFile(slot, "meta.csv");
		const std::string tmpPath = path + ".tmp";
		{
			std::ofstream ofs(tmpPath, std::ios::trunc);
			if (!ofs.is_open())
			{
				return false;
			}
			ofs << "META,1\n";
			ofs << info.savedAt << "\n";
			ofs << info.playSeconds << "," << info.stageId << ","
				<< info.partyCount << "," << info.money << "\n";
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

	bool Delete(int slot)
	{
		if (!IsValidSlot(slot))
		{
			return false;
		}
		for (const char* name : SLOT_FILES)
		{
			std::remove(SlotFile(slot, name).c_str());
		}
		RemoveDirectoryA(SlotDir(slot).c_str());
		return !Exists(slot);
	}

	int FindEmptySlot(void)
	{
		for (int i = 0; i < SLOT_COUNT; i++)
		{
			if (!Exists(i))
			{
				return i;
			}
		}
		return -1;
	}

	std::string NowString(void)
	{
		const std::time_t now = std::time(nullptr);
		std::tm t{};
		localtime_s(&t, &now);
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d",
			t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
		return buf;
	}
}