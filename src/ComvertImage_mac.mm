/**
 * @file ComvertImage_mac.mm
 * @brief macOS 標準の ImageIO/CoreGraphics による画像変換
 */
#include "pch.h"
#include "ComvertImage.h"

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <cstring>
#include <vector>

namespace {

CFURLRef CreateFileUrl(const std::string& path) {
	return CFURLCreateFromFileSystemRepresentation(
		kCFAllocatorDefault,
		reinterpret_cast<const UInt8*>(path.data()),
		static_cast<CFIndex>(path.size()),
		false);
}

bool SetError(std::string* errorMessage, const char* message) {
	if (errorMessage) *errorMessage = message;
	return false;
}

CGImageRef LoadImage(const std::string& path, std::string* errorMessage) {
	CFURLRef url = CreateFileUrl(path);
	if (!url) { SetError(errorMessage, "Could not create input file URL."); return nullptr; }
	CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
	CFRelease(url);
	if (!source) { SetError(errorMessage, "ImageIO could not open the input image."); return nullptr; }
	CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
	CFRelease(source);
	if (!image) SetError(errorMessage, "ImageIO could not decode the input image.");
	return image;
}

bool WritePng(CGImageRef image, const std::string& outputPath, std::string* errorMessage) {
	CFURLRef url = CreateFileUrl(outputPath);
	if (!url) return SetError(errorMessage, "Could not create output file URL.");
	CGImageDestinationRef destination = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
	CFRelease(url);
	if (!destination) return SetError(errorMessage, "ImageIO could not create PNG destination.");
	CGImageDestinationAddImage(destination, image, nullptr);
	const bool result = CGImageDestinationFinalize(destination);
	CFRelease(destination);
	return result ? true : SetError(errorMessage, "ImageIO could not write PNG output.");
}

#pragma pack(push, 1)
struct BmpFileHeader {
	uint16_t type = 0x4D42;
	uint32_t size = 0;
	uint16_t reserved1 = 0;
	uint16_t reserved2 = 0;
	uint32_t offset = 54;
};
struct BmpInfoHeader {
	uint32_t size = 40;
	int32_t width = 0;
	int32_t height = 0;
	uint16_t planes = 1;
	uint16_t bitCount = 24;
	uint32_t compression = 0;
	uint32_t imageSize = 0;
	int32_t xPixelsPerMeter = 0;
	int32_t yPixelsPerMeter = 0;
	uint32_t colorsUsed = 0;
	uint32_t colorsImportant = 0;
};
#pragma pack(pop)

bool Write24BitBmp(CGImageRef image, const std::string& outputPath, std::string* errorMessage) {
	const size_t width = CGImageGetWidth(image), height = CGImageGetHeight(image);
	if (width == 0 || height == 0 || width > INT32_MAX || height > INT32_MAX) return SetError(errorMessage, "Invalid image dimensions.");
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	const size_t sourceStride = width * 4;
	std::vector<uint8_t> pixels(sourceStride * height);
	CGContextRef context = CGBitmapContextCreate(pixels.data(), width, height, 8, sourceStride, colorSpace, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(colorSpace);
	if (!context) return SetError(errorMessage, "CoreGraphics could not create bitmap context.");
	CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
	CGContextRelease(context);

	const size_t rowSize = ((width * 3 + 3) / 4) * 4;
	const size_t imageSize = rowSize * height;
	if (imageSize > UINT32_MAX) return SetError(errorMessage, "BMP image is too large.");
	BmpFileHeader fileHeader{}; fileHeader.size = static_cast<uint32_t>(54 + imageSize);
	BmpInfoHeader infoHeader{}; infoHeader.width = static_cast<int32_t>(width); infoHeader.height = static_cast<int32_t>(height); infoHeader.imageSize = static_cast<uint32_t>(imageSize);
	FILE* file = std::fopen(outputPath.c_str(), "wb");
	if (!file) return SetError(errorMessage, "Could not open BMP output file.");
	const bool headerOk = std::fwrite(&fileHeader, sizeof(fileHeader), 1, file) == 1 && std::fwrite(&infoHeader, sizeof(infoHeader), 1, file) == 1;
	std::vector<uint8_t> row(rowSize, 0);
	bool writeOk = headerOk;
	for (size_t y = height; writeOk && y-- > 0;) {
		for (size_t x = 0; x < width; ++x) { const uint8_t* pixel = &pixels[(y * width + x) * 4]; row[x * 3] = pixel[0]; row[x * 3 + 1] = pixel[1]; row[x * 3 + 2] = pixel[2]; }
		writeOk = std::fwrite(row.data(), 1, rowSize, file) == rowSize;
	}
	std::fclose(file);
	return writeOk ? true : SetError(errorMessage, "Could not write BMP output.");
}

} // namespace

namespace ComvertImage {

bool BmpToPng(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage) {
	if (errorMessage) errorMessage->clear();
	CGImageRef image = LoadImage(inputPath, errorMessage); if (!image) return false;
	const bool result = WritePng(image, outputPath, errorMessage);
	CGImageRelease(image);
	return result;
}

bool PngToBmp(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage) {
	if (errorMessage) errorMessage->clear();
	CGImageRef image = LoadImage(inputPath, errorMessage); if (!image) return false;
	const bool result = Write24BitBmp(image, outputPath, errorMessage);
	CGImageRelease(image);
	return result;
}

bool WriteRgbaPng(const std::string& outputPath, const unsigned char* bgraPixels, int width, int height, std::string* errorMessage) {
	if (errorMessage) errorMessage->clear();
	if (!bgraPixels || width <= 0 || height <= 0) return SetError(errorMessage, "Invalid BGRA image buffer.");
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	std::vector<uint8_t> rgba(pixelCount * 4);
	for (size_t i = 0; i < pixelCount; ++i) { rgba[i * 4] = bgraPixels[i * 4 + 2]; rgba[i * 4 + 1] = bgraPixels[i * 4 + 1]; rgba[i * 4 + 2] = bgraPixels[i * 4]; rgba[i * 4 + 3] = bgraPixels[i * 4 + 3]; }
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, rgba.data(), rgba.size(), nullptr);
	CGImageRef image = CGImageCreate(static_cast<size_t>(width), static_cast<size_t>(height), 8, 32, static_cast<size_t>(width) * 4, colorSpace, kCGImageAlphaLast | kCGBitmapByteOrder32Big, provider, nullptr, false, kCGRenderingIntentDefault);
	CGDataProviderRelease(provider); CGColorSpaceRelease(colorSpace);
	if (!image) return SetError(errorMessage, "CoreGraphics could not create RGBA image.");
	const bool result = WritePng(image, outputPath, errorMessage);
	CGImageRelease(image);
	return result;
}

} // namespace ComvertImage
#endif // defined(__APPLE__)
