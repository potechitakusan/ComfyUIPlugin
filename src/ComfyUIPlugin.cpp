/**
 * @file ComfyUIPlugin.cpp
 * @author consomme hollywood
 * @brief クリスタ用の画像生成プラグイン：メインモジュール
 */
#include "pch.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib> // system関数のために必要
#include <sstream> // ファイル読み込みのために必要
#include <array>
#include <wchar.h> // ワイド文字用（日本語フォルダ用を想定したがなくても良いかも）
#include <chrono>  // sleep_for
#include <thread>  // sleep_for

#include "ComfyUIPlugin.h"
#include "FilterPlugIn.h"

using namespace ComfyUIPlugin;


typedef uint8_t byte_t;
typedef byte_t* pbyte_t;


/// このプラグインのモジュールID（GUID）
/// @note プラグイン毎に違う値にしないと駄目
constexpr auto kModuleIDString = "cebe5795-0c3a-4ea5-81e4-e33c1372cd44";

// ComfyUIのサーバーアドレス（デフォルト）
const std::string SERVER_ADDRESS_DEFAULT = "http://127.0.0.1:8188";

// 置換対象のマーカー 入力画像
const std::string MARKER_INPUT_IMAGE = "temp_img_req_yyyyMMddhhmmss.png"; 
constexpr size_t kSubImageDropdownCount = 3;
const std::array<std::string, kSubImageDropdownCount> kSubImageMarkers = {
	"temp_subimg_req_yyyyMMddhhmmss.png",
	"temp_subimg2_req_yyyyMMddhhmmss.png",
	"temp_subimg3_req_yyyyMMddhhmmss.png"
};
const std::array<std::string, kSubImageDropdownCount> kSubImageUploadPrefixes = {
	"temp_subimg_req_",
	"temp_subimg2_req_",
	"temp_subimg3_req_"
};

// 置換対象のマーカー プロンプト
const std::string MARKER_PROMPT = "###input1###"; 
const std::string MARKER_NPROMPT = "###input2###"; 

const std::string kNoImageDisplayName = "(no image)";

/// このDLLのベースパス
std::string g_BasePath;

/// デバッグログの書き出し先
std::string g_DebugPath;

/// server
std::string g_ServerAddress;

/// API Key
std::string g_APIKey;

/// Retry
int g_RetryMaxCount;
int g_RetryWaitSeconds;

/// temp_post.jsonの書き出し先
std::string g_TempPostJsonPath;

/// temp_xxx_res.jsonの書き出し先
std::string g_TempPromptResultJsonPath;
std::string g_TempHistoryResultJsonPath;

// パラメーター構造体
struct Params {
     std::string template_workflow_filename;
     std::array<std::string, kSubImageDropdownCount> input_subimage_filenames;
     std::string prompt;
     std::string negative_prompt;
     int sample_steps;
};

/// フィルター情報
struct FilterInfo {
	FilterPlugIn::Server const* server;
	Params params;
	int setting;
	std::array<int, kSubImageDropdownCount> subimage_indices;
};


class ImageBuffer {
private:
	int width_ = 0;
	int height_ = 0;
	
	// R, G, B の順で全てのピクセルデータを連続して保持
	std::unique_ptr<unsigned char[]> data_buffer_;
	const int CHANNELS = 3; // R, G, B

public:
	ImageBuffer() = default;

	FilterPlugIn::Rect rect;

	void allocate(int w, int h) {
		width_ = w;
		height_ = h;
		size_t total_bytes = static_cast<size_t>(width_) * height_ * CHANNELS;

		// 全ピクセル分のバイトを一度に割り当て
		data_buffer_ = std::make_unique<unsigned char[]>(total_bytes);
	}

	int get_width()  const { return width_; }
	int get_height() const { return height_; }
	
	/**
	 * @brief 指定された座標 (x, y) のピクセル値を取得します。
	 * @param x 列インデックス (0 から width-1)
	 * @param y 行インデックス (0 から height-1)
	 * @param channel_offset 取得したいチャネル (R=0, G=1, B=2)
	 * @return unsigned char ピクセル値
	 */
	unsigned char get_pixel_value(int x, int y, int channel_offset) const {
		if (x < 0 || x >= width_ || y < 0 || y >= height_ || channel_offset < 0 || channel_offset >= CHANNELS) {
			// 範囲外のアクセスはエラーまたは0を返す
			return 0; 
		}

		// 1次元配列内のインデックスを計算
		// (y * 幅 + x) * チャンネル数 + チャンネルオフセット
		size_t index = (static_cast<size_t>(y) * width_ + x) * CHANNELS + channel_offset;
		
		return data_buffer_[index];
	}
	
	void set_pixel_value(int x, int y, int channel_offset, unsigned char value) const {
		if (x < 0 || x >= width_ || y < 0 || y >= height_ || channel_offset < 0 || channel_offset >= CHANNELS) {
			// 範囲外のアクセスはエラーまたは0を返す
			return; 
		}

		// 1次元配列内のインデックスを計算
		// (y * 幅 + x) * チャンネル数 + チャンネルオフセット
		size_t index = (static_cast<size_t>(y) * width_ + x) * CHANNELS + channel_offset;
		
		data_buffer_[index] = value;
	}
	
	// データポインタを取得する（データ格納関数内で使用）
	unsigned char* get_data_pointer() {
		return data_buffer_.get();
	}
};


void Transfer(const FilterPlugIn::Block& dst, const class ImageBuffer& src, const FilterPlugIn::Block& alpha);
void Transfer(const ImageBuffer& dst, const FilterPlugIn::Block& src, int offsetY, int offsetX);
void Transfer(const FilterPlugIn::Block& dst, const class ImageBuffer& src, const FilterPlugIn::Block& alpha, const FilterPlugIn::Block& select);


// パラメーター実体
Params g_params;

/// 設定リスト（サブウィンドウの先頭のドロップダウン）
std::vector<std::string> g_Settings;

/// サブイメージリスト（サブウィンドウの次のドロップダウン）
std::vector<std::string> g_SubImages;

/// デバッグ出力の開始
void InitDebugOutput(const std::string& basePath) {
	g_DebugPath = basePath + "debuglog.txt";
	FILE* fp = nullptr;
	fopen_s(&fp, g_DebugPath.c_str(), "w");
	if (fp) fclose(fp);

	// Python用のdebuglogは .bat や .py から出力する。ここはファイルのクリアのみ。
	FILE* fp2 = nullptr;
	fopen_s(&fp2, (basePath + "debuglog_py.txt").c_str(), "w");
	if (fp2) fclose(fp2);
}

/// デバッグ出力
/// @note ホストアプリがデバッガを嫌うから原始的なファイル出力で
void print(const char* format, ...) {
	if (g_DebugPath.empty()) return;
	FILE* fp = nullptr;
	fopen_s(&fp, g_DebugPath.c_str(), "a");
	if (fp) {
		va_list arg;
		va_start(arg, format);
		vfprintf(fp, format, arg);
		va_end(arg);
		fputs("\n", fp);
		fclose(fp);
	}
}

/// デバッグ出力（wstring用）
void print(const wchar_t* format, ...) {
	if (g_DebugPath.empty()) return;
	FILE* fp = nullptr;
	fopen_s(&fp, g_DebugPath.c_str(), "a");
	if (fp) {
		va_list arg;
		va_start(arg, format);
		vfwprintf(fp, format, arg);
		va_end(arg);
		fputs("\n", fp);
		fclose(fp);
	}
}

// curlでPOSTするためにjsonをファイルに書き出し
void write_json_to_temp(const char* jsonstr, std::string tempPostJsonPath, ...) {
	FILE* fp = nullptr;
	std::remove(tempPostJsonPath.c_str());
	fopen_s(&fp, tempPostJsonPath.c_str(), "w");
	if (fp) {
		va_list arg;
		va_start(arg, jsonstr);
		vfprintf(fp, jsonstr, arg);
		va_end(arg);
		fputs("\n", fp);
		fclose(fp);
	}
}


/// @brief 文字列のUNICODE化
/// @param str ShiftJIS文字列
/// @return UNICODE文字列
std::wstring ShiftJIS_to_UTF16(const std::string& str)
{
    static_assert(sizeof(wchar_t) == 2, "for windows only");
    const int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    std::vector<wchar_t> result(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &result[0], len);
    return &result[0];
}

/// @brief 文字列のUNICODE化
/// @param str ShiftJIS文字列
/// @return UNICODE文字列
std::string UTF16_to_UTF8(const std::wstring& utf16str)
{
    // 変換後のchar配列の要素数(null終端を含む)を取得
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16str.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string utf8str(size - 1, '\0');

    // 変換
    WideCharToMultiByte(CP_UTF8, 0, utf16str.c_str(), -1, &utf8str[0], size - 1, nullptr, nullptr);

    return utf8str;
}

/// @brief コマンドプロンプトを表示せずに実行
/// @param command 
/// @return 
int exe_command_silent(std::string command) {
    // プロセス起動情報
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    // コンソールウィンドウを表示しない設定
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // 非表示

    ZeroMemory(&pi, sizeof(pi));

    // cmd.exe /C を使ってコマンドを一回実行
    std::wstring cmdLine = L"cmd.exe /C " + ShiftJIS_to_UTF16(command);

    BOOL result = CreateProcessW(
        nullptr,
        &cmdLine[0],
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, // ★ここがポイント
        nullptr,
        ShiftJIS_to_UTF16(g_BasePath).c_str(),
        &si,
        &pi
    );

    if (!result) {
        std::wcerr << L"プロセス作成失敗: " << GetLastError() << std::endl;
        return 1;
    }

    // コマンド完了を待機
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 終了コード確認
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    std::wcout << L"完了 (ExitCode=" << exitCode << L")" << std::endl;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
/**
 * @brief HTTP GETリクエストを実行し、レスポンスをファイルに保存
 * @param url リクエストURL
 * @param output_filename レスポンスを書き込むファイル名
 * @return true 成功, false 失敗
 */
bool http_get_to_file(const std::string& url, const std::string& output_filename) {
    std::string command = "curl -s -o " + output_filename + " \"" + url + "\"";
    print(("GET Command: " + command).c_str());
    int result = exe_command_silent(command);
    print(("curl GET command returns :" + std::to_string(result)).c_str());
    return true;
}


/**
 * @brief HTTP POSTリクエストを実行し、レスポンスをファイルに保存
 * * @param url リクエストURL
 * @param data_filename POSTするデータファイル名
 * @param output_filename レスポンスを書き込むファイル名
 * @return true 成功, false 失敗
 */
bool http_post_file_to_file(const std::string& url, const std::string& data_filename, const std::string& output_filename) {
    // Windowsで実行する前提として、curlを利用し、JSONをPOST
    std::string command = "curl -s -X POST -H \"Content-Type: application/json\" -d @" + data_filename + " -o " + output_filename + " \"" + url + "\"";
    print(("POST Command: " + command).c_str());
	int result = std::system(command.c_str());
    print(("curl POST command returns :" + std::to_string(result)).c_str());
    return true;
}

/**
 * @brief HTTP POSTリクエストを実行し、レスポンスをファイルに保存
 * * @param url リクエストURL
 * @param image_filename POSTする画像ファイル名
 * @param output_filename レスポンスを書き込むファイル名
 * @return true 成功, false 失敗
 */
bool http_post_image_to_file(const std::string& url, const std::string& image_filepath, const std::string& image_filename, const std::string& output_filename) {
    // Windowsで実行する前提として、curlを利用し、JSONをPOST
    std::string command = "curl -s -X POST -F \"image=@" + image_filepath + ";filename=" + image_filename + "\" -o " + output_filename + " \"" + url + "\"";
    print(("POST Command: " + command).c_str());
	int result = std::system(command.c_str());
    print(("curl POST command returns :" + std::to_string(result)).c_str());
    return true;
}

bool call_png_to_bmp() {
    std::string command = g_BasePath + "png_to_bmp.bat";
    if (std::system(command.c_str()) != 0) {
        print("Error: png_to_bmp command failed.");
        return false;
    }
    return true;
}

bool call_bmp_to_png(std::string imagePath) {
    std::string command = g_BasePath + "bmp_to_png.bat " + imagePath;
    if (std::system(command.c_str()) != 0) {
        print("Error: bmp_to_png command failed.");
        return false;
    }
    return true;
}

/**
 * @brief ワークフローをComfyUIに投げて実行する関数 (run_workflowの代替)
 * * @param workflow_json_path 変更後のワークフローJSONファイルパス
 * @param client_id クライアントID
 * @return std::string prompt_id, 失敗時は空文字列
 */
std::string run_workflow(const std::string& workflow_json_path, const std::string& client_id) {
    std::string temp_res_file = g_TempPromptResultJsonPath;
    std::string url = g_ServerAddress + "/prompt";
    
    // POSTリクエスト実行
    if (!http_post_file_to_file(url, workflow_json_path, temp_res_file)) {
        std::remove(temp_res_file.c_str());
        return "";
    }

    std::ifstream ifs(temp_res_file);
    if (!ifs.is_open()) {
        print(("Error: Could not open response file: " + temp_res_file).c_str() );
        return "";
    }
    
    // JSONレスポンスからprompt_idを抽出 (簡易的な文字列検索)
    // C++標準機能のみの制約により、簡易的な実装になります。
    std::string line;
    std::string prompt_id = "";
    while (std::getline(ifs, line)) {
        size_t pos = line.find("\"prompt_id\"");
        if (pos != std::string::npos) {
            size_t start = line.find(':', pos) + 1;
            size_t end = line.find(',', start);
            if (end == std::string::npos) end = line.find('}', start); // 最後の要素の可能性
            
            if (start != std::string::npos && end != std::string::npos && start < end) {
                // "xxxx" を取り出すために、startとendを調整
                start = line.find('"', start) + 1;
                end = line.find('"', start);

                if (start != std::string::npos && end != std::string::npos && start < end) {
                    prompt_id = line.substr(start, end - start);
                    break;
                }
            }
        }
    }

    ifs.close();
    // std::remove(temp_res_file.c_str()); // 一時ファイルはデバッグ用に残す

    return prompt_id;
}

/**
 * @brief 実行履歴を取得する関数 (get_historyの代替)
 * @param prompt_id 
 * @return std::string history_json_content, 失敗時は空文字列
 */
std::string get_history(const std::string& prompt_id) {
    std::string temp_res_file = g_TempHistoryResultJsonPath;
    std::string url = g_ServerAddress + "/history/" + prompt_id;
    
    if (!http_get_to_file(url, temp_res_file)) {
        std::remove(temp_res_file.c_str());
        return "";
    }

    std::ifstream ifs(temp_res_file);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    // std::remove(temp_res_file.c_str());

	print("content");
	print(content.c_str());

    return content; // JSON全体を文字列として返す
}

/**
 * @brief 画像データを取得する関数 (get_imageの代替)
 * * @param filename ファイル名
 * @param type タイプ (image, outputなど)
 * @param subfolder サブフォルダ
 * @return std::string 一時ファイル名, 失敗時は空文字列
 */
std::string get_image(const std::string& filename, const std::string& type, const std::string& subfolder) {
    std::string temp_img_file = g_BasePath + "temp_img_res.png";
    
    std::string url = g_ServerAddress + "/view?filename=" + filename + "&type=" + type + "&subfolder=" + subfolder;
    
    if (!http_get_to_file(url, temp_img_file)) {
        std::remove(temp_img_file.c_str());
        return "";
    }

    // 画像データは一時ファイルに直接保存される
    return temp_img_file;
}

/**
 * ファイル全体を文字列として読み込む
 * @param path 読み込むファイルのパス
 * @return ファイルの内容を示す文字列
 */
std::string read_file_to_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        print(("Error: Could not open file: " + path).c_str());
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
	file.close();
    return buffer.str();
}
std::wstring read_file_to_wstring(const std::string& path) {
    std::wfstream file(path);
    if (!file.is_open()) {
        return L"";
    }
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string ansi_to_utf8(const std::string& ansi_str) {
    // ANSIからUTF-16 (WideChar)に変換
    int wide_len = MultiByteToWideChar(CP_ACP, 0, ansi_str.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wide_chars(wide_len);
    MultiByteToWideChar(CP_ACP, 0, ansi_str.c_str(), -1, wide_chars.data(), wide_len);

    // UTF-16からUTF-8に変換
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_chars.data(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> utf8_chars(utf8_len);
    WideCharToMultiByte(CP_UTF8, 0, wide_chars.data(), -1, utf8_chars.data(), utf8_len, nullptr, nullptr);

    return std::string(utf8_chars.data());
}

/**
 * 文字列内の特定のマーカーを置換する
 * @param str 元の文字列
 * @param from 置換前のマーカー
 * @param to 置換後のテキスト
 * @return 置換後の文字列
 */
std::string replace_all(std::string str, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return str;
	}
	const std::string utf8_string = ansi_to_utf8(to);
	const size_t replacementLength = utf8_string.length();
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), utf8_string);
		if (replacementLength > 0) {
			start_pos += replacementLength;
		} else {
			start_pos += from.length();
		}
    }
    return str;
}

std::wstring replace_all(std::wstring str, const std::wstring& from, const std::wstring& to) {
	if (from.empty()) {
		return str;
	}
	const size_t replacementLength = to.length();
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
		if (replacementLength > 0) {
			start_pos += replacementLength;
		} else {
			start_pos += from.length();
		}
    }
    return str;
}

/**
 * ワークフローをComfyUIサーバーのキューに送信する
 * @param prompt_json ワークフローのJSON文字列
 */
std::string queue_prompt(const std::string prompt_json) {

	std::string prompt_json_to = prompt_json;

	// print(prompt_json_to.c_str());
    std::string payload_data = "";
	payload_data += "{ \"prompt\": ";
	payload_data += prompt_json_to;
	if (!g_APIKey.empty()) {
		payload_data += " , \"extra_data\": { ";
		payload_data += " \"api_key_comfy_org\": ";
		payload_data += " \"" + g_APIKey + "\" ";
		payload_data += " } ";
	}
	payload_data += " } ";

	// print(payload_data.c_str());

    print(("Write to json:" + g_TempPostJsonPath).c_str());
	// ペイロードをファイル出力し、-dオプションで渡す
	write_json_to_temp(payload_data.c_str(), g_TempPostJsonPath);
    print("Write Finished");

    print("Sending prompt to ComfyUI...");
	std::string client_id = "";

	std::string prompt_id = run_workflow(g_TempPostJsonPath, client_id);

    std::string history_content;
	for (int i = 0; i < g_RetryMaxCount; i++) {
		history_content = get_history(prompt_id);
        size_t error_pos = history_content.find("execution_error");
		if (error_pos != std::string::npos) {
			print("");
			print("history has returned execution error");
			print("");
			size_t message_pos = history_content.find("exception_message");
			if (message_pos != std::string::npos) {
                std::string message = history_content.substr(message_pos);
				message = replace_all(message, "\\\\n", "###back_to_n###");
				message = replace_all(message, "\\n", "\n");
				message = replace_all(message, "###back_to_n###", "\\\\n");
				message = replace_all(message, "\", \"", "");
				
				print(message.c_str());
				print("");

			}			
			return "";
		}
        size_t image_pos = history_content.find("CCPImage_");
		if (image_pos == std::string::npos) {
            std::this_thread::sleep_for(std::chrono::seconds(g_RetryWaitSeconds)); 
			continue;
		}
		break;
	}

	std::string filename;
	std::string type = "output";
	std::string subfolder = "CLIPSTUDIO_ComfyUI_PLUGIN";
    size_t image_pos = history_content.find("CCPImage_");
    if (image_pos != std::string::npos) {
		// "filename" 抽出
		size_t fn_start = image_pos;
		size_t fn_end = history_content.find(".png", fn_start) + 4;
		print(std::to_string(fn_start).c_str());
		print(std::to_string(fn_end).c_str());
		if (fn_start < fn_end) filename = history_content.substr(fn_start, fn_end - fn_start);

		// "type" 抽出
		size_t t_pos = history_content.find("\"type\"", image_pos);
		if (t_pos != std::string::npos) {
			size_t t_start = history_content.find('\"', t_pos + 6) + 1;
			size_t t_end = history_content.find('\"', t_start);
			if (t_start < t_end) type = history_content.substr(t_start, t_end - t_start);
		}

		// "subfolder" 抽出
		size_t sf_pos = history_content.find("\"subfolder\"", image_pos);
		if (sf_pos != std::string::npos) {
			size_t sf_start = history_content.find('\"', sf_pos + 11) + 1;
			size_t sf_end = history_content.find('\"', sf_start);
			if (sf_start < sf_end) subfolder = history_content.substr(sf_start, sf_end - sf_start);
		}
	}

	print(filename.c_str());

    std::string temp_image_path = get_image(filename, type, subfolder);
    if (temp_image_path.empty()) {
        print("Error: Failed to retrieve image data.");
    }

	print(temp_image_path.c_str());

	return temp_image_path;

}

/// @brief ベースパス取得
/// @param hModule モジュールハンドル
/// @return このモジュールのベースパス
/// @note ロングパス対策なしの簡易版だけど用途的には大丈夫な筈
std::string GetBasePath(HMODULE hModule) {
	std::vector<char> buf(MAX_PATH);
	GetModuleFileNameA(hModule, &buf[0], MAX_PATH);
	std::filesystem::path path(&buf[0]);
	return path.parent_path().string() + "\\";
}

/// @brief DLLのエントリーポイント
/// @param hModule DLL（自分）のモジュールハンドル
/// @param fdwReason 呼ばれた理由（プロセスorスレッドのアタッチorデタッチ）
/// @param lpReserved 予約済み
/// @return 基本的にTRUE（ロード相手にNULL返すならFALSE）
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  fdwReason, LPVOID lpReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		// ベースパスの取得＆デバッグ出力の開始
		g_BasePath = GetBasePath(hModule);
		InitDebugOutput(g_BasePath);
		g_TempPostJsonPath = g_BasePath + "temp_post.json"; 
		g_TempPromptResultJsonPath = g_BasePath + "temp_prompt_res.json"; 
		g_TempHistoryResultJsonPath = g_BasePath + "temp_history_res.json"; 
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

/// プロパティキー
enum PropertyKey {
	ITEM_SETTING = 1,
	ITEM_SUBIMAGE,
	ITEM_SUBIMAGE_PICTURE3,
	ITEM_SUBIMAGE_PICTURE4,
	ITEM_PROMPT,
	ITEM_NPROMPT,
};

/// プラグイン初期化
/// @return 正常終了ならtrue
/// @note ここでfalse返すとクリスタのバージョン上げろって言われる
static bool InitializeModule(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data) {
	// 初期化
	FilterPlugIn::ModuleInitialize initialize(server);
	if (!initialize.Initialize(kModuleIDString)) return false;

	// 情報インスタンス
	auto info = new FilterInfo;
	info->server = server;
	info->subimage_indices.fill(0);
	*data = info;

	return true;
}

/// プラグイン終了
/// @return 正常終了ならtrue
static bool TerminateModule(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data) {
	// 情報インスタンス解放
	if (*data) {
		delete static_cast<FilterInfo*>(*data);
		*data = nullptr;
	}
	// StableDiffusionのDLL解放
	// StableDiffusion::Terminate();
	return true;
}


/// @brief 設定ファイルのパス
/// @return iniファイルのフルパスを返す
static std::string GetIniPath()
{
	return g_BasePath + "ComfyUIPlugin.ini";
}

/// @brief 設定ファイルのセクション
/// @return COMMON以外のセクションを返す
static std::vector<std::string> GetIniSections(const std::string& iniPath)
{
		char sections[4096];
		GetPrivateProfileSectionNamesA(sections, sizeof(sections), iniPath.c_str());
		char *section = sections;
		std::vector<std::string> result;
		while (*section != NULL)
		{
			std::string name = section;
			if (name != "COMMON") result.push_back(name);
			section += strlen(section) + 1;
		}
		return result;
}

// SubImageフォルダ内の.pngファイルのリストを返却する。
static std::vector<std::string> GetSubImages()
{
    std::vector<std::string> imageFiles;

    try {
		std::string folderPath = g_BasePath + "SubImage";
        if (!std::filesystem::exists(folderPath)) {
            print(("フォルダが存在しません: SubImage at " + g_BasePath).c_str());
            return imageFiles;
        }

        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (!entry.is_regular_file()) continue; // ファイルでないものはスキップ

            auto ext = entry.path().extension().string();
            // 小文字に変換して比較
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".png") {
                imageFiles.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        print("ファイルシステムエラー ");
    }

    return imageFiles;
}

static int GetNoImageSelectionIndex() {
	return static_cast<int>(g_SubImages.size());
}

static bool IsNoImageSelection(int selection) {
	return selection < 0 || selection >= static_cast<int>(g_SubImages.size());
}

static std::string ResolveSubImageFilename(int selection) {
	if (selection >= 0 && selection < static_cast<int>(g_SubImages.size())) {
		return g_SubImages[selection];
	}
	return "";
}


/// プロパティの初期化
static void InitProperty(FilterPlugIn::Property& p) {
	auto setting = p.addEnumerationItem(ITEM_SETTING, "Setting");
	for(int i = 0; i < g_Settings.size(); ++i) {
		setting.addValue(i, ShiftJIS_to_UTF16(g_Settings[i])); // UI側はUNICODEが良い
	}

	const int noImageIndex = static_cast<int>(g_SubImages.size());
	auto addSubImageValues = [&](const FilterPlugIn::Property::EnumerationItem& enumeration) {
		for(int i = 0; i < g_SubImages.size(); ++i) {
			enumeration.addValue(i, ShiftJIS_to_UTF16(g_SubImages[i])); // UI側はUNICODEが良い
		}
		enumeration.addValue(noImageIndex, ShiftJIS_to_UTF16(kNoImageDisplayName));
	};

	auto subimage = p.addEnumerationItem(ITEM_SUBIMAGE, "SubImage(Picture 2)");
	addSubImageValues(subimage);

	auto subimage3 = p.addEnumerationItem(ITEM_SUBIMAGE_PICTURE3, "SubImage(Picture 3)");
	addSubImageValues(subimage3);

	auto subimage4 = p.addEnumerationItem(ITEM_SUBIMAGE_PICTURE4, "SubImage(Picture 4)");
	addSubImageValues(subimage4);
	// p.addIntegerItem(ITEM_STEPS, "Steps", 20, 1, 60);
	// p.addDecimalItem(ITEM_STRENGTH, "Strength", 0.5, 0.0, 1.0);
	// p.addDecimalItem(ITEM_CONTROL_STRENGTH, "Control Strength", 8.0, 1.0, 20.0);

	p.addStringItem(ITEM_PROMPT, "Prompt", 800);
	p.addStringItem(ITEM_NPROMPT, "Negative Prompt", 800);
}

// iniファイルから文字列読み込み
std::string iniGetString(const std::string& filePath, const std::string& section, const std::string& key){
	char buf[MAX_PATH] = {};
	GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buf, MAX_PATH, filePath.c_str());
	auto text = std::string(buf);

	// コメント削除
	auto commentPos = text.find_first_of("#;");
	if (commentPos != std::string::npos) {
		text.erase(commentPos);
	}

	return text;
}

// iniファイル読み込み：文字列
void ini(const std::string& filePath, const std::string& section, const std::string& key, std::string& val){
	auto s = iniGetString(filePath, section, key);
	if (!s.empty()) val = s;
}

/// @brief 設定の切り替え
/// @param index スイッチ先の設定インデックス
/// @param data フィルター情報
/// @param propertyObject 反映先プロパティ
static void SwitchToSetting(int index, FilterPlugIn::Property& property, bool isDefault) {
	if (index < 0 || g_Settings.size() <= index) return;
	const auto setting = g_Settings[index];

	// コンフィグのロード
	auto iniPath = GetIniPath();
	
    ini(iniPath, setting, "template_workflow_filename", g_params.template_workflow_filename);
    ini(iniPath, setting, "prompt", g_params.prompt);
    ini(iniPath, setting, "negative_prompt", g_params.negative_prompt);

	print("SwitchToSetting:");
	print(setting.c_str());
	print(g_params.template_workflow_filename.c_str());
	print(g_params.prompt.c_str());
	print(g_params.negative_prompt.c_str());

	// プロパティへの反映
	property.setEnumeration(ITEM_SETTING, index);
	property.setStringDefault(ITEM_PROMPT, ShiftJIS_to_UTF16(g_params.prompt));
	property.setStringDefault(ITEM_NPROMPT, ShiftJIS_to_UTF16(g_params.negative_prompt));
    property.setString(ITEM_PROMPT, ShiftJIS_to_UTF16(g_params.prompt));
	property.setString(ITEM_NPROMPT, ShiftJIS_to_UTF16(g_params.negative_prompt));
}

/// プロパティ同期
static bool SyncProperty(FilterPlugIn::Int itemKey, FilterPlugIn::PropertyObject propertyObject, FilterPlugIn::Ptr data) {
	auto& info = *static_cast<FilterInfo*>(data);
	// auto& params = info.params;
	FilterPlugIn::Property property(info.server, propertyObject);

	print("setting itemKey:");
	print(std::to_string(itemKey).c_str());
	
	switch (itemKey) {
	case ITEM_SETTING:
	{
		// 設定変更を検出してコンフィグを切り替える
		auto setting = property.getEnumeration(ITEM_SETTING);
		print("setting:");
		print(std::to_string(setting).c_str());
		if (info.setting != setting) {
			SwitchToSetting(setting, property, false);
			info.setting = setting;
			return true;
		}
	}
	break;
	case ITEM_SUBIMAGE:
	{
		auto subimage = property.getEnumeration(ITEM_SUBIMAGE);
		if (info.subimage_indices[0] != subimage) {
			info.subimage_indices[0] = subimage;
			return true;
		}
		break;
	}
	case ITEM_SUBIMAGE_PICTURE3:
	{
		auto subimage = property.getEnumeration(ITEM_SUBIMAGE_PICTURE3);
		if (info.subimage_indices[1] != subimage) {
			info.subimage_indices[1] = subimage;
			return true;
		}
		break;
	}
	case ITEM_SUBIMAGE_PICTURE4:
	{
		auto subimage = property.getEnumeration(ITEM_SUBIMAGE_PICTURE4);
		if (info.subimage_indices[2] != subimage) {
			info.subimage_indices[2] = subimage;
			return true;
		}
		break;
	}
	case ITEM_PROMPT:
		return property.sync(ITEM_PROMPT, g_params.prompt);
	case ITEM_NPROMPT:
	 	return property.sync(ITEM_NPROMPT, g_params.negative_prompt);
	}
	return false;
}

/// プロパティコールバック
static void FilterPropertyCallBack(FilterPlugIn::PropertyCallBackResult* result, FilterPlugIn::PropertyObject propertyObject, const FilterPlugIn::Int itemKey, const FilterPlugIn::PropertyCallBackNotify notify, FilterPlugIn::Ptr data) {
	(*result) = FilterPlugIn::PropertyCallBackResult::NoModify;
	if (notify != FilterPlugIn::PropertyCallBackNotify::ValueChanged) return;
	if (SyncProperty(itemKey, propertyObject, data)) {
		(*result) = FilterPlugIn::PropertyCallBackResult::Modify;
	}
}


/// フィルタ初期化
/// @return 正常終了ならtrue
static bool InitializeFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data) {
	FilterPlugIn::Initialize initialize(server);
	auto info = static_cast<FilterInfo*>(*data);
	info->server = server;

	// 初期設定の読み込み
	std::string iniPath = GetIniPath();
	ini(iniPath, "COMMON", "server_address", g_ServerAddress);
	if (g_ServerAddress.empty()) {
		g_ServerAddress = SERVER_ADDRESS_DEFAULT;
	}
	ini(iniPath, "COMMON", "api_key", g_APIKey);
	std::string retryMaxCount;
	ini(iniPath, "COMMON", "getimage_retry_max_count", retryMaxCount);
	std::string retryWaitSeconds;
	ini(iniPath, "COMMON", "getimage_retry_wait_seconds", retryWaitSeconds);

	g_RetryMaxCount = std::stoi(retryMaxCount);
	g_RetryWaitSeconds = std::stoi(retryWaitSeconds);

	// 設定リストの初期化
	g_Settings = GetIniSections(iniPath);

	// SubImageの初期化
	g_SubImages = GetSubImages();
	const int noImageIndex = static_cast<int>(g_SubImages.size());

	info->subimage_indices[0] = g_SubImages.empty() ? noImageIndex : 0;
	for (size_t i = 1; i < kSubImageDropdownCount; ++i) {
		info->subimage_indices[i] = noImageIndex;
	}

	// フィルタカテゴリ名とフィルタ名の設定
	initialize.SetCategoryName("ComfyUI API", 'x');
	initialize.SetFilterName("Generate", 'x');

	// プレビュー無し（LCMとかで高速化出来たらONにしてもいいか？）
	initialize.SetCanPreview(false);

	// ブランク画像はNG
	initialize.SetUseBlankImage(false);

	// ターゲット
	initialize.SetTargetKinds({ FilterPlugIn::Initialize::Target::RGBAlpha });

	// プロパティの作成
	auto property = FilterPlugIn::Property(server);
	InitProperty(property);
	property.setEnumeration(ITEM_SUBIMAGE, info->subimage_indices[0]);
	property.setEnumeration(ITEM_SUBIMAGE_PICTURE3, info->subimage_indices[1]);
	property.setEnumeration(ITEM_SUBIMAGE_PICTURE4, info->subimage_indices[2]);
	initialize.SetProperty(property);

	// 初回は0番設定に
	SwitchToSetting(0, property, true);
	info->setting = 0;

	//	プロパティコールバック
	initialize.SetPropertyCallBack(FilterPropertyCallBack, *data);

	return true;
}

/// フィルタ終了
/// @return 正常終了ならtrue
static bool TerminateFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data) {
	// 特に解放するリソースは無い
	return true;
}

// 画像ファイルの中身確認用（通常使わない）
std::string convert_address_to_hex_string(const void* address) {
    if (address == nullptr) {
        return "";
    }

    // void* を const char* にキャストして、バイト単位でアクセスできるようにする
    const char* char_ptr = static_cast<const char*>(address);
    std::stringstream ss;

    // ヌル終端文字 ('\0') に到達するまでループ
    for (const char* p = char_ptr; *p != '\0'; ++p) {
        // 各バイトを2桁の16進数としてフォーマット

        // 1. バイト値の取得: char* から dereference で char を取得
        // 2. 符号なしキャスト: 符号拡張を防ぐため unsigned char にキャスト
        // 3. 整数への昇格: ストリームで値として扱うため int にキャスト
        ss << std::hex << std::setw(2) << std::setfill('0') << std::uppercase
           << static_cast<int>(static_cast<unsigned char>(*p));
    }

    return ss.str();
}

// Windows BMPファイルヘッダの構造体
// 実際のファイルからの読み込み順序はリトルエンディアンに依存しますが、
// ここではバイト単位で読み込むため、簡略化しています。
#pragma pack(push, 1)
// 1. BMPファイルヘッダー (14 バイト)
struct BMPFileHeader {
    unsigned short signature = 0x4D42; // "BM"
    unsigned int file_size = 0;        // ファイル全体のサイズ
    unsigned short reserved1 = 0;      // 予約領域
    unsigned short reserved2 = 0;      // 予約領域
    unsigned int data_offset = 54;     // ピクセルデータまでのオフセット (54バイト)
};

// 2. DIBヘッダー (BITMAPINFOHEADER - 40 バイト)
struct BMPInfoHeader {
    unsigned int header_size = 40;     // ヘッダーサイズ (40)
    int width = 0;                     // 画像の幅 (ピクセル)
    int height = 0;                    // 画像の高さ (ピクセル)
    unsigned short planes = 1;         // プレーン数 (常に1)
    unsigned short bits_per_pixel = 24; // 1ピクセルあたりのビット数 (24)
    unsigned int compression = 0;      // 圧縮形式 (0: 無圧縮)
    unsigned int image_size = 0;       // ピクセルデータのサイズ
    int x_pixels_per_meter = 2835;     // 水平解像度 (適当な値 72dpi)
    int y_pixels_per_meter = 2835;     // 垂直解像度 (適当な値 72dpi)
    unsigned int colors_used = 0;      // 使用されている色数 (パレット未使用のため0)
    unsigned int colors_important = 0; // 重要な色数 (0: 全て重要)
};

#pragma pack(pop)

// --- 2. ピクセルデータ保持用構造体/クラス ---

bool load_bmp_rgb_to_buffer(const std::string& filename, ImageBuffer& img_data) {
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        print(("エラー: ファイルを開けませんでした: " + filename).c_str());
        return false;
    }

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    if (!file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header))) return false;
    if (!file.read(reinterpret_cast<char*>(&info_header), sizeof(info_header))) return false;

    if (file_header.signature != 0x4D42 || info_header.bits_per_pixel != 24 || info_header.compression != 0) {
        print("エラー: 24ビット非圧縮BMPのみをサポートしています。");
        return false;
    }

    long width = info_header.width;
    long height = std::abs(info_header.height);

    img_data.allocate(width, height);
    
    const long BYTES_PER_PIXEL = 3;
    long row_size = width * BYTES_PER_PIXEL;
    long padding = (4 - (row_size % 4)) % 4;

    file.seekg(file_header.data_offset, std::ios::beg);

    std::vector<unsigned char> row_data(row_size);
    unsigned char* dest_ptr = img_data.get_data_pointer();
    
    // BMPはボトムアップ形式 (下から上)
    // ImageBufferはトップダウン形式 (左上から順)で格納するため、行の順序を反転
    for (long y = height - 1; y >= 0; --y) {
        // 1. 1行分のピクセルデータを読み込み (BGR順)
        if (!file.read(reinterpret_cast<char*>(row_data.data()), row_size)) return false;
        
        // 2. パディングをスキップ
        file.seekg(padding, std::ios::cur);
        
        // 3. データ格納位置の計算: ImageBufferの y 行目の開始位置
        // (y * width * CHANNELS) を指すポインタ
        size_t row_start_index = static_cast<size_t>(y) * width * BYTES_PER_PIXEL;
        unsigned char* current_row_dest = dest_ptr + row_start_index;

        // 4. 1行内のピクセルを処理 (BGRをRGBに並べ替えて格納)
        for (long x = 0; x < width; ++x) {
            long src_index = x * BYTES_PER_PIXEL; // 読み込んだ row_data 内の B の位置
            long dest_index = x * BYTES_PER_PIXEL; // 格納先の R の位置

            // BMP (BGR) から ImageBuffer (RGB) へコピー
            current_row_dest[dest_index + 0] = row_data[src_index + 2]; // R = row_data[B+2]
            current_row_dest[dest_index + 1] = row_data[src_index + 1]; // G = row_data[B+1]
            current_row_dest[dest_index + 2] = row_data[src_index + 0]; // B = row_data[B]
        }
    }

	file.close();

    return true;
}

/**
 * @brief ImageBufferの内容を24bit BMPファイルとして出力します。
 * @param buffer 出力元のImageBufferオブジェクト
 * @param filename 出力ファイル名
 * @return bool 成功/失敗
 */
bool write_bmp_file(const ImageBuffer& buffer, const std::string& filename) {
    // 幅と高さを取得
    const int width = buffer.get_width();
    const int height = buffer.get_height();
    const int CHANNELS = 3; // R, G, B

    if (width <= 0 || height <= 0) {
        print("Error: Image dimensions are invalid.");
        return false;
    }

    // 各行に必要なパディングバイト数を計算 (行バイト長が4の倍数になるように)
    // 24bit (3バイト) * 幅
    const int row_byte_size = width * CHANNELS; 
    // row_byte_sizeを4で割った余りを計算し、4から引く。ただし余りが0の場合はパディングは0
    const int padding_size = (4 - (row_byte_size % 4)) % 4; 
    
    // パディングを含む1行の合計バイト数
    const int padded_row_size = row_byte_size + padding_size;
    
    // ピクセルデータ全体のサイズ
    const unsigned int image_size = padded_row_size * height;
    
    // ファイル全体のサイズ (ヘッダー54バイト + ピクセルデータサイズ)
    const unsigned int file_size = 54 + image_size;

    // ヘッダーを準備
    BMPFileHeader file_header;
    file_header.file_size = file_size;

    BMPInfoHeader info_header;
    info_header.width = width;
    info_header.height = height; // BMPは通常、正の値で左下から上へ
    info_header.image_size = image_size;

    // ファイルを開く (バイナリモードで出力)
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        print(("Error: Could not open file " + filename + " for writing.").c_str());
        return false;
    }

    // 1. ヘッダーを書き込み (合計54バイト)
    ofs.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    ofs.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));

    // 2. ピクセルデータを書き込み
    // BMPは通常、左下から上に向かって書き込むため、行を逆順に処理する (y = height - 1 から 0 へ)
    
    // パディング用のゼロデータ
    const unsigned char padding_byte = 0;

    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            // ImageBufferは R, G, B の順。BMPは B, G, R の順で書き込む
            unsigned char R = buffer.get_pixel_value(x, y, 0); // R
            unsigned char G = buffer.get_pixel_value(x, y, 1); // G
            unsigned char B = buffer.get_pixel_value(x, y, 2); // B

            // B, G, R の順で書き込み
            ofs.write(reinterpret_cast<const char*>(&B), 1);
            ofs.write(reinterpret_cast<const char*>(&G), 1);
            ofs.write(reinterpret_cast<const char*>(&R), 1);
        }

        // 行の終わりにパディングを書き込み
        for (int p = 0; p < padding_size; ++p) {
            ofs.write(reinterpret_cast<const char*>(&padding_byte), 1);
        }
    }

    ofs.close();
	print(("Successfully wrote BMP file: " + filename).c_str());
	
    return true;
}

// 24ビットBMPファイルからBlock構造体を読み込む
FilterPlugIn::Block read24BitBmpBlock(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        print(("Error: ファイルが見つかりません: " + filename).c_str());
        return FilterPlugIn::Block{};
    }

    // --- 1. ヘッダー情報の読み込みと抽出 ---
    
    // 幅 (0x12) と 高さ (0x16) の位置にシーク
    FilterPlugIn::Int width = 0, height = 0;
    file.seekg(0x12, std::ios::beg);
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);

    // データ開始オフセット (0x0A) の位置にシーク
    FilterPlugIn::Int dataOffset = 0;
    file.seekg(0x0A, std::ios::beg);
    file.read(reinterpret_cast<char*>(&dataOffset), 4);

    if (width <= 0 || height == 0) {
        print("Error: BMPサイズが不正です。");
        return FilterPlugIn::Block{};
    }

    // --- 2. パディングを含む物理的な行バイト数とデータサイズの計算 ---
    const FilterPlugIn::Int PIXEL_BYTES = 3; // 24bit = 3 bytes/pixel
    
    // 論理的な行バイト数 (パディングなし)
    FilterPlugIn::Int logicalRowBytes = width * PIXEL_BYTES;
    
    // BMPの物理的な行バイト数 (4の倍数に切り上げ)
    // actualRowBytes = ((3 * width) + 3) & (~3); 
    FilterPlugIn::Int actualRowBytes = ((logicalRowBytes + 3) / 4) * 4;
    
    FilterPlugIn::Int physicalDataSize = height * actualRowBytes;

	print(("physicalDataSize:" + std::to_string(physicalDataSize)).c_str());
	// print(("dataOffset:" + std::to_string(dataOffset)).c_str());

    // --- 3. ファイルからの物理データ読み込み ---
    auto rawData = std::make_unique<unsigned char[]>(physicalDataSize);
    file.seekg(dataOffset, std::ios::beg);
    file.read(reinterpret_cast<char*>(rawData.get()), physicalDataSize);

    // --- 4. 連続メモリへの再配置と上下反転処理 (ここが重要) ---
    
    // 最終的に Block に格納する、パディングなしの連続データ領域
    FilterPlugIn::Int finalDataSize = height * logicalRowBytes;
    auto finalData = std::make_unique<unsigned char[]>(finalDataSize);

    for (int y = 0; y < height; ++y) {
        // BMPは通常「下から上」に格納されている。
        // ファイルの (height - 1 - y) 行目を、メモリの (y) 行目にコピーする。
        
        // ファイルから読み込んだデータ配列内の対応する行ポインタ
        const unsigned char* srcRow = rawData.get() + (height - 1 - y) * actualRowBytes;
        
        // 最終的な連続データ配列内の対応する行ポインタ
        unsigned char* dstRow = finalData.get() + y * logicalRowBytes;
        
        // パディングを除いてコピーする (logicalRowBytes のみコピー)
        std::memcpy(dstRow, srcRow, logicalRowBytes);
    }
    
    // --- 5. Block構造体の設定 ---
    FilterPlugIn::Block block;
    block.rect = {0, 0, width, height};
    block.rowBytes = logicalRowBytes;   // パディングなし
    block.pixelBytes = PIXEL_BYTES;     // 3バイト
    
    // 24ビットBMPは通常 BGR 形式 (B=0, G=1, R=2)
    block.r = 2; block.g = 1; block.b = 0;
    block.needOffset = false;

    // メモリのアドレスを設定
    block.address = finalData.get();

	// print(convert_address_to_hex_string(finalData.get()).c_str());
    
    print(("read: " + filename + ", RowBytes: " + std::to_string( logicalRowBytes) 
	    + " (real file : " + std::to_string(actualRowBytes) + ")\n").c_str());
	
	file.close();
    return block;
}

// ファイル名で利用するため現在日時を取得
std::string getDateString() {
	// 現在日時を取得
	auto now = std::chrono::system_clock::now();
	auto t = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&t);

	// yyyyMMdd形式の文字列を生成
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y%m%d%H%M%S");
	return oss.str();
}

