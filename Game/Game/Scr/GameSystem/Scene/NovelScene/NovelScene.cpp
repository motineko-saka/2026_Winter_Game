#include "NovelScene.h"
#include <cstdlib>
#include "../SceneManager.h"
#include "../BattleScene/BattleScene.h"
#include "../../../AppSystem/InputManager/InputManager.h"
#include "../../Object/Monster/MonsterData.h"
#include "../../Object/Monster/MonsterParty.h"
#include "../../Object/Item/Inventory.h"
#include "../../Flag/GameFlags.h"

NovelScene::NovelScene(void)
{
}

NovelScene::~NovelScene(void)
{
}

void NovelScene::Setup(const std::string& scriptPath, const MonsterData* data, MonsterParty* party,
	const ItemData* items, Inventory* inventory, GameFlags* flags)
{
	scriptPath_ = scriptPath;
	data_ = data;
	party_ = party;
	items_ = items;
	inventory_ = inventory;
	flags_ = flags;
}

void NovelScene::Init(void)
{
	battle_.reset();

	// 文字コードは DxLib の設定に合わせる（スクリプトファイルもその文字コードで保存すること）
	novel_.cfg.utf8 = (GetUseCharCodeFormat() == DX_CHARCODEFORMAT_UTF8);

	// キーボード操作のみ（マウスは使わない）
	novel_.cfg.useMouse = false;

	// ゲーム全体のフラグを、台本の $名前 から使えるようにする
	if (flags_ != nullptr)
	{
		novel_.SetGlobalFlags(&flags_->Map());
	}

	// 素材やラベルが見つからないときの警告を、出力ウィンドウに出す
	novel_.onWarning = [](const std::string& m)
		{
			OutputDebugStringA(("[novel] " + m + "\n").c_str());
		};

	novel_.SetCommandHandler([this](const std::string& name, const std::vector<std::string>& args)
		{
			return OnCommand(name, args);
		});

	// 読み込めなければ何も表示せず、最初の Update で閉じる（Load が警告を出す）
	novel_.Load(scriptPath_);

	// ゲーム側の状態をスクリプトから見られるようにする（@if party_count >= 2 ... など）
	if (party_ != nullptr)
	{
		novel_.SetVar("party_count", party_->GetCount());
	}
}

void NovelScene::Load(void)
{
}

void NovelScene::LoadEnd(void)
{
}

void NovelScene::Update(void)
{
	// バトルから戻ってきた：結果を変数に入れて、ノベルを再開する
	if (battle_ != nullptr)
	{
		const BattleScene::Result result = battle_->GetResult();
		if (result == BattleScene::Result::NONE)
		{
			return;	// まだ戦闘中
		}

		int value = 2;	// 逃げた
		if (result == BattleScene::Result::WIN)
		{
			value = 1;
		}
		else if (result == BattleScene::Result::LOSE)
		{
			value = 0;
		}
		else if (result == BattleScene::Result::CAUGHT)
		{
			value = 3;
		}
		novel_.SetVar("battle_result", value);
		if (party_ != nullptr)
		{
			novel_.SetVar("party_count", party_->GetCount());
		}
		battle_.reset();
		novel_.Wait(0);	// 待ち状態を解除して、@battle の次の行から再開する
	}

	novel_.Update();

	// スクリプトが終わったら元の画面に戻る（PopScene後は this を触らない）
	if (novel_.IsFinished())
	{
		SceneManager::GetInstance()->PopScene();
	}
}

void NovelScene::Draw(void)
{
	novel_.Draw();
}

void NovelScene::Release(void)
{
	novel_.ReleaseAssets();
	battle_.reset();
}

bool NovelScene::OnCommand(const std::string& name, const std::vector<std::string>& args)
{
	// @item 道具番号 個数  （バッグだけを使うので、手持ちがなくても動く）
	if (name == "item")
	{
		if (args.empty() || inventory_ == nullptr)
		{
			return false;
		}
		const int itemId = std::atoi(args[0].c_str());
		const int count = (args.size() > 1) ? std::atoi(args[1].c_str()) : 1;
		if (count >= 0)
		{
			return inventory_->Add(itemId, count);
		}
		return inventory_->Remove(itemId, -count);
	}

	if (data_ == nullptr || party_ == nullptr)
	{
		return false;
	}

	// @give 図鑑番号 レベル
	if (name == "give")
	{
		if (args.size() < 2)
		{
			return false;
		}
		MonsterInstance monster;
		if (!MonsterParty::Create(*data_, std::atoi(args[0].c_str()), std::atoi(args[1].c_str()), monster)
			|| !party_->Add(monster))
		{
			return false;	// 存在しない図鑑番号、または手持ちが満員（警告に記録される）
		}
		novel_.SetVar("party_count", party_->GetCount());
		return true;
	}

	// @heal
	if (name == "heal")
	{
		party_->HealAll(*data_);
		return true;
	}

	// @battle 図鑑番号 レベル
	if (name == "battle")
	{
		if (args.size() < 2)
		{
			return false;
		}
		battle_ = std::make_shared<BattleScene>();
		battle_->Setup(data_, party_, std::atoi(args[0].c_str()), std::atoi(args[1].c_str()));
		battle_->SetItems(items_, inventory_);
		SceneManager::GetInstance()->PushScene(battle_);

		// バトルが終わるまでノベルを止める（再開は Update 側）
		novel_.Wait(1000000000);
		return true;
	}

	return false;
}