#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../SceneBase.h"
#include "Novel.hpp"	// ※このファイルと同じフォルダに置く

class MonsterData;
class MonsterParty;
class ItemData;
class Inventory;
class BattleScene;

// ストーリー（ノベルパート）を再生するシーン
//
// 使い方（フィールド側）：
//   auto novel = std::make_shared<NovelScene>();
//   novel->Setup("Data/Story/prologue.txt", &monsterData_, &monsterParty_, &itemData_, &inventory_);
//   SceneManager::GetInstance()->PushScene(novel);
//   → スクリプトが終わると自動で PopScene して、元の画面に戻る
//
// スクリプトから使える、ゲーム専用のコマンド：
//   @give 図鑑番号 レベル   手持ちにモンスターを加える（満員なら警告だけ出して何もしない）
//   @heal                   手持ちを全回復
//   @battle 図鑑番号 レベル 野生バトル。終わると変数 battle_result が入る（1=勝ち 0=負け 2=逃げた 3=捕まえた）
class NovelScene : public SceneBase
{
public:

	NovelScene(void);				// コンストラクタ
	~NovelScene(void) override;		// デストラクタ

public:

	void Init(void)		override;	// 初期化（スクリプトの読み込みもここ）
	void Load(void)		override;	// 読み込み
	void LoadEnd(void)	override;	// 読み込み後の処理
	void Update(void)	override;	// 更新
	void Draw(void)		override;	// 描画
	void Release(void)	override;	// 解放

public:

	// PushScene の前に呼ぶ。items / inventory を渡すと、スクリプトのバトルでバッグが使える（省略するとバッグは空扱い）
	void Setup(const std::string& scriptPath, const MonsterData* data, MonsterParty* party,
		const ItemData* items = nullptr, Inventory* inventory = nullptr);

	// スクリプト内の変数を読む（終了後のフラグ確認用。PopScene後もshared_ptrを持っていれば読める）
	int GetVar(const std::string& name) const { return novel_.GetVar(name); }

private:

	// スクリプトの未知コマンドを受け取る（処理したらtrue）
	bool OnCommand(const std::string& name, const std::vector<std::string>& args);

private:

	Novel::Engine novel_;

	std::string scriptPath_;
	const MonsterData* data_ = nullptr;
	MonsterParty* party_ = nullptr;
	const ItemData* items_ = nullptr;
	Inventory* inventory_ = nullptr;

	// @battle で積んだバトル。終わるまで、ノベルの進行を止める
	std::shared_ptr<BattleScene> battle_;
};