#pragma once

#include <string>

// セーブスロットの管理（フォルダの場所、一覧用の情報、削除など）
//
// 保存先：Data/Save/slot_000 〜 slot_099 （1スロット＝1フォルダ）
//   party.csv / inventory.csv / flags.csv / game.csv … 中身は従来の SaveGame と同じ
//   meta.csv … スロット一覧に出す情報（これが書けて初めて「セーブ済み」とみなす）
//
// スロット数を増やしたいときは SLOT_COUNT を変えるだけ（フォルダ名は3桁なので999まで）
namespace SaveSystem
{
	constexpr int SLOT_COUNT = 100;

	// 一覧に出す情報
	struct SlotInfo
	{
		bool exists = false;		// セーブデータがあるか
		std::string savedAt;		// 保存した日時（例：2026/10/22 14:03）
		int playSeconds = 0;		// プレイ時間（秒）
		int stageId = 0;			// いたステージ
		int partyCount = 0;			// 手持ちの数
		int money = 0;				// 所持金
	};

	bool IsValidSlot(int slot);								// 0〜SLOT_COUNT-1 か
	std::string SlotDir(int slot);							// 例："Data/Save/slot_003"
	std::string SlotFile(int slot, const char* fileName);	// 例："Data/Save/slot_003/party.csv"

	bool PrepareSlot(int slot);								// 保存先フォルダを作る（あれば何もしない）
	bool Exists(int slot);									// セーブ済みか
	bool ReadInfo(int slot, SlotInfo& out);					// 一覧用の情報を読む（無ければ false。out.exists も false）
	bool WriteInfo(int slot, const SlotInfo& info);			// 一覧用の情報を書く（セーブの最後に呼ぶ）
	bool Delete(int slot);									// スロットを消す
	int  FindEmptySlot(void);								// 最初の空きスロット（全部埋まっていれば -1）

	std::string NowString(void);							// 今の日時の文字列
}