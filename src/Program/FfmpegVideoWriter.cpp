#include "Program/FfmpegVideoWriter.h"

#include <algorithm>
#include <format>
#include <limits>
#include <vector>

namespace Chrivent {
	namespace {
		void PumpWindowMessages() {
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
		}
	}

	FfmpegVideoWriter::~FfmpegVideoWriter() {
		Cancel();
	}

	std::filesystem::path FfmpegVideoWriter::FindExecutable() {
		std::vector<wchar_t> modulePath(32768);
		const DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath.data(),
			static_cast<DWORD>(modulePath.size()));
		if (moduleLength > 0 && moduleLength < modulePath.size()) {
			const std::filesystem::path adjacent = std::filesystem::path(
				std::wstring(modulePath.data(), moduleLength)).parent_path() / L"ffmpeg.exe";
			std::error_code existsError;
			if (std::filesystem::is_regular_file(adjacent, existsError))
				return adjacent;
		}

		std::vector<wchar_t> searchResult(32768);
		const DWORD searchLength = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
			static_cast<DWORD>(searchResult.size()), searchResult.data(), nullptr);
		if (searchLength == 0 || searchLength >= searchResult.size())
			return {};
		return std::filesystem::path(std::wstring(searchResult.data(), searchLength));
	}

	std::wstring FfmpegVideoWriter::QuoteArgument(const std::wstring& argument) {
		if (argument.empty())
			return L"\"\"";
		if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
			return argument;
		std::wstring quoted = L"\"";
		size_t backslashCount = 0;
		for (const wchar_t character : argument) {
			if (character == L'\\') {
				backslashCount++;
				continue;
			}
			if (character == L'\"') {
				quoted.append(backslashCount * 2 + 1, L'\\');
				quoted.push_back(character);
				backslashCount = 0;
				continue;
			}
			quoted.append(backslashCount, L'\\');
			backslashCount = 0;
			quoted.push_back(character);
		}
		quoted.append(backslashCount * 2, L'\\');
		quoted.push_back(L'\"');
		return quoted;
	}

	std::wstring FfmpegVideoWriter::FormatWindowsError(const DWORD errorCode) {
		wchar_t* buffer = nullptr;
		const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
		std::wstring message = length > 0 && buffer ? std::wstring(buffer, length)
			: std::format(L"Windows 오류 {}", errorCode);
		if (buffer)
			LocalFree(buffer);
		while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
			message.pop_back();
		return message;
	}

	void FfmpegVideoWriter::CloseInputPipe() {
		if (inputPipe) {
			CloseHandle(inputPipe);
			inputPipe = nullptr;
		}
	}

	void FfmpegVideoWriter::CloseProcess() {
		if (process) {
			CloseHandle(process);
			process = nullptr;
		}
	}

	bool FfmpegVideoWriter::Start(const std::filesystem::path& destination,
		const int width, const int height, const int frameRate,
		const uint64_t frameCount, const std::filesystem::path& audioPath,
		const double audioStartSeconds) {
		Cancel();
		completed = false;
		errorMessage.clear();
		outputPath = destination;
		logPath = destination;
		logPath.replace_extension(L".ffmpeg.log");

		const std::filesystem::path ffmpegPath = FindExecutable();
		if (ffmpegPath.empty()) {
			errorMessage = L"ffmpeg.exe를 찾지 못했습니다. 실행 파일 옆이나 PATH에 FFmpeg를 설치해 주세요.";
			return false;
		}

		std::vector<std::wstring> arguments{
			L"-hide_banner", L"-loglevel", L"error", L"-y",
			L"-thread_queue_size", L"64", L"-f", L"rawvideo", L"-pix_fmt", L"rgba",
			L"-s:v", std::format(L"{}x{}", width, height), L"-r", std::to_wstring(frameRate),
			L"-i", L"pipe:0"
		};
		std::error_code audioError;
		const bool hasAudio = !audioPath.empty() && std::filesystem::is_regular_file(audioPath, audioError);
		if (hasAudio) {
			arguments.emplace_back(L"-ss");
			arguments.emplace_back(std::format(L"{:.6f}", std::max(0.0, audioStartSeconds)));
			arguments.emplace_back(L"-i");
			arguments.emplace_back(audioPath.wstring());
		}
		arguments.insert(arguments.end(), { L"-map", L"0:v:0" });
		if (hasAudio)
			arguments.insert(arguments.end(), { L"-map", L"1:a:0", L"-c:a", L"aac",
				L"-b:a", L"320k", L"-af", L"apad" });
		else
			arguments.emplace_back(L"-an");
		arguments.insert(arguments.end(), {
			L"-c:v", L"libx264", L"-preset", L"ultrafast", L"-crf", L"15",
			L"-pix_fmt", L"yuv420p", L"-frames:v", std::to_wstring(frameCount),
			L"-t", std::format(L"{:.6f}", static_cast<double>(frameCount) / frameRate),
			L"-movflags", L"+faststart", outputPath.wstring()
		});

		std::wstring commandLine = QuoteArgument(ffmpegPath.wstring());
		for (const std::wstring& argument : arguments) {
			commandLine.push_back(L' ');
			commandLine += QuoteArgument(argument);
		}
		std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;
		HANDLE readPipe = nullptr;
		HANDLE writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &securityAttributes, 4 * 1024 * 1024)) {
			errorMessage = L"FFmpeg 입력 파이프를 만들지 못했습니다: " + FormatWindowsError(GetLastError());
			return false;
		}
		SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0);
		HANDLE logFile = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
			&securityAttributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (logFile == INVALID_HANDLE_VALUE) {
			errorMessage = L"FFmpeg 로그 파일을 만들지 못했습니다: " + FormatWindowsError(GetLastError());
			CloseHandle(readPipe);
			CloseHandle(writePipe);
			return false;
		}

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdInput = readPipe;
		startupInfo.hStdOutput = logFile;
		startupInfo.hStdError = logFile;
		PROCESS_INFORMATION processInfo{};
		const BOOL created = CreateProcessW(ffmpegPath.c_str(), mutableCommandLine.data(), nullptr,
			nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
		const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
		CloseHandle(readPipe);
		CloseHandle(logFile);
		if (!created) {
			CloseHandle(writePipe);
			errorMessage = L"FFmpeg를 시작하지 못했습니다: " + FormatWindowsError(createError);
			return false;
		}
		CloseHandle(processInfo.hThread);
		process = processInfo.hProcess;
		inputPipe = writePipe;
		return true;
	}

	bool FfmpegVideoWriter::WriteFrame(const std::span<const uint8_t> rgbaPixels) {
		if (!process || !inputPipe) {
			errorMessage = L"FFmpeg 인코더가 실행 중이 아닙니다.";
			return false;
		}
		size_t writtenTotal = 0;
		while (writtenTotal < rgbaPixels.size()) {
			const DWORD chunkSize = static_cast<DWORD>(std::min<size_t>(
				rgbaPixels.size() - writtenTotal, std::numeric_limits<DWORD>::max()));
			DWORD written = 0;
			if (!WriteFile(inputPipe, rgbaPixels.data() + writtenTotal, chunkSize, &written, nullptr)
				|| written == 0) {
				errorMessage = L"FFmpeg에 프레임을 전달하지 못했습니다: " + FormatWindowsError(GetLastError());
				return false;
			}
			writtenTotal += written;
		}
		return true;
	}

	bool FfmpegVideoWriter::Finish() {
		if (!process)
			return false;
		CloseInputPipe();
		while (WaitForSingleObject(process, 50) == WAIT_TIMEOUT)
			PumpWindowMessages();
		DWORD exitCode = 1;
		const bool readExitCode = GetExitCodeProcess(process, &exitCode) != FALSE;
		CloseProcess();
		if (!readExitCode || exitCode != 0) {
			errorMessage = std::format(L"FFmpeg가 오류 코드 {}로 종료되었습니다. 로그: {}",
				exitCode, logPath.wstring());
			return false;
		}
		completed = true;
		std::error_code logSizeError;
		if (std::filesystem::file_size(logPath, logSizeError) == 0) {
			std::error_code removeLogError;
			std::filesystem::remove(logPath, removeLogError);
		}
		return true;
	}

	void FfmpegVideoWriter::Cancel() {
		CloseInputPipe();
		if (process) {
			if (WaitForSingleObject(process, 500) == WAIT_TIMEOUT) {
				TerminateProcess(process, 1);
				WaitForSingleObject(process, 1000);
			}
			CloseProcess();
		}
		if (!completed && !outputPath.empty()) {
			std::error_code removeError;
			std::filesystem::remove(outputPath, removeError);
		}
	}
}
