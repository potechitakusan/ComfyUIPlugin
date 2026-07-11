/**
 * @file ComvertImage.h
 * @author consomme hollywood
 * @brief Windows標準のWICを利用した画像形式変換
 */
#pragma once

#include <string>

namespace ComvertImage {

/// 24bit BMPをPNGへ変換する。
bool BmpToPng(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage = nullptr);

/// PNGをアルファなし24bit BMPへ変換する。
bool PngToBmp(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage = nullptr);

}
