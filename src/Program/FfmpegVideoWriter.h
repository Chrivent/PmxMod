#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace Chrivent {
	// FFmpeg 자식 프로세스의 표준 입력으로 RGBA 프레임을 전달해 MP4를 생성한다.
	class FfmpegVideoWriter {
		HANDLE process = nullptr;
		HANDLE inputPipe = nullptr;
		std::filesystem::path outputPath;
		std::filesystem::path logPath;
		std::wstring errorMessage;
		bool completed = false;

		static std::filesystem::path FindExecutable();
		static std::wstring QuoteArgument(const std::wstring& argument);
		static std::wstring FormatWindowsError(DWORD errorCode);
		void CloseInputPipe();
		void CloseProcess();

	public:
		FfmpegVideoWriter() = default;
		~FfmpegVideoWriter();
		FfmpegVideoWriter(const FfmpegVideoWriter&) = delete;
		FfmpegVideoWriter& operator=(const FfmpegVideoWriter&) = delete;

		// 출력 파일과 선택적 음악 입력을 열고 FFmpeg 인코딩 프로세스를 시작한다.
		bool Start(const std::filesystem::path& destination, int width, int height, int frameRate,
			uint64_t frameCount, const std::filesystem::path& audioPath, double audioStartSeconds);
		// 한 프레임의 연속 RGBA 바이트를 FFmpeg 표준 입력으로 모두 기록한다.
		bool WriteFrame(std::span<const uint8_t> rgbaPixels);
		// 입력을 닫고 FFmpeg가 MP4 컨테이너를 마무리할 때까지 기다린다.
		bool Finish();
		// 실행 중인 인코더를 종료하고 완성되지 않은 출력 파일을 제거한다.
		void Cancel();

		const std::filesystem::path& GetOutputPath() const { return outputPath; }
		const std::filesystem::path& GetLogPath() const { return logPath; }
		const std::wstring& GetErrorMessage() const { return errorMessage; }
	};
}
