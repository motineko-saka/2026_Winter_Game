#pragma once
#include <string>
#include <vector>

// イベントが動き出すきっかけ
enum class EventTrigger
{
	ACTION,		// 近く（同じマスか上下左右の隣）で Enter / Space / Z を押したとき（話しかける・調べる）
	TOUCH,		// そのマスに足を踏み入れたとき
	AUTO,		// 条件を満たしていれば、そのマップに来たとき自動で（1回のマップ訪問につき1度だけ）
};

// マップ上のイベント1つ分（RPGツクールの「イベント」＋「出現条件」にあたる）
struct EventSpawn
{
	int id;						// イベントID（1から。重複しないこと）
	int stageId;				// 置くステージ
	int tileX;					// 置くマス(X)
	int tileY;					// 置くマス(Y)
	EventTrigger trigger;		// きっかけ
	std::string condition;		// 出現条件（GameFlags::Check の書式。空なら常に）
	std::string script;			// 再生する台本（Data/Story/xxx.txt）
	bool once;					// true なら、1度やったら二度と出ない（完了フラグ event_<ID>_done が立つ）
	std::string image;			// 画像（空なら見えない＝踏む場所や、見えない会話スポット）
								// 書式はトレーナーと同じ（Trainer.csv の画像の欄を参照）。画像があるとそのマスは通行不可になる
};

// イベントの定義を管理するクラス（TrainerData と同じ作り）
//   Data/CSVData/Event.csv
//   #ID,ステージID,マスX,マスY,きっかけ(ACTION/TOUCH/AUTO),出現条件,台本,1回だけ(0/1),画像
class EventData
{
public:

	EventData() = default;

	void Init(void);	// 初期化
	void Load(void);	// 読み込み
	void Release(void);	// 解放

	// 読み込む（失敗したら false）
	bool LoadEvents(const std::string& path);

	const std::vector<EventSpawn>& GetEvents(void) const { return events_; }

	// 「1度だけ」のイベントが終わったときに立つフラグの名前
	static std::string DoneFlagName(int eventId);

private:

	std::vector<EventSpawn> events_;
};
