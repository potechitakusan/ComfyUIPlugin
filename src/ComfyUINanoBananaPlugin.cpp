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
#include <algorithm>
#include <wchar.h> // ワイド文字用（日本語フォルダ用を想定したがなくても良いかも）
#include <chrono>  // sleep_for
#include <thread>  // sleep_for

#include "ComfyUIPlugin.h"
#include "FilterPlugIn.h"

using namespace ComfyUIPlugin;

/// このプラグインのモジュールID（GUID）
/// @note プラグイン毎に違う値にしないと駄目
constexpr auto kModuleIDString = "bdf2dfd5-5405-4803-6343-83869a513339";

const std::string PLUGIN_MODE = "Banana";

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
		if (!InitializeModule(server, data, kModuleIDString)) return;
		break;
	case FilterPlugIn::Selector::ModuleTerminate:
		if (!TerminateModule(server, data)) return;
		break;
	case FilterPlugIn::Selector::FilterInitialize:
		//print("InitializeNanoBananaFilter {");
		if (!server->recordSuite.filterInitializeRecord) return;
		if (!InitializeFilter(server, data, PLUGIN_MODE)) return;
		//print("InitializeNanoBananaFilter }");
		break;
	case FilterPlugIn::Selector::FilterTerminate:
		if (!TerminateFilter(server, data)) return;
		break;
	case FilterPlugIn::Selector::FilterRun:
		//print("RunFilter {");
		if (!server->recordSuite.filterRunRecord) return;
		if (!RunFilter(server, data, PLUGIN_MODE)) return;
		//print("RunFilter }");
		break;
	}
	*result = FilterPlugIn::CallResult::Success;
}

