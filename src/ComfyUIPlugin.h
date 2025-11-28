/**
 * @file ComfyUIPlugin.h
 * @author consomme hollywood
 * @brief 基本ヘッダー
 * @note ライブラリ系はpch.hに
 */
#pragma once


#include "FilterPlugIn.h"
#include <string>

// 共通処理（ComfyUIPlugin.cpp）を別エントリポイントから再利用するための宣言
bool InitializeModule(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data, std::string id);
bool TerminateModule(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data);
bool InitializeFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data, std::string mode);
bool TerminateFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data);
bool RunFilter(FilterPlugIn::Server* server, FilterPlugIn::Ptr* data, std::string mode);

namespace ComfyUIPlugin {

}