/// フィルタ実行f
/// @return 正常終了ならtrue
static bool RunFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data) {
	print("RunFilter start");
	FilterPlugIn::Run run(server);
	auto info = static_cast<FilterInfo*>(*data);
	info->server = server;

	// 前回の設定で開く
	FilterPlugIn::Property property(server, run.GetProperty());
	SwitchToSetting(info->setting, property, true);
	auto refreshSelectedSubImages = [&]() {
		for (size_t i = 0; i < kSubImageDropdownCount; ++i) {
			const int selection = info->subimage_indices[i];
			if (IsNoImageSelection(selection)) {
				g_params.input_subimage_filenames[i].clear();
			} else {
				g_params.input_subimage_filenames[i] = g_SubImages[selection];
			}
			std::string logMessage = "subimage_selection[" + std::to_string(i) + "] : ";
			logMessage += g_params.input_subimage_filenames[i].empty() ? kNoImageDisplayName : g_params.input_subimage_filenames[i];

			print(logMessage.c_str());
		}

		property.setEnumeration(ITEM_SUBIMAGE, info->subimage_indices[0]);
		property.setEnumeration(ITEM_SUBIMAGE_PICTURE3, info->subimage_indices[1]);
		property.setEnumeration(ITEM_SUBIMAGE_PICTURE4, info->subimage_indices[2]);
	};
	refreshSelectedSubImages();

	// 選択範囲の取得
	const auto selectAreaRect = run.GetSelectArea();
	const auto width = selectAreaRect.right - selectAreaRect.left;
	const auto height = selectAreaRect.bottom - selectAreaRect.top;
	const auto offsetX = selectAreaRect.left;
	const auto offsetY = selectAreaRect.top;

	print("selectArea:");
	print(std::to_string(width).c_str());
	print(std::to_string(height).c_str());
	print(std::to_string(offsetX).c_str());
	print(std::to_string(offsetY).c_str());

	// オフスクリーンの取得
	FilterPlugIn::Offscreen offscreenSource(server), offscreenDestination(server), offscreenSelectArea(server);
	offscreenSource.GetSource();
	offscreenDestination.GetDestination();
	offscreenSelectArea.GetSelectArea();

	// メイン処理
	while (true) {
		if (run.Process(FilterPlugIn::Run::States::Start) == FilterPlugIn::Run::Results::Exit) break;
		refreshSelectedSubImages();

		// パラメータの取得
		// 入力画像の取得
		ImageBuffer inputImageBuffer;
		inputImageBuffer.allocate(width, height);
		inputImageBuffer.rect.top = offsetY;
		inputImageBuffer.rect.left = offsetX;
		inputImageBuffer.rect.bottom = offsetY + inputImageBuffer.get_height();
		inputImageBuffer.rect.right = offsetX + inputImageBuffer.get_width();
		auto sourceRects = offscreenSource.GetBlockRects(selectAreaRect);
		for (const auto& rect : sourceRects) {
			if (run.Process(FilterPlugIn::Run::States::Continue) != FilterPlugIn::Run::Results::Continue) break;
			FilterPlugIn::Block srcBlock = offscreenSource.GetBlockImage(rect);
			// print("offscreenSource srcBlock:");
			// print(std::to_string(srcBlock.rect.top).c_str());
			// print(std::to_string(srcBlock.rect.left).c_str());
			// print(std::to_string(srcBlock.rect.bottom).c_str());
			// print(std::to_string(srcBlock.rect.right).c_str());
			Transfer(inputImageBuffer, srcBlock, offsetY, offsetX);
		}
		std::string datetimenow = getDateString();
		std::string inputImageFileName = "temp_img_req_" + datetimenow;
		std::array<std::string, kSubImageDropdownCount> subImageUploadFileNames{};
		std::string tempImageFileName = "temp_img_req";
		write_bmp_file(inputImageBuffer, g_BasePath + tempImageFileName +".bmp");

		// 入力画像をPNGに変換
		call_bmp_to_png(tempImageFileName + ".bmp");

		// 入力画像を事前にPOST
        std::string url = g_ServerAddress + "/upload/image";
		http_post_image_to_file(url, g_BasePath + tempImageFileName + ".png", inputImageFileName + ".png", "temp_json_preimage_res.json");

		for (size_t i = 0; i < kSubImageDropdownCount; ++i) {
			const auto& selectedSubImage = g_params.input_subimage_filenames[i];
			if (!selectedSubImage.empty()) {
				const std::string uploadFileName = kSubImageUploadPrefixes[i] + datetimenow + ".png";
				const std::string localPath = g_BasePath + "SubImage\\" + selectedSubImage;
				const std::string responseFile = "temp_json_presubimage_res_" + std::to_string(i) + ".json";
				print(("pre-post subimage[" + std::to_string(i) + "]: " + localPath).c_str());
				http_post_image_to_file(url, localPath, uploadFileName, responseFile);
				subImageUploadFileNames[i] = uploadFileName;
			} else {
				subImageUploadFileNames[i] = "empty.png";
				print(("skip pre-post subimage[" + std::to_string(i) + "]: " + kNoImageDisplayName).c_str());
			}
		}

		// 生成
		// JSONファイルを読み込む
		std::string prompt_original = read_file_to_string(g_BasePath + g_params.template_workflow_filename);
		if (prompt_original.empty()) {
			print("Aborting process.");
			return false;
		}

		FilterPlugIn::Block outputBlock;

		print("Replace prompt");

		// 2. 読み込んだJSON文字列内のマーカーを置換する
		std::string prompt_modified = replace_all(prompt_original, MARKER_PROMPT, g_params.prompt);
		prompt_modified = replace_all(prompt_modified, MARKER_NPROMPT, g_params.negative_prompt);
		print("Replace input image path");
		prompt_modified = replace_all(prompt_modified, MARKER_INPUT_IMAGE, inputImageFileName + ".png");
		for (size_t i = 0; i < kSubImageDropdownCount; ++i) {
			prompt_modified = replace_all(prompt_modified, kSubImageMarkers[i], subImageUploadFileNames[i]);
		}
		
		print("Replace finished.");

		// 3. 変更したワークフローをキューに送信
		std::string temp_image_path = queue_prompt(prompt_modified);

		if (temp_image_path == "") {
    		print("Generate error.");
			return false;
		} 

		call_png_to_bmp();

		print("Output to layer.");

		ImageBuffer outputImageBuffer;
		load_bmp_rgb_to_buffer(g_BasePath + "temp_img_res.bmp", outputImageBuffer);
		outputBlock = read24BitBmpBlock(g_BasePath + "temp_img_res.bmp");
        if (!outputBlock.address) print("address is error");

		outputImageBuffer.rect.top = offsetY;
		outputImageBuffer.rect.left = offsetX;
		outputImageBuffer.rect.bottom = offsetY + outputImageBuffer.get_height();
		outputImageBuffer.rect.right = offsetX + outputImageBuffer.get_width();

		print("start transfer");

		// ブロック転送
		auto destRects = offscreenDestination.GetBlockRects(selectAreaRect);
		for (const auto& rect : destRects) {
			if (run.Process(FilterPlugIn::Run::States::Continue) != FilterPlugIn::Run::Results::Continue) break;

    		// print("transfer1");
			// 転送先ブロック
			FilterPlugIn::Block imageBlock = offscreenDestination.GetBlockImage(rect);
			FilterPlugIn::Block alphaBlock = offscreenDestination.GetBlockAlpha(rect);

			if (offscreenSelectArea) {
				// 選択範囲ブロック
				FilterPlugIn::Block selectBlock = offscreenSelectArea.GetBlockSelectArea(rect);

        		// print("transfer2");
				// 選択範囲（マスク）付きで描画
				Transfer(imageBlock, outputImageBuffer, alphaBlock, selectBlock);
			} else {
        		// print("transfer2-2");
				// 選択範囲なしで描画（透明ピクセルは埋めない）
				// Transfer(imageBlock, outputBlock, alphaBlock);
				Transfer(imageBlock, outputImageBuffer, alphaBlock);
			}

    		// print("transfer3");
			run.UpdateRect(rect);
		}
  		print("end transfer");
		if (run.Result() == FilterPlugIn::Run::Results::Restart) continue;
		if (run.Result() == FilterPlugIn::Run::Results::Exit) break;

		// 継続確認
		if (run.Process(FilterPlugIn::Run::States::End) != FilterPlugIn::Run::Results::Restart) break;
	}

	return true;
}


/// @brief ブロック転送
/// @param dst 転送先のブロック
/// @param src 転送元のブロック
void Transfer(const ImageBuffer& dst, const FilterPlugIn::Block& src, int offsetY, int offsetX) {
	FilterPlugIn::Rect dst_rect;
	dst_rect.top = offsetY;
	dst_rect.bottom = offsetY + dst.get_height();
	dst_rect.left = offsetX;
	dst_rect.right = offsetX + dst.get_width();
	const auto rect = FilterPlugIn::intersectRects(dst_rect, src.rect);
	if (FilterPlugIn::isRectEmpty(rect)) return;

	// const auto dstRowBytes = dst.rowBytes;
	// const auto dstPixelBytes = dst.pixelBytes;
	// const auto dstR = dst.r, dstG = dst.g, dstB = dst.b;

	const auto srcRowBytes = src.rowBytes;
	const auto srcPixelBytes = src.pixelBytes;
	const auto srcR = src.r, srcG = src.g, srcB = src.b;

	const auto cols = rect.right - rect.left;
	const auto rows = rect.bottom - rect.top;
	// pbyte_t pDstRow = static_cast<pbyte_t>(dst.address) + addressOffset(dst, rect);
	pbyte_t pSrcRow = static_cast<pbyte_t>(src.address) + FilterPlugIn::addressOffset(src, rect);
	for (int y = 0; y < rows; ++y) {
		pbyte_t pSrc = pSrcRow;
		// pbyte_t pDst = pDstRow;
		for (int x = 0; x < cols; ++x) {
			// pDst[dstR] = pSrc[srcR];
			// pDst[dstG] = pSrc[srcG];
			// pDst[dstB] = pSrc[srcB];
			dst.set_pixel_value(x + src.rect.left - offsetX, y + src.rect.top - offsetY, 0, pSrc[srcR]);
			dst.set_pixel_value(x + src.rect.left - offsetX, y + src.rect.top - offsetY, 1, pSrc[srcG]);
			dst.set_pixel_value(x + src.rect.left - offsetX, y + src.rect.top - offsetY, 2, pSrc[srcB]);
			pSrc += srcPixelBytes;
			// pDst += dstPixelBytes;
		}
		pSrcRow += srcRowBytes;
		// pDstRow += dstRowBytes;
	}
}


/// @brief ブロック転送（アルファ付き）
/// @param dst 転送先のブロック
/// @param src 転送元のブロック
/// @param alpha 転送先のアルファチャンネル
void Transfer(const FilterPlugIn::Block& dst, const ImageBuffer& src, const FilterPlugIn::Block& alpha) {
	const auto rect = FilterPlugIn::intersectRects(dst.rect, src.rect);
	if (FilterPlugIn::isRectEmpty(rect)) return;

	const auto dstRowBytes = dst.rowBytes;
	const auto dstPixelBytes = dst.pixelBytes;
	const auto dstR = dst.r, dstG = dst.g, dstB = dst.b;

	// const auto srcRowBytes = src.rowBytes;
	// const auto srcPixelBytes = src.pixelBytes;
	// const auto srcR = src.r, srcG = src.g, srcB = src.b;

	const auto alpRowBytes = alpha.rowBytes;
	const auto alpPixelBytes = alpha.pixelBytes;

	const auto cols = rect.right - rect.left;
	const auto rows = rect.bottom - rect.top;
	pbyte_t pDstRow = static_cast<pbyte_t>(dst.address) + FilterPlugIn::addressOffset(dst, rect);
	//pbyte_t pSrcRow = static_cast<pbyte_t>(src.address) + addressOffset(src, rect);
	pbyte_t pAlpRow = static_cast<pbyte_t>(alpha.address) + FilterPlugIn::addressOffset(alpha, rect);
	for (int y = 0; y < rows; ++y) {
		//pbyte_t pSrc = pSrcRow;
		pbyte_t pDst = pDstRow;
		pbyte_t pAlp = pAlpRow;
		for (int x = 0; x < cols; ++x) {
			//print(std::to_string(x).c_str());
			//print(std::to_string(y).c_str());
			if (*pAlp > 0) {
				//if (pDst[dstR] != NULL) print("pDst[dstR]");
				//if (pSrc[srcR] != NULL) print("pSrc[srcR]");
				//print("pAlp");
				// print((const char*)src.get_pixel_value(x, y, 2));
				// print((const char*)src.get_pixel_value(x, y, 1));
				// print((const char*)src.get_pixel_value(x, y, 0));
				pDst[dstR] = src.get_pixel_value(x + rect.left, y + rect.top, 0);
				pDst[dstG] = src.get_pixel_value(x + rect.left, y + rect.top, 1);
				pDst[dstB] = src.get_pixel_value(x + rect.left, y + rect.top, 2);
			}
			//pSrc += srcPixelBytes;
			pDst += dstPixelBytes;
			pAlp += alpPixelBytes;
		}
		//pSrcRow += srcRowBytes;
		pDstRow += dstRowBytes;
		pAlpRow += alpRowBytes;
	}
}


/// @brief ブロック転送（アルファ＆選択マスク付き）
/// @param dst 転送先のブロック
/// @param src 転送元のブロック
/// @param alpha 転送先のアルファチャンネル
/// @param select 転送元のアルファチャンネル（選択領域用）
void Transfer(const FilterPlugIn::Block& dst, const ImageBuffer& src, const FilterPlugIn::Block& alpha, const FilterPlugIn::Block& select) {
	
	const auto rect = FilterPlugIn::intersectRects(dst.rect, src.rect);
	if (FilterPlugIn::isRectEmpty(rect)) return;

	const auto dstRowBytes = dst.rowBytes;
	const auto dstPixelBytes = dst.pixelBytes;
	const auto dstR = dst.r, dstG = dst.g, dstB = dst.b;

	// const auto srcRowBytes = src.rowBytes;
	// const auto srcPixelBytes = src.pixelBytes;
	// const auto srcR = src.r, srcG = src.g, srcB = src.b;

	const auto alpRowBytes = alpha.rowBytes;
	const auto alpPixelBytes = alpha.pixelBytes;

	const auto selRowBytes = select.rowBytes;
	const auto selPixelBytes = select.pixelBytes;

	const auto cols = rect.right - rect.left;
	const auto rows = rect.bottom - rect.top;
	pbyte_t pDstRow = static_cast<pbyte_t>(dst.address) + FilterPlugIn::addressOffset(dst, rect);
	// pbyte_t pSrcRow = static_cast<pbyte_t>(src.address) + addressOffset(src, rect);
	pbyte_t pAlpRow = static_cast<pbyte_t>(alpha.address) + FilterPlugIn::addressOffset(alpha, rect);
	pbyte_t pSelRow = static_cast<pbyte_t>(select.address) + FilterPlugIn::addressOffset(select, rect);
	for (int y = 0; y < rows; ++y) {
		// pbyte_t pSrc = pSrcRow;
		pbyte_t pDst = pDstRow;
		pbyte_t pAlp = pAlpRow;
		pbyte_t pSel = pSelRow;
		for (int x = 0; x < cols; ++x) {
			if (*pAlp > 0) {
				uint16_t alpha = *pSel;
				// pDst[dstR] = BlendFunction(pDst[dstR], pSrc[srcR], alpha);
				// pDst[dstG] = BlendFunction(pDst[dstG], pSrc[srcG], alpha);
				// pDst[dstB] = BlendFunction(pDst[dstB], pSrc[srcB], alpha);
				pDst[dstR] = FilterPlugIn::BlendFunction(pDst[dstR], src.get_pixel_value(x + rect.left - src.rect.left, y + rect.top - src.rect.top, 0), alpha);
				pDst[dstG] = FilterPlugIn::BlendFunction(pDst[dstG], src.get_pixel_value(x + rect.left - src.rect.left, y + rect.top - src.rect.top, 1), alpha);
				pDst[dstB] = FilterPlugIn::BlendFunction(pDst[dstB], src.get_pixel_value(x + rect.left - src.rect.left, y + rect.top - src.rect.top, 2), alpha);
			}
			// pSrc += srcPixelBytes;
			pDst += dstPixelBytes;
			pAlp += alpPixelBytes;
			pSel += selPixelBytes;
		}
		// pSrcRow += srcRowBytes;
		pDstRow += dstRowBytes;
		pAlpRow += alpRowBytes;
		pSelRow += selRowBytes;
	}
}

/// @brief プラグインのエントリーポイント
/// @param result ここに成否の結果をつっこむ
/// @param data 共有データ
/// @param selector 処理のセレクタ（Run以外は初期化/解放１回ずつ来る）
/// @param server 処理サーバー（ホスト側アクセスが全部入ってる）
/// @param reserved 予約済みかな？
/// @return 無し
extern "C" __declspec(dllexport) void TriglavPluginCall(FilterPlugIn::CallResult* result, FilterPlugIn::Ptr* data, FilterPlugIn::Selector selector, FilterPlugIn::Server* server, void* reserved) {
	*result = FilterPlugIn::CallResult::Failed;

	// 生きてないと困るものをチェック
	if (!server) return;
	if (!server->serviceSuite.stringService) return;
	if (!server->serviceSuite.propertyService) return;
	if (!server->serviceSuite.propertyService2) return;
	if (!server->serviceSuite.offscreenService) return;

	// 処理の振り分け
	switch (selector) {
	case FilterPlugIn::Selector::ModuleInitialize:
		if (!server->recordSuite.moduleInitializeRecord) return;
		if (!InitializeModule(server, data)) return;
		break;
	case FilterPlugIn::Selector::ModuleTerminate:
		if (!TerminateModule(server, data)) return;
		break;
	case FilterPlugIn::Selector::FilterInitialize:
		print("InitializeFilter {");
		if (!server->recordSuite.filterInitializeRecord) return;
		if (!InitializeFilter(server, data)) return;
		print("InitializeFilter }");
		break;
	case FilterPlugIn::Selector::FilterTerminate:
		if (!TerminateFilter(server, data)) return;
		break;
	case FilterPlugIn::Selector::FilterRun:
		print("RunFilter {");
		if (!server->recordSuite.filterRunRecord) return;
		if (!RunFilter(server, data)) return;
		print("RunFilter }");
		break;
	}
	*result = FilterPlugIn::CallResult::Success;
}

