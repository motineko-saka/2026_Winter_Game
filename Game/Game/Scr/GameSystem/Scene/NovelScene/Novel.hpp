// Novel.hpp - embedded novel-part engine for DxLib (C++14, single header). See README.md.
// (Japanese comments removed so the file is safe in any source encoding.)
//
//
//   Novel::Engine novel;
//   novel.Load("DATA/STR/sample.txt");
//   while (ProcessMessage() == 0 && !novel.IsFinished()) {
//       ClearDrawScreen();
//       ScreenFlip();
//   }
#pragma once

#include "DxLib.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Novel {

	// ---------------------------------------------------------------------------
	// ---------------------------------------------------------------------------
	struct Config {
		int screenW = 0, screenH = 0;

		int fontHandle = -1;

		bool utf8 = false;

		std::string bgDir = "DATA/BACKGROUND";
		std::string charaDir = "DATA/CHARACTER";
		std::string bgmDir = "DATA/BACKGROUNDMUSIC";
		std::string seDir = "DATA/SOUNDEFFECT";

		double charsPerSec = 30.0;
		int autoWaitMs = 800;
		int autoPerCharMs = 50;

		int boxMargin = 10, boxHeight = 0, boxPadding = 14;
		int boxRGB = 0x000000, boxAlpha = 170;   // alpha: 0-255
		int textRGB = 0xFFFFFF, nameRGB = 0xFFE08A, choiceSelRGB = 0xFFE08A;

		int bgmVolume = 100, seVolume = 100;

		int backlogMax = 200;

		// false: mouse (click / wheel / hover) is ignored. Keyboard only.
		bool useMouse = false;
	};

	// ---------------------------------------------------------------------------
	// ---------------------------------------------------------------------------
	struct Input {
		bool ok = false;
		bool click = false;
		bool skip = false;
		bool toggleAuto = false;
		bool up = false, down = false;
		bool backlog = false;
		bool cancel = false;
		int wheel = 0;
		int mouseX = -1, mouseY = -1;
		int dtMs = 16;
	};

	using CommandHandler =
		std::function<bool(const std::string& name, const std::vector<std::string>& args)>;

	// ---------------------------------------------------------------------------
	class Engine {
	public:
		Config cfg;

		Engine() = default;
		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;
		~Engine() { ReleaseAssets(); }

		bool Load(const std::string& path) {
			std::ifstream f(path.c_str(), std::ios::in | std::ios::binary);
			if (!f) { Warn("script not found: " + path); return false; }
			std::stringstream ss; ss << f.rdbuf();
			scriptPath_ = path;
			return Parse(ss.str());
		}
		bool LoadFromString(const std::string& text) {
			scriptPath_.clear();
			return Parse(text);
		}

		bool IsFinished() const { return state_ == State::Finished; }
		bool IsRunning()  const { return !IsFinished(); }

		bool Update() { return Update(ReadInput()); }
		bool Update(const Input& in) {
			if (state_ == State::Finished) return false;
			if (UpdateBacklog(in)) return true;

			if (in.toggleAuto) auto_ = !auto_;

			// While a page is showing: Down = next page, Up = open backlog.
			// (While choosing, Up/Down move the cursor instead.)
			const bool textState = (state_ == State::Typing || state_ == State::WaitClick);
			if (in.backlog || in.wheel > 0 || (textState && in.up)) { OpenBacklog(); return true; }

			const bool advance = in.ok || in.click || (textState && in.down);
			switch (state_) {
			case State::Running: RunUntilBlock(); break;

			case State::Typing:
				if (advance || in.skip) shown_ = (double)chars_.size();
				else shown_ += cfg.charsPerSec * in.dtMs / 1000.0;
				if (shown_ >= (double)chars_.size()) { shown_ = (double)chars_.size(); state_ = State::WaitClick; idleMs_ = 0; }
				break;

			case State::WaitClick: {
				idleMs_ += in.dtMs;
				const int autoTime = cfg.autoWaitMs + (int)chars_.size() * cfg.autoPerCharMs;
				if (advance || in.skip || (auto_ && idleMs_ >= autoTime)) { state_ = State::Running; RunUntilBlock(); }
				break;
			}

			case State::Wait:
				waitMs_ -= in.dtMs;
				if (waitMs_ <= 0 || in.skip) { state_ = State::Running; RunUntilBlock(); }
				break;

			case State::Choice: UpdateChoice(in); break;
			default: break;
			}
			return state_ != State::Finished;
		}

		void Draw() const {
			const int W = ScreenW(), H = ScreenH();
			if (bg_ >= 0) DrawExtendGraph(0, 0, W, H, bg_, TRUE);
			for (const auto& c : charas_) DrawChara(c);

			if (state_ == State::Finished) return;
			if (backlogOpen_) { DrawBacklog(); return; }

			if (!page_.empty() || state_ == State::Typing || state_ == State::WaitClick) DrawMessage();
			if (state_ == State::Choice) DrawChoices();
		}

		void Run() {
			while (ProcessMessage() == 0 && !IsFinished()) {
				ClearDrawScreen();
				Update();
				Draw();
				ScreenFlip();
			}
		}

		void SetCommandHandler(CommandHandler h) { handler_ = std::move(h); }

		// Variables.
		//   "name"  : local to this script (cleared every time a script is loaded)
		//   "$name" : global flag shared by the whole game (see SetGlobalFlags). Kept between scripts.
		int  GetVar(const std::string& n) const { return ReadVar(n); }
		void SetVar(const std::string& n, int v) { WriteVar(n, v); }
		const std::map<std::string, int>& Vars() const { return vars_; }

		// Give the engine the game-wide flag table. Keys are stored WITHOUT the leading '$'.
		// The table must outlive the engine. Pass nullptr to go back to local-only.
		void SetGlobalFlags(std::map<std::string, int>* g) { globals_ = g; }

		void Wait(int ms) { waitMs_ = ms; state_ = State::Wait; }

		const std::vector<std::string>& Warnings() const { return warnings_; }
		std::function<void(const std::string&)> onWarning;

		struct LogEntry { std::string speaker, text; };
		const std::vector<LogEntry>& Backlog() const { return backlog_; }

		std::string Save() const {
			if (scriptPath_.empty()) return "";
			std::ostringstream o;
			o << "novel1\n";
			o << "script\t" << scriptPath_ << "\n";
			o << "pc\t" << ResumePc() << "\n";
			o << "speaker\t" << speaker_ << "\n";
			o << "bg\t" << bgName_ << "\n";
			o << "bgm\t" << bgmName_ << "\n";
			for (const auto& c : charas_) o << "chara\t" << c.id << "\t" << c.file << "\t" << c.pos << "\n";
			for (const auto& v : vars_) o << "var\t" << v.first << "\t" << v.second << "\n";
			return o.str();
		}
		bool Restore(const std::string& data) {
			std::istringstream in(data);
			std::string line;
			if (!std::getline(in, line) || line != "novel1") return false;
			std::string script, speaker, bg, bgm; int pc = 0;
			struct C { std::string id, file, pos; }; std::vector<C> cs;
			std::map<std::string, int> vars;
			while (std::getline(in, line)) {
				auto f = SplitTab(line);
				if (f.empty()) continue;
				if (f[0] == "script" && f.size() > 1) script = f[1];
				else if (f[0] == "pc" && f.size() > 1) pc = std::atoi(f[1].c_str());
				else if (f[0] == "speaker") speaker = f.size() > 1 ? f[1] : "";
				else if (f[0] == "bg") bg = f.size() > 1 ? f[1] : "";
				else if (f[0] == "bgm") bgm = f.size() > 1 ? f[1] : "";
				else if (f[0] == "chara" && f.size() > 3) cs.push_back({ f[1], f[2], f[3] });
				else if (f[0] == "var" && f.size() > 2) vars[f[1]] = std::atoi(f[2].c_str());
			}
			if (script.empty() || !Load(script)) return false;
			pc_ = std::min(std::max(pc, 0), (int)lines_.size());
			speaker_ = speaker; vars_ = vars;
			if (!bg.empty()) CmdBg(bg);
			for (auto& c : cs) CmdChara(c.id, c.file, c.pos);
			if (!bgm.empty()) CmdBgm(bgm);
			return true;
		}

		void ReleaseAssets() {
			StopBgm();
			for (auto& kv : gcache_) if (kv.second >= 0) DeleteGraph(kv.second);
			for (auto& kv : scache_) if (kv.second >= 0) DeleteSoundMem(kv.second);
			gcache_.clear(); scache_.clear();
			bg_ = -1; charas_.clear();
		}

	private:
		enum class State { Running, Typing, WaitClick, Wait, Choice, Finished };
		enum class LineType { Text, Cmd, Label, Speaker };
		struct Line {
			LineType type = LineType::Text;
			std::string text;
			std::vector<std::string> args;
			int target = -1;   // block @if: line to go to when false / @else: line to go to (the @endif)
		};
		struct Chara { std::string id, file, pos; int handle; };
		struct Choice { std::string text, label; };

		bool Parse(const std::string& src) {
			lines_.clear(); labels_.clear(); warnings_.clear();
			vars_.clear(); charas_.clear(); backlog_.clear();
			page_.clear(); chars_.clear(); speaker_.clear();
			bg_ = -1; bgName_.clear(); StopBgm(); bgmName_.clear();
			pc_ = 0; pageLine_ = choiceLine_ = 0; state_ = State::Running;
			backlogOpen_ = false; auto_ = false;

			std::istringstream in(src);
			std::string raw; bool first = true;
			struct Open { int ifIdx; int elseIdx; };
			std::vector<Open> blocks;   // open block @if (for matching @else / @endif)
			int lineNo = 0;
			while (std::getline(in, raw)) {
				++lineNo;
				if (first && raw.compare(0, 3, "\xEF\xBB\xBF") == 0) raw.erase(0, 3);  // BOM
				first = false;
				while (!raw.empty() && (raw.back() == '\r' || raw.back() == ' ' || raw.back() == '\t')) raw.pop_back();
				if (raw.empty() || raw.compare(0, 2, "//") == 0) continue;

				Line l;
				if (raw[0] == '@') {
					auto t = Tokenize(raw.substr(1));
					if (t.empty()) continue;
					l.type = LineType::Cmd; l.text = Lower(t[0]); t.erase(t.begin()); l.args = std::move(t);

					// Match block @if / @else / @endif
					const int idx = (int)lines_.size();
					if (l.text == "if" && IsBlockIf(l.args)) {
						blocks.push_back({ idx, -1 });
					}
					else if (l.text == "else") {
						if (blocks.empty() || blocks.back().elseIdx >= 0) Warn("line " + std::to_string(lineNo) + ": @else without @if");
						else blocks.back().elseIdx = idx;
					}
					else if (l.text == "endif") {
						if (blocks.empty()) Warn("line " + std::to_string(lineNo) + ": @endif without @if");
						else {
							const Open o = blocks.back(); blocks.pop_back();
							if (o.elseIdx >= 0) { lines_[o.ifIdx].target = o.elseIdx + 1; lines_[o.elseIdx].target = idx; }
							else lines_[o.ifIdx].target = idx;
						}
					}
				}
				else if (raw[0] == '*') {
					l.type = LineType::Label; l.text = Trim(raw.substr(1));
					labels_[l.text] = (int)lines_.size();
				}
				else if (raw[0] == '#') {
					l.type = LineType::Speaker; l.text = Trim(raw.substr(1));
				}
				else {
					l.type = LineType::Text; l.text = ReplaceAll(raw, "\\n", "\n");
				}
				lines_.push_back(std::move(l));
			}
			if (!blocks.empty()) Warn("@if without @endif (opened at line index " + std::to_string(blocks.back().ifIdx) + ")");
			return true;
		}

		// @if forms:
		//   @if flag                    (1 arg)  block, true when flag != 0     ("!flag" = true when 0)
		//   @if a op b                  (3 args) block
		//   @if flag label              (2 args) jump to label when flag != 0
		//   @if a op b label            (4 args) jump to label when true
		static bool IsBlockIf(const std::vector<std::string>& a) { return a.size() == 1 || a.size() == 3; }

		static std::vector<std::string> Tokenize(const std::string& s) {
			std::vector<std::string> out; std::string cur; bool q = false, has = false;
			for (char c : s) {
				if (c == '"') { q = !q; has = true; }
				else if (!q && (c == ' ' || c == '\t')) { if (has) { out.push_back(cur); cur.clear(); has = false; } }
				else { cur += c; has = true; }
			}
			if (has) out.push_back(cur);
			return out;
		}
		static std::string Trim(const std::string& s) {
			size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
			return a == std::string::npos ? "" : s.substr(a, b - a + 1);
		}
		static std::string Lower(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }
		static std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
			for (size_t p = 0; (p = s.find(from, p)) != std::string::npos; p += to.size()) s.replace(p, from.size(), to);
			return s;
		}
		static std::vector<std::string> SplitTab(const std::string& s) {
			std::vector<std::string> o; std::string cur;
			for (char c : s) { if (c == '\t') { o.push_back(cur); cur.clear(); } else if (c != '\r') cur += c; }
			o.push_back(cur); return o;
		}

		void RunUntilBlock() {
			for (int guard = 0; state_ == State::Running && guard < 100000; ++guard) {
				if (pc_ >= (int)lines_.size()) { state_ = State::Finished; return; }
				const Line& l = lines_[pc_];
				const int cur = pc_++;
				switch (l.type) {
				case LineType::Label: break;
				case LineType::Speaker: speaker_ = l.text; break;
				case LineType::Text: StartPage(l.text, cur); break;
				case LineType::Cmd: Exec(l, cur); break;
				}
			}
		}

		void StartPage(const std::string& text, int lineIdx) {
			pageLine_ = lineIdx;
			page_ = text;
			Layout();
			shown_ = 0; idleMs_ = 0;
			backlog_.push_back({ speaker_, text });
			if ((int)backlog_.size() > cfg.backlogMax) backlog_.erase(backlog_.begin());
			state_ = State::Typing;
		}

		// Number literal, or the value of a variable ("name" local / "$name" global).
		bool Truthy(const std::string& tok, int& out) const {
			char* end = nullptr;
			long v = std::strtol(tok.c_str(), &end, 10);
			if (end && *end == '\0' && !tok.empty()) { out = (int)v; return true; }
			out = ReadVar(tok); return false;
		}
		int Val(const std::string& tok) const { int v; Truthy(tok, v); return v; }

		int ReadVar(const std::string& n) const {
			if (!n.empty() && n[0] == '$' && globals_) {
				auto it = globals_->find(n.substr(1));
				return it == globals_->end() ? 0 : it->second;
			}
			auto it = vars_.find(n);
			return it == vars_.end() ? 0 : it->second;
		}
		void WriteVar(const std::string& n, int v) {
			if (!n.empty() && n[0] == '$' && globals_) (*globals_)[n.substr(1)] = v;
			else vars_[n] = v;
		}

		// Evaluate the condition part of @if (see IsBlockIf). Returns false (and warns) if it is malformed.
		bool EvalCond(const std::vector<std::string>& a) {
			if (a.size() == 1 || a.size() == 2) {
				std::string t = a[0]; bool neg = false;
				if (!t.empty() && t[0] == '!') { neg = true; t.erase(0, 1); }
				const bool r = Val(t) != 0;
				return neg ? !r : r;
			}
			if (a.size() >= 3) {
				const int lhs = Val(a[0]), rhs = Val(a[2]); const std::string& op = a[1];
				if (op == "==") return lhs == rhs;
				if (op == "!=") return lhs != rhs;
				if (op == ">=") return lhs >= rhs;
				if (op == "<=") return lhs <= rhs;
				if (op == ">")  return lhs > rhs;
				if (op == "<")  return lhs < rhs;
				Warn("@if: unknown operator '" + op + "'");
				return false;
			}
			Warn("@if: missing condition");
			return false;
		}

		void Exec(const Line& l, int curIdx) {
			const std::string& c = l.text; const auto& a = l.args;
			auto arg = [&](size_t i) -> std::string { return i < a.size() ? a[i] : std::string(); };

			if (c == "bg") CmdBg(arg(0));
			else if (c == "chara") CmdChara(arg(0), arg(1), a.size() > 2 ? a[2] : "center");
			else if (c == "hide") { if (arg(0) == "all" || arg(0).empty()) charas_.clear(); else RemoveChara(arg(0)); }
			else if (c == "bgm") CmdBgm(arg(0));
			else if (c == "bgmstop") { StopBgm(); bgmName_.clear(); }
			else if (c == "se") CmdSe(arg(0));
			else if (c == "wait") { waitMs_ = std::atoi(arg(0).c_str()); state_ = State::Wait; }
			else if (c == "clear") { page_.clear(); chars_.clear(); }
			else if (c == "jump") Jump(arg(0));
			else if (c == "end") { state_ = State::Finished; }
			else if (c == "set") {
				const std::string name = arg(0), op = arg(1); const int v = Val(arg(2));
				const int cur = ReadVar(name);
				if (op == "=") WriteVar(name, v);
				else if (op == "+=") WriteVar(name, cur + v);
				else if (op == "-=") WriteVar(name, cur - v);
				else Warn("@set: unknown operator '" + op + "'");
			}
			// @on name / @off name : switch a global flag ON(1) / OFF(0)   ("name" or "$name" both work)
			else if (c == "on" || c == "off") {
				std::string name = arg(0);
				if (name.empty()) { Warn("@" + c + ": no flag name"); return; }
				if (name[0] != '$') name = "$" + name;
				WriteVar(name, c == "on" ? 1 : 0);
			}
			else if (c == "if") {
				if (IsBlockIf(a)) {
					if (!EvalCond(a)) {
						if (l.target < 0) { Warn("@if without @endif"); pc_ = (int)lines_.size(); }
						else pc_ = l.target;
					}
				}
				else if (EvalCond(a)) Jump(a.size() == 2 ? a[1] : arg(3));
			}
			else if (c == "else") {   // reached by running through the true branch: skip the false branch
				if (l.target >= 0) pc_ = l.target;
			}
			else if (c == "endif") {}
			else if (c == "choice") {
				choices_.clear();
				for (size_t i = 0; i + 1 < a.size(); i += 2) choices_.push_back({ a[i], a[i + 1] });
				if (choices_.empty()) { Warn("@choice: no options"); return; }
				choiceSel_ = 0; choiceLine_ = curIdx; state_ = State::Choice;
			}
			else {
				if (!(handler_ && handler_(c, a))) Warn("unknown command: @" + c);
			}
		}

		void Jump(const std::string& label) {
			auto it = labels_.find(label);
			if (it == labels_.end()) { Warn("label not found: " + label); return; }
			pc_ = it->second;
		}

		void UpdateChoice(const Input& in) {
			const int n = (int)choices_.size();
			if (in.up)   choiceSel_ = (choiceSel_ + n - 1) % n;
			if (in.down) choiceSel_ = (choiceSel_ + 1) % n;
			bool pick = in.ok;
			if (in.mouseX >= 0) {
				for (int i = 0; i < n; ++i) {
					int x, y, w, h; ChoiceRect(i, n, x, y, w, h);
					if (in.mouseX >= x && in.mouseX < x + w && in.mouseY >= y && in.mouseY < y + h) {
						choiceSel_ = i;
						if (in.click) pick = true;
					}
				}
			}
			if (pick) {
				backlog_.push_back({ "", "> " + choices_[choiceSel_].text });
				Jump(choices_[choiceSel_].label);
				state_ = State::Running; choices_.clear();
				RunUntilBlock();
			}
		}

		int ResumePc() const {
			switch (state_) {
			case State::Typing: case State::WaitClick: return pageLine_;
			case State::Choice: return choiceLine_;
			default: return pc_;
			}
		}

		static bool FileExists(const std::string& p) { std::ifstream f(p.c_str(), std::ios::binary); return (bool)f; }

		std::string Resolve(const std::string& dir, const std::string& name, const char* const* exts, size_t n) {
			const std::string base = dir.empty() ? name : dir + "/" + name;
			if (name.find('.') != std::string::npos && FileExists(base)) return base;
			for (size_t i = 0; i < n; ++i) if (FileExists(base + exts[i])) return base + exts[i];
			return "";
		}
		int GetGraph(const std::string& dir, const std::string& name) {
			static const char* const E[] = { ".png", ".jpg", ".jpeg", ".bmp", ".webp" };
			const std::string key = dir + "/" + name;
			auto it = gcache_.find(key); if (it != gcache_.end()) return it->second;
			std::string p = Resolve(dir, name, E, 5); int h = -1;
			if (p.empty()) Warn("image not found: " + key); else h = LoadGraph(p.c_str());
			return gcache_[key] = h;
		}
		int GetSound(const std::string& dir, const std::string& name) {
			static const char* const E[] = { ".ogg", ".mp3", ".wav", ".m4a" };
			const std::string key = dir + "/" + name;
			auto it = scache_.find(key); if (it != scache_.end()) return it->second;
			std::string p = Resolve(dir, name, E, 4); int h = -1;
			if (p.empty()) Warn("sound not found: " + key); else h = LoadSoundMem(p.c_str());
			return scache_[key] = h;
		}

		void CmdBg(const std::string& name) { bgName_ = name; bg_ = GetGraph(cfg.bgDir, name); }
		void CmdChara(const std::string& id, const std::string& file, const std::string& pos) {
			RemoveChara(id);
			charas_.push_back({ id, file, pos, GetGraph(cfg.charaDir, file) });
		}
		void RemoveChara(const std::string& id) {
			charas_.erase(std::remove_if(charas_.begin(), charas_.end(), [&](const Chara& c) { return c.id == id; }), charas_.end());
		}
		void CmdBgm(const std::string& name) {
			StopBgm(); bgmName_ = name;
			int h = GetSound(cfg.bgmDir, name);
			if (h >= 0) { ChangeVolumeSoundMem(255 * cfg.bgmVolume / 100, h); PlaySoundMem(h, DX_PLAYTYPE_LOOP); bgmHandle_ = h; }
		}
		void StopBgm() { if (bgmHandle_ >= 0) StopSoundMem(bgmHandle_); bgmHandle_ = -1; }
		void CmdSe(const std::string& name) {
			int h = GetSound(cfg.seDir, name);
			if (h >= 0) { ChangeVolumeSoundMem(255 * cfg.seVolume / 100, h); PlaySoundMem(h, DX_PLAYTYPE_BACK); }
		}

		void Warn(const std::string& m) { warnings_.push_back(m); if (onWarning) onWarning(m); }

		Input ReadInput() {
			Input in;
			const bool kOk = CheckHitKey(KEY_INPUT_RETURN) || CheckHitKey(KEY_INPUT_Z) || CheckHitKey(KEY_INPUT_SPACE);
			const int  mb = GetMouseInput();
			const bool mL = (mb & MOUSE_INPUT_LEFT) != 0, mR = (mb & MOUSE_INPUT_RIGHT) != 0;
			const bool kA = CheckHitKey(KEY_INPUT_A) != 0, kL = CheckHitKey(KEY_INPUT_L) != 0;
			const bool kU = CheckHitKey(KEY_INPUT_UP) != 0 || CheckHitKey(KEY_INPUT_W) != 0;
			const bool kD = CheckHitKey(KEY_INPUT_DOWN) != 0 || CheckHitKey(KEY_INPUT_S) != 0;
			const bool kB = CheckHitKey(KEY_INPUT_BACK) != 0;

			// First call: treat keys that are already held (e.g. the Enter that opened this scene)
			// as "already pressed", so they do not count as a new press.
			if (!inputPrimed_) {
				pOk_ = kOk; pL_ = mL; pR_ = mR; pA_ = kA; pLog_ = kL; pU_ = kU; pD_ = kD; pB_ = kB;
				inputPrimed_ = true;
			}

			in.ok = kOk && !pOk_;
			in.cancel = kB && !pB_;
			in.toggleAuto = kA && !pA_;  in.backlog = kL && !pLog_;
			in.up = kU && !pU_;  in.down = kD && !pD_;
			in.skip = CheckHitKey(KEY_INPUT_LCONTROL) || CheckHitKey(KEY_INPUT_RCONTROL);
			if (cfg.useMouse) {
				in.click = mL && !pL_;
				in.cancel = in.cancel || (mR && !pR_);
				in.wheel = GetMouseWheelRotVol();
				GetMousePoint(&in.mouseX, &in.mouseY);
			}
			pOk_ = kOk; pL_ = mL; pR_ = mR; pA_ = kA; pLog_ = kL; pU_ = kU; pD_ = kD; pB_ = kB;

			const int now = GetNowCount();
			in.dtMs = lastMs_ ? std::min(now - lastMs_, 100) : 16;
			lastMs_ = now;
			return in;
		}
		bool pOk_ = false, pL_ = false, pR_ = false, pA_ = false, pLog_ = false, pU_ = false, pD_ = false, pB_ = false;
		bool inputPrimed_ = false;
		int lastMs_ = 0;

		void OpenBacklog() { if (backlog_.empty()) return; backlogOpen_ = true; backlogScroll_ = 0; }
		bool UpdateBacklog(const Input& in) {
			if (!backlogOpen_) return false;
			const int maxScroll = std::max(0, (int)backlog_.size() - 1);
			if (in.up || in.wheel > 0)   backlogScroll_ = std::min(backlogScroll_ + 1, maxScroll);
			if (in.down || in.wheel < 0) backlogScroll_ = std::max(backlogScroll_ - 1, 0);
			if (in.cancel || in.backlog || in.click || in.ok) backlogOpen_ = false;
			return true;
		}

		int ScreenW() const { if (cfg.screenW > 0) return cfg.screenW; int w, h, c; GetDrawScreenSize(&w, &h); (void)c; return w; }
		int ScreenH() const { if (cfg.screenH > 0) return cfg.screenH; int w, h; GetDrawScreenSize(&w, &h); return h; }
		int FontPx() const { return cfg.fontHandle >= 0 ? GetFontSizeToHandle(cfg.fontHandle) : GetFontSize(); }
		int LineH() const { return FontPx() * 3 / 2; }
		static unsigned Col(int rgb) { return GetColor((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
		void Str(int x, int y, const std::string& s, int rgb) const {
			if (cfg.fontHandle >= 0) DrawStringToHandle(x, y, s.c_str(), Col(rgb), cfg.fontHandle);
			else DrawString(x, y, s.c_str(), Col(rgb));
		}
		int StrW(const std::string& s) const {
			return cfg.fontHandle >= 0 ? GetDrawStringWidthToHandle(s.c_str(), (int)s.size(), cfg.fontHandle)
				: GetDrawStringWidth(s.c_str(), (int)s.size());
		}
		int CharLen(const std::string& s, size_t i) const {
			const unsigned char c = (unsigned char)s[i]; size_t n = 1;
			if (cfg.utf8) n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
			else n = ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) ? 2 : 1;
			return (int)std::min(n, s.size() - i);
		}

		void BoxRect(int& x, int& y, int& w, int& h) const {
			const int H = ScreenH();
			h = cfg.boxHeight > 0 ? cfg.boxHeight : H * 28 / 100;
			x = cfg.boxMargin; w = ScreenW() - cfg.boxMargin * 2; y = H - h - cfg.boxMargin;
		}

		void Layout() {
			chars_.clear();
			int bx, by, bw, bh; BoxRect(bx, by, bw, bh);
			const int maxW = bw - cfg.boxPadding * 2, lh = LineH();
			int x = 0, y = 0;
			for (size_t i = 0; i < page_.size();) {
				const int n = CharLen(page_, i);
				std::string ch = page_.substr(i, n); i += n;
				if (ch == "\n") { x = 0; y += lh; continue; }
				const int w = StrW(ch);
				if (x + w > maxW) { x = 0; y += lh; }
				chars_.push_back({ ch, x, y });
				x += w;
			}
		}

		void DrawBlendBox(int x1, int y1, int x2, int y2) const {
			SetDrawBlendMode(DX_BLENDMODE_ALPHA, cfg.boxAlpha);
			DrawBox(x1, y1, x2, y2, Col(cfg.boxRGB), TRUE);
			SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
		}

		void DrawMessage() const {
			int bx, by, bw, bh; BoxRect(bx, by, bw, bh);
			DrawBlendBox(bx, by, bx + bw, by + bh);
			const int tx = bx + cfg.boxPadding, ty = by + cfg.boxPadding;
			if (!speaker_.empty()) {
				const int nh = LineH() + 6, nw = StrW(speaker_) + 24;
				DrawBlendBox(bx, by - nh - 4, bx + nw, by - 4);
				Str(bx + 12, by - nh - 4 + 3, speaker_, cfg.nameRGB);
			}
			const int n = std::min((int)shown_, (int)chars_.size());
			for (int i = 0; i < n; ++i) Str(tx + chars_[i].x, ty + chars_[i].y, chars_[i].ch, cfg.textRGB);

			if (state_ == State::WaitClick && ((idleMs_ / 400) % 2 == 0)) {
				const int ix = bx + bw - 24, iy = by + bh - 22;
				DrawTriangle(ix, iy, ix + 12, iy, ix + 6, iy + 8, Col(cfg.textRGB), TRUE);
			}
			if (auto_) Str(bx + bw - 60, by + 2, "AUTO", cfg.nameRGB);
		}

		void ChoiceRect(int i, int n, int& x, int& y, int& w, int& h) const {
			const int H = ScreenH(), W = ScreenW(), lh = LineH() + 16;
			w = W * 3 / 5; h = lh; x = (W - w) / 2;
			const int top = (H * 2 / 5) - (n * (lh + 8)) / 2;
			y = top + i * (lh + 8);
		}
		void DrawChoices() const {
			const int n = (int)choices_.size();
			for (int i = 0; i < n; ++i) {
				int x, y, w, h; ChoiceRect(i, n, x, y, w, h);
				DrawBlendBox(x, y, x + w, y + h);
				if (i == choiceSel_) DrawBox(x, y, x + w, y + h, Col(cfg.choiceSelRGB), FALSE);
				Str(x + (w - StrW(choices_[i].text)) / 2, y + 8, choices_[i].text, i == choiceSel_ ? cfg.choiceSelRGB : cfg.textRGB);
			}
		}

		void DrawChara(const Chara& c) const {
			if (c.handle < 0) return;
			int gw, gh; GetGraphSize(c.handle, &gw, &gh);
			const int W = ScreenW(), H = ScreenH();
			int cx = W / 2;
			if (c.pos == "left") cx = W * 22 / 100; else if (c.pos == "right") cx = W * 78 / 100;
			else if (c.pos != "center" && !c.pos.empty()) cx = std::atoi(c.pos.c_str());
			DrawGraph(cx - gw / 2, H - gh, c.handle, TRUE);
		}

		void DrawBacklog() const {
			const int W = ScreenW(), H = ScreenH(), lh = LineH();
			DrawBlendBox(0, 0, W, H);
			Str(8, 4, "BACKLOG", cfg.nameRGB);
			int y = H - lh * 2;
			for (int i = (int)backlog_.size() - 1 - backlogScroll_; i >= 0 && y > lh; --i) {
				const auto& e = backlog_[i];
				const int lines = 1 + (int)std::count(e.text.begin(), e.text.end(), '\n');
				y -= lh * lines;
				std::string t = (e.speaker.empty() ? "" : "[" + e.speaker + "] ") + ReplaceAll(e.text, "\n", " ");
				Str(12, y, t, cfg.textRGB);
				y -= 6;
			}
		}

		std::vector<Line> lines_;
		std::map<std::string, int> labels_, vars_;
		std::map<std::string, int>* globals_ = nullptr;   // game-wide flags ("$name"), not owned
		std::string scriptPath_;
		int pc_ = 0, pageLine_ = 0, choiceLine_ = 0;
		State state_ = State::Running;

		std::string speaker_, page_;
		struct Glyph { std::string ch; int x, y; };
		std::vector<Glyph> chars_;
		double shown_ = 0; int idleMs_ = 0, waitMs_ = 0;
		bool auto_ = false;

		std::vector<Choice> choices_; int choiceSel_ = 0;

		int bg_ = -1; std::string bgName_, bgmName_; int bgmHandle_ = -1;
		std::vector<Chara> charas_;
		std::unordered_map<std::string, int> gcache_, scache_;

		std::vector<LogEntry> backlog_; bool backlogOpen_ = false; int backlogScroll_ = 0;
		std::vector<std::string> warnings_;
		CommandHandler handler_;
	};

}  // namespace Novel