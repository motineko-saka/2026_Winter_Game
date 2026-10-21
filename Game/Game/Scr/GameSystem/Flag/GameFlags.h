#pragma once
#include <map>
#include <string>

// ゲーム全体で共有するフラグ（RPGツクールの「スイッチ／変数」にあたる）
//
//  ・名前（文字列）→ 整数値。持っていない名前は 0（OFF）として扱う
//  ・ノベルの台本からは「$名前」で読み書きできる
//        @on met_doctor              … met_doctor を ON(1) にする
//        @off met_doctor             … OFF(0) にする
//        @set $gold += 10            … 数値としても使える
//        @if $met_doctor ... @endif
//  ・Event.csv の「条件」欄でも使える（Check を参照）
//  ・セーブ／ロード対応（Data/Save/flags.csv）
class GameFlags
{
public:

	GameFlags() = default;

	void Clear(void) { flags_.clear(); }

	int  Get(const std::string& name) const;			// 値（なければ0）
	bool IsOn(const std::string& name) const { return Get(name) != 0; }
	void Set(const std::string& name, int value);
	void On(const std::string& name) { Set(name, 1); }
	void Off(const std::string& name) { Set(name, 0); }

	// 条件の文字列を判定する。空文字なら常に true。
	//   "met_doctor"            … ON のとき
	//   "!met_doctor"           … OFF のとき
	//   "gold>=100"             … 比較（== != >= <= > < が使える）
	//   "met_doctor&!got_item"  … & でつなぐと、すべて満たすときだけ true
	// 名前の先頭の $ は付けても付けなくてもよい。書式が壊れている条件は false。
	bool Check(const std::string& condition) const;

	// ノベルエンジンに渡すための実体（キーは $ なし）
	std::map<std::string, int>& Map(void) { return flags_; }
	const std::map<std::string, int>& Map(void) const { return flags_; }

	// セーブ先のフォルダは先に作っておくこと。失敗したら false
	bool Save(const std::string& path) const;

	// 読み込む。失敗（ファイルなし・形式違い）したら今の中身は変えずに false
	bool Load(const std::string& path);

private:

	std::map<std::string, int> flags_;
};
