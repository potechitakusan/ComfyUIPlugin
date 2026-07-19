/**
 * @file ComvertImage.cpp
 * @author consomme hollywood
 * @brief Windows標準のWICを利用した画像形式変換
 *
 * PNG/BMPのチャンクや圧縮を独自実装せず、Windowsに標準搭載されている
 * Windows Imaging Component (WIC) のネイティブコーデックを直接呼び出す。
 * 出力画素形式を24bpp BGRに統一し、既存のBMP読込処理との互換性を保つ。
 */
#include "pch.h"

#if defined(_WIN32)

#include "ComvertImage.h"

#include <wincodec.h>

#include <iomanip>
#include <sstream>

#pragma comment(lib, "windowscodecs.lib")

namespace {

template<class T>
class ComObject {
public:
	ComObject() = default;
	~ComObject() { reset(); }

	ComObject(const ComObject&) = delete;
	ComObject& operator=(const ComObject&) = delete;

	T* get() const { return value_; }
	T** put() {
		reset();
		return &value_;
	}
	T* operator->() const { return value_; }
	explicit operator bool() const { return value_ != nullptr; }

	void reset() {
		if (value_) {
			value_->Release();
			value_ = nullptr;
		}
	}

private:
	T* value_ = nullptr;
};

class ComInitializer {
public:
	ComInitializer() {
		result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		needsUninitialize_ = SUCCEEDED(result_);
		if (result_ == RPC_E_CHANGED_MODE) {
			// ホストが別のアパートメントモデルでCOMを初期化済みでもWICは利用できる。
			result_ = S_OK;
			needsUninitialize_ = false;
		}
	}

	~ComInitializer() {
		if (needsUninitialize_) CoUninitialize();
	}

	HRESULT result() const { return result_; }

private:
	HRESULT result_ = E_FAIL;
	bool needsUninitialize_ = false;
};

std::wstring LocalPathToWide(const std::string& path) {
	if (path.empty()) return {};
	const int length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
	if (length <= 0) return {};
	std::wstring result(static_cast<size_t>(length), L'\0');
	if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path.c_str(), -1, result.data(), length) <= 0) return {};
	result.resize(static_cast<size_t>(length - 1));
	return result;
}

std::string HResultMessage(const char* operation, HRESULT result) {
	std::ostringstream message;
	message << operation << " failed (HRESULT=0x"
		<< std::hex << std::uppercase << std::setw(8) << std::setfill('0')
		<< static_cast<unsigned long>(result) << ")";
	return message.str();
}

bool Fail(const char* operation, HRESULT result, std::string* errorMessage) {
	if (errorMessage) *errorMessage = HResultMessage(operation, result);
	return false;
}

bool Convert(
	const std::string& inputPath,
	const std::string& outputPath,
	REFGUID outputContainer,
	REFWICPixelFormatGUID requestedPixelFormat,
	std::string* errorMessage) {
	if (errorMessage) errorMessage->clear();

	const auto inputPathWide = LocalPathToWide(inputPath);
	const auto outputPathWide = LocalPathToWide(outputPath);
	if (inputPathWide.empty() || outputPathWide.empty()) {
		if (errorMessage) *errorMessage = "Image path conversion failed.";
		return false;
	}

	ComInitializer com;
	if (FAILED(com.result())) return Fail("CoInitializeEx", com.result(), errorMessage);

	ComObject<IWICImagingFactory> factory;
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(factory.put()));
	if (FAILED(hr)) return Fail("CoCreateInstance(CLSID_WICImagingFactory)", hr, errorMessage);

	ComObject<IWICBitmapDecoder> decoder;
	hr = factory->CreateDecoderFromFilename(
		inputPathWide.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		decoder.put());
	if (FAILED(hr)) return Fail("IWICImagingFactory::CreateDecoderFromFilename", hr, errorMessage);

	ComObject<IWICBitmapFrameDecode> decodedFrame;
	hr = decoder->GetFrame(0, decodedFrame.put());
	if (FAILED(hr)) return Fail("IWICBitmapDecoder::GetFrame", hr, errorMessage);

	UINT width = 0;
	UINT height = 0;
	hr = decodedFrame->GetSize(&width, &height);
	if (FAILED(hr) || width == 0 || height == 0) {
		return Fail("IWICBitmapFrameDecode::GetSize", FAILED(hr) ? hr : E_INVALIDARG, errorMessage);
	}

	ComObject<IWICFormatConverter> sourceConverter;
	hr = factory->CreateFormatConverter(sourceConverter.put());
	if (FAILED(hr)) return Fail("IWICImagingFactory::CreateFormatConverter", hr, errorMessage);

	BOOL canConvert = FALSE;
	WICPixelFormatGUID sourcePixelFormat{};
	hr = decodedFrame->GetPixelFormat(&sourcePixelFormat);
	if (FAILED(hr)) return Fail("IWICBitmapFrameDecode::GetPixelFormat", hr, errorMessage);
	hr = sourceConverter->CanConvert(sourcePixelFormat, requestedPixelFormat, &canConvert);
	if (FAILED(hr) || !canConvert) {
		return Fail("IWICFormatConverter::CanConvert", FAILED(hr) ? hr : WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT, errorMessage);
	}

	hr = sourceConverter->Initialize(
		decodedFrame.get(),
		requestedPixelFormat,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0,
		WICBitmapPaletteTypeCustom);
	if (FAILED(hr)) return Fail("IWICFormatConverter::Initialize", hr, errorMessage);

	// 前回の変換結果を誤って再利用しないよう、エンコード前に出力を削除する。
	DeleteFileW(outputPathWide.c_str());

	ComObject<IWICStream> outputStream;
	hr = factory->CreateStream(outputStream.put());
	if (FAILED(hr)) return Fail("IWICImagingFactory::CreateStream", hr, errorMessage);
	hr = outputStream->InitializeFromFilename(outputPathWide.c_str(), GENERIC_WRITE);
	if (FAILED(hr)) return Fail("IWICStream::InitializeFromFilename", hr, errorMessage);

	ComObject<IWICBitmapEncoder> encoder;
	hr = factory->CreateEncoder(outputContainer, nullptr, encoder.put());
	if (FAILED(hr)) return Fail("IWICImagingFactory::CreateEncoder", hr, errorMessage);
	hr = encoder->Initialize(outputStream.get(), WICBitmapEncoderNoCache);
	if (FAILED(hr)) return Fail("IWICBitmapEncoder::Initialize", hr, errorMessage);

	ComObject<IWICBitmapFrameEncode> encodedFrame;
	ComObject<IPropertyBag2> encoderOptions;
	hr = encoder->CreateNewFrame(encodedFrame.put(), encoderOptions.put());
	if (FAILED(hr)) return Fail("IWICBitmapEncoder::CreateNewFrame", hr, errorMessage);
	hr = encodedFrame->Initialize(encoderOptions.get());
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::Initialize", hr, errorMessage);
	hr = encodedFrame->SetSize(width, height);
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::SetSize", hr, errorMessage);

	double dpiX = 96.0;
	double dpiY = 96.0;
	if (FAILED(decodedFrame->GetResolution(&dpiX, &dpiY)) || dpiX <= 0.0 || dpiY <= 0.0) {
		dpiX = 96.0;
		dpiY = 96.0;
	}
	hr = encodedFrame->SetResolution(dpiX, dpiY);
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::SetResolution", hr, errorMessage);

	WICPixelFormatGUID encoderPixelFormat = requestedPixelFormat;
	hr = encodedFrame->SetPixelFormat(&encoderPixelFormat);
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::SetPixelFormat", hr, errorMessage);

	IWICBitmapSource* writeSource = sourceConverter.get();
	ComObject<IWICFormatConverter> encoderConverter;
	if (!IsEqualGUID(encoderPixelFormat, requestedPixelFormat)) {
		hr = factory->CreateFormatConverter(encoderConverter.put());
		if (FAILED(hr)) return Fail("IWICImagingFactory::CreateFormatConverter(encoder)", hr, errorMessage);
		hr = encoderConverter->Initialize(
			sourceConverter.get(),
			encoderPixelFormat,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom);
		if (FAILED(hr)) return Fail("IWICFormatConverter::Initialize(encoder)", hr, errorMessage);
		writeSource = encoderConverter.get();
	}

	hr = encodedFrame->WriteSource(writeSource, nullptr);
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::WriteSource", hr, errorMessage);
	hr = encodedFrame->Commit();
	if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::Commit", hr, errorMessage);
	hr = encoder->Commit();
	if (FAILED(hr)) return Fail("IWICBitmapEncoder::Commit", hr, errorMessage);

	return true;
}

}

