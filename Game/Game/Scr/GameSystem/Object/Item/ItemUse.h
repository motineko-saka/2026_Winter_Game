#pragma once
#include <string>
#include "ItemData.h"

class MonsterData;
struct MonsterInstance;

// アイテムの効果をモンスターに適用する処理（状態を持たないのでstatic）
// 戦闘（BattleScene）とフィールドのバッグ画面の両方から同じ処理を呼ぶ。
// 効果の数値や対象は Item.csv で決まるので、ここは基本的にいじらなくてよい。
class ItemUse
{
public:

	// モンスターを選んで使うタイプのアイテムか
	static bool NeedsTarget(const ItemMasterData& item);

	// そのモンスターに今使って意味があるか（だめなら reason に理由が入る）
	static bool CanUseOnMonster(const MonsterData& data, const ItemMasterData& item,
		const MonsterInstance& mon, std::string& reason);

	// 実際に効果を適用する。成功したら true を返して message に結果の文章を入れる。
	// 使えない場合は何も変更せず false を返す（reason と同じ文章が message に入る）。
	// ※ EVOLVE（進化アイテム）はここでは扱わない。GetItemEvolution で進化先を調べて、
	//   進化の処理は呼び出し側で行うこと。
	static bool ApplyToMonster(const MonsterData& data, const ItemMasterData& item,
		MonsterInstance& mon, std::string& message);

	// 進化アイテムを使ったときの進化先の図鑑番号（進化しなければ0）
	// MonsterEvolution.csv の「必要アイテムID」が一致し、必要レベルを満たしているものを探す
	static int GetItemEvolution(const MonsterData& data, const MonsterInstance& mon, int itemId);
};
