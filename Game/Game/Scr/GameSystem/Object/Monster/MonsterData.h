#pragma once
#include <string>
#include <vector>

class MonsterData
{
	// 技のデータ構造体
	struct MoveData
	{
		int id;                      // 技のID
		std::string name;            // 技の名前
		int power;                   // 技の威力
		int type;                    // 技のタイプ
	};

	// モンスターのデータ構造体
	struct MonsterMasterData
	{
		// モンスターの基本情報
		int id;                      // 図鑑番号
		std::string name;            // 名前
		int type1;                   // タイプ1
		int type2;                   // タイプ2

		// 種族値
		int baseHp;                  // 基礎HP
		int baseAttack;              // 基礎攻撃
		int baseDefense;             // 基礎防御
		int baseSpeed;               // 基礎素早さ

		// リソースファイルパス
		std::string iconPath;        // アイコン画像のパス
		std::string frontPath;       // 正面画像のパス
		std::string backPath;        // 背面画像のパス
		std::string crySoundPath;    // 鳴き声のファイルパス

		// 技のリスト
		std::vector<MoveData> moveList; // 技のリスト
	};

public:

	MonsterData();
	~MonsterData();

public:

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void LoadEnd(void);	// 読み込み後の処理
	void Update(void);	// 更新
	void Draw(void);	// 描画
	void Release(void);	// 解放

private:

};