namespace ComvertImage {

bool BmpToPng(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage) {
	return Convert(
		inputPath,
		outputPath,
		GUID_ContainerFormatPng,
		GUID_WICPixelFormat24bppBGR,
		errorMessage);
}

bool PngToBmp(const std::string& inputPath, const std::string& outputPath, std::string* errorMessage) {
	return Convert(
		inputPath,
		outputPath,
		GUID_ContainerFormatBmp,
		GUID_WICPixelFormat24bppBGR,
		errorMessage);
}

bool WriteRgbaPng(const std::string& outputPath, const unsigned char* rgbaPixels, int width, int height, std::string* errorMessage) {
	if (errorMessage) errorMessage->clear(); if (!rgbaPixels || width <= 0 || height <= 0) { if (errorMessage) *errorMessage = "Invalid RGBA image buffer."; return false; }
	const auto outputPathWide = LocalPathToWide(outputPath); if (outputPathWide.empty()) { if (errorMessage) *errorMessage = "Image path conversion failed."; return false; }
	const auto stride64 = static_cast<unsigned long long>(width) * 4; const auto bufferSize64 = stride64 * static_cast<unsigned long long>(height); if (stride64 > UINT_MAX || bufferSize64 > UINT_MAX) { if (errorMessage) *errorMessage = "RGBA image is too large."; return false; }
	ComInitializer com; if (FAILED(com.result())) return Fail("CoInitializeEx", com.result(), errorMessage); ComObject<IWICImagingFactory> factory; HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())); if (FAILED(hr)) return Fail("CoCreateInstance(CLSID_WICImagingFactory)", hr, errorMessage);
	ComObject<IWICBitmap> bitmap; hr = factory->CreateBitmapFromMemory(static_cast<UINT>(width), static_cast<UINT>(height), GUID_WICPixelFormat32bppBGRA, static_cast<UINT>(stride64), static_cast<UINT>(bufferSize64), const_cast<BYTE*>(reinterpret_cast<const BYTE*>(rgbaPixels)), bitmap.put()); if (FAILED(hr)) return Fail("IWICImagingFactory::CreateBitmapFromMemory", hr, errorMessage);
	DeleteFileW(outputPathWide.c_str()); ComObject<IWICStream> stream; hr = factory->CreateStream(stream.put()); if (FAILED(hr)) return Fail("IWICImagingFactory::CreateStream", hr, errorMessage); hr = stream->InitializeFromFilename(outputPathWide.c_str(), GENERIC_WRITE); if (FAILED(hr)) return Fail("IWICStream::InitializeFromFilename", hr, errorMessage);
	ComObject<IWICBitmapEncoder> encoder; hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()); if (FAILED(hr)) return Fail("IWICImagingFactory::CreateEncoder", hr, errorMessage); hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache); if (FAILED(hr)) return Fail("IWICBitmapEncoder::Initialize", hr, errorMessage);
	ComObject<IWICBitmapFrameEncode> frame; ComObject<IPropertyBag2> options; hr = encoder->CreateNewFrame(frame.put(), options.put()); if (FAILED(hr)) return Fail("IWICBitmapEncoder::CreateNewFrame", hr, errorMessage); hr = frame->Initialize(options.get()); if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::Initialize", hr, errorMessage); hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height)); if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::SetSize", hr, errorMessage);
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA; hr = frame->SetPixelFormat(&format); if (FAILED(hr) || !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) return Fail("IWICBitmapFrameEncode::SetPixelFormat", FAILED(hr) ? hr : WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT, errorMessage); hr = frame->WriteSource(bitmap.get(), nullptr); if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::WriteSource", hr, errorMessage); hr = frame->Commit(); if (FAILED(hr)) return Fail("IWICBitmapFrameEncode::Commit", hr, errorMessage); hr = encoder->Commit(); if (FAILED(hr)) return Fail("IWICBitmapEncoder::Commit", hr, errorMessage); return true;
}
}

#endif // defined(_WIN32)
