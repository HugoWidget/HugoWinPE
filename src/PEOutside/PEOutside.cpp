#define _CRT_SECURE_NO_WARNINGS
#include "WinUtils/WinUtils.h"
#include "WinUtils/ini.h"
#include "WinUtils/StrConvert.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/Console.h"
#include <Windows.h>
#include <Winbase.h>
#include <iostream>
#include <string>
#include <optional>
#include <filesystem>
#include <lmcons.h>
#include <shellapi.h>
#include <cstdlib>
#include <cctype>

namespace fs = std::filesystem;
using namespace WinUtils;
using namespace std;

// 常量定义
constexpr wchar_t PE_DIR_NAME[] = L"HugoWinPE";
constexpr wchar_t PE_DIR_NAME2[] = L":\\HugoWinPE\\";
constexpr wchar_t PE_BOOT_ENTRY_NAME[] = L"Ramdisk(HugoWinPE)";
constexpr char PE_BOOT_ENTRY_NAME2[] = "Ramdisk(HugoWinPE)";
constexpr wchar_t BACKUP_FILE_NAME[] = L"BCD_Backup";
constexpr wchar_t CONFIG_FILE_NAME[] = L"peconfig.ini";
constexpr wchar_t BOOT_WIM[] = L"boot.wim";
constexpr wchar_t BOOT_SDI[] = L"boot.sdi";
constexpr wchar_t RAMDISK_OPTIONS_ID[] = L"{ramdiskoptions}";

// 执行命令
void ExecCmd(const wstring& cmd) {
	wcout << L"[执行] " << cmd << L'\n';
	if (int ret = _wsystem(cmd.c_str()); ret != 0) {
		wcerr << L"[警告] 命令返回值: " << ret << L'\n';
	}
	wcout << L"----------------------------------------\n";
}

string ExecCmdAndCaptureOutput(const wstring& cmd) {
	string result;
	FILE* pipe = _wpopen(cmd.c_str(), L"r");
	if (!pipe) {
		cerr << "[错误] 无法执行命令\n";
		return "";
	}
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}
	_pclose(pipe);
	return result;
}

// 字符串工具
string ExtractUUID(const string& output) {
	size_t start = output.find('{');
	size_t end = output.find('}');
	return (start == string::npos || end == string::npos) ? "" : output.substr(start, end - start + 1);
}

// 目录/文件操作
void CreateDir(const fs::path& path) {
	if (!fs::exists(path)) {
		fs::create_directories(path);
		wcout << L"[创建] 目录: " << path.wstring() << L'\n';
	}
}

bool CopyFileIfExists(const fs::path& src, const fs::path& dst, bool overwrite = false) {
	if (!fs::exists(src)) {
		wcerr << L"[错误] 源文件不存在: " << src.wstring() << L'\n';
		return false;
	}
	if (fs::exists(dst) && !overwrite) {
		wcerr << L"[错误] 目标文件已存在且未允许覆盖: " << dst.wstring() << L'\n';
		return false;
	}
	if (!CopyFileW(src.c_str(), dst.c_str(), overwrite ? FALSE : TRUE)) {
		wcerr << L"[错误] 复制文件失败: " << src.wstring() << L" -> " << dst.wstring() << L'\n';
		return false;
	}
	wcout << L"[复制] " << src.filename().wstring() << L" -> " << dst.wstring() << L'\n';
	return true;
}

bool CopyDirectoryContents(const fs::path& srcDir, const fs::path& dstDir) {
	bool success = true;
	for (const auto& entry : fs::directory_iterator(srcDir)) {
		if (entry.is_regular_file()) {
			fs::path dest = dstDir / entry.path().filename();
			if (!CopyFileIfExists(entry.path(), dest, true)) {
				success = false;
			}
		}
	}
	return success;
}

// 驱动器验证
bool ValidateDrive(wchar_t c) {
	c = towupper(c);
	return c >= L'C' && c <= L'Z';
}

// 查找 HugoWinPE 目录
optional<fs::path> FindHugoWinPEDirectory() {
	WCHAR drives[256] = {};
	if (!GetLogicalDriveStringsW(ARRAYSIZE(drives), drives)) {
		return nullopt;
	}
	for (WCHAR* pDrive = drives; *pDrive; pDrive += wcslen(pDrive) + 1) {
		if (GetDriveTypeW(pDrive) == DRIVE_FIXED) {
			fs::path targetDir = fs::path(pDrive) / PE_DIR_NAME;
			if (fs::exists(targetDir) && fs::is_directory(targetDir)) {
				return targetDir;
			}
		}
	}
	return nullopt;
}

// BCD 备份与恢复
void BackupBCD(const fs::path& backupPath) {
	wcout << L"[备份] 系统BCD -> " << backupPath.wstring() << L'\n';
	ExecCmd(L"bcdedit /export \"" + backupPath.wstring() + L'"');
}

bool BackupBCDWithOverwritePrompt(const fs::path& backupPath) {
	if (fs::exists(backupPath)) {
		wcout << L"备份文件已存在，是否覆盖？(Y/N): ";
		wchar_t answer;
		wcin >> answer;
		if (towupper(answer) != L'Y') {
			wcout << L"取消备份。\n";
			return false;
		}
	}
	BackupBCD(backupPath);
	return true;
}

bool RestoreBCD(const fs::path& backupPath) {
	if (!fs::exists(backupPath)) {
		wcerr << L"[错误] BCD备份文件不存在！\n";
		return false;
	}
	wcout << L"[恢复] 还原系统BCD...\n";
	ExecCmd(L"bcdedit /import \"" + backupPath.wstring() + L'"');
	wcout << L"[成功] BCD已恢复原始状态！\n";
	return true;
}

// 生成 peconfig.ini
void GeneratePEConfig(const fs::path& iniPath) {
	wcout << L"\n===== PE 配置生成 =====\n";
	wchar_t c_unfreeze, c_rename, c_link;
	wcout << L"1.解除冰点，操作不可还原 (Y/N): "; wcin >> c_unfreeze;
	wcout << L"2.禁用希沃管家以取消锁屏 (Y/N): "; wcin >> c_rename;
	wcout << L"3.启用清理（开启后自动还原希沃管家） (Y/N): "; wcin >> c_link;

	auto to_bool = [](wchar_t c) {
		return towupper(c) == L'Y' ? L"true" : L"false";
		};

	INIStructure iniData;
	iniData[L"Config"][L"Unfreeze"] = to_bool(c_unfreeze);
	iniData[L"Config"][L"RenameSSA"] = to_bool(c_rename);
	iniData[L"Config"][L"CreateLink"] = to_bool(c_link);
	iniData[L"Config"][L"UserName"] = ConvertString(GetCurrentUserName());

	INIFile iniFile(iniPath);
	iniFile.generate(iniData, true);
	wcout << L"[成功] 生成配置文件: " << iniPath.wstring() << L'\n';
}

// 检查启动项状态
bool CheckRamdiskOptionsExists() {
	string output = ExecCmdAndCaptureOutput(L"bcdedit /enum " + wstring(RAMDISK_OPTIONS_ID));
	return output.find("标识符") != string::npos || output.find("Identifier") != string::npos;
}

bool IsPEBootRegistered() {
	string output = ExecCmdAndCaptureOutput(L"bcdedit /enum");
	return output.find(PE_BOOT_ENTRY_NAME2) != string::npos;
}

wstring GetPEBootEntryUUID() {
	if (!IsPEBootRegistered()) return L"";
	string output = ExecCmdAndCaptureOutput(L"bcdedit /enum");
	size_t pos = output.find(PE_BOOT_ENTRY_NAME2);
	if (pos == string::npos) return L"";
	return ConvertString(ExtractUUID(output.substr(pos)));
}

// 创建 PE 启动项
bool CreatePEBoot(const fs::path& peDir, wstring& outUUID) {
	fs::path path_sdi = peDir / BOOT_SDI;
	fs::path path_wim = peDir / BOOT_WIM;

	if (!fs::exists(path_sdi) || !fs::exists(path_sdi)) {
		wcerr << L"[错误] 程序目录缺少 PE 文件（boot.sdi 或 boot.wim）！\n";
		return false;
	}

	// 获取盘符和相对路径
	wstring driveRoot = peDir.root_name().wstring();   // 如 "D:"
	wstring peRelPath = L'\\' + wstring(PE_DIR_NAME) + L'\\' + BOOT_WIM;
	wstring sdiRelPath = L'\\' + wstring(PE_DIR_NAME) + L'\\' + BOOT_SDI;

	// 配置 ramdiskoptions
	if (!CheckRamdiskOptionsExists()) {
		ExecCmd(L"bcdedit /create " + wstring(RAMDISK_OPTIONS_ID) + L" /d \"HugoWinPE\"");
	}
	ExecCmd(L"bcdedit /set " + wstring(RAMDISK_OPTIONS_ID) + L" ramdisksdidevice partition=" + driveRoot);
	ExecCmd(L"bcdedit /set " + wstring(RAMDISK_OPTIONS_ID) + L" ramdisksdipath " + sdiRelPath);

	// 创建启动项
	string createOutput = ExecCmdAndCaptureOutput(
		L"bcdedit /create /d \"" + wstring(PE_BOOT_ENTRY_NAME) + L"\" /application osloader"
	);
	wstring uuid = ConvertString(ExtractUUID(createOutput));
	if (uuid.empty()) {
		wcerr << L"[错误] 创建启动项失败！\n";
		return false;
	}
	wcout << L"[成功] 启动项UUID: " << uuid << L'\n';

	wstring devicePath = L"ramdisk=[" + driveRoot + L']' + peRelPath + L',' + RAMDISK_OPTIONS_ID;
	ExecCmd(L"bcdedit /set " + uuid + L" device " + devicePath);
	ExecCmd(L"bcdedit /set " + uuid + L" osdevice " + devicePath);
	ExecCmd(L"bcdedit /set " + uuid + L" highestmode true");

	FIRMWARE_TYPE ft = {};
	GetFirmwareType(&ft);
	wstring loaderPath = (ft == FirmwareTypeUefi) ? L"\\windows\\system32\\winload.efi" : L"\\windows\\system32\\winload.exe";
	ExecCmd(L"bcdedit /set " + uuid + L" path " + loaderPath);

	ExecCmd(L"bcdedit /set " + uuid + L" systemroot \\windows");
	ExecCmd(L"bcdedit /set " + uuid + L" winpe yes");
	ExecCmd(L"bcdedit /set " + uuid + L" nx optin");
	ExecCmd(L"bcdedit /set " + uuid + L" detecthal yes");
	ExecCmd(L"bcdedit /displayorder " + uuid + L" /addlast");
	ExecCmd(L"bcdedit /timeout 5");

	outUUID = uuid;
	return true;
}

// 设置启动顺序并重启
void SetBootSequenceToPEAndRestart(const wstring& uuid) {
	if (uuid.empty()) {
		wcerr << L"[错误] UUID 为空，无法设置启动顺序。\n";
		return;
	}
	ExecCmd(L"bcdedit /timeout 0");
	ExecCmd(L"bcdedit /bootsequence " + uuid);
	system("shutdown /r /t 0");
}

void ManualBootToPE() {
	if (!IsPEBootRegistered()) {
		wcerr << L"[错误] HugoWinPE 尚未注册，请先执行注册。\n";
		return;
	}
	ExecCmd(L"bcdedit /timeout 5");
	ExecCmd(L"shutdown /r /t 0");
}

// 查询 BCD 状态
void QueryBCDStatus() {
	string output = ExecCmdAndCaptureOutput(L"bcdedit /enum");
	wcout << L"\n===== BCD 启动项列表 =====\n";
	cout << output << endl;
	wcout << L"[状态] HugoWinPE 启动项：" << (IsPEBootRegistered() ? L"已注册" : L"未注册") << L'\n';
}

// 首次安装：复制自身及依赖到目标盘
bool FirstTimeInstall() {
	fs::path currentDir = GetCurrentProcessDir();
	fs::path src_sdi = currentDir / BOOT_SDI;
	fs::path src_wim = currentDir / BOOT_WIM;
	if (!fs::exists(src_sdi) || !fs::exists(src_wim)) {
		wcerr << L"[错误] 当前目录缺少 PE 核心文件（boot.sdi 或 boot.wim）！\n";
		wcerr << L"请将本程序与 boot.sdi、boot.wim 放在同一目录下再运行。\n";
		system("pause");
		return false;
	}
	wcout << L"检测到尚未安装 HugoWinPE。\n";
	wcout << L"请输入要创建的盘符（如 D/E/F，不能是 C，不能是冰冻盘）: ";
	wchar_t drive;
	wcin >> drive;
	drive = towupper(drive);
	if (!ValidateDrive(drive) || drive == L'C') {
		wcerr << L"[错误] 无效盘符或为 C 盘！\n";
		return false;
	}
	fs::path targetDir = fs::path(wstring(1, drive) + L":\\") / PE_DIR_NAME;
	if (fs::exists(targetDir)) {
		wcerr << L"[错误] 目录已存在，请先删除！\n";
		return false;
	}
	CreateDir(targetDir);

	fs::path srcDir = GetCurrentProcessDir();
	if (!CopyDirectoryContents(srcDir, targetDir)) {
		wcerr << L"[错误] 复制文件失败。\n";
		return false;
	}

	// 重新启动副本
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	fs::path currentExe(exePath);
	fs::path newExePath = targetDir / currentExe.filename();

	wcout << L"[完成] 文件已复制到 " << targetDir.wstring() << L'\n';
	wcout << L"正在重新启动程序...\n";
	ShellExecuteW(nullptr, L"open", newExePath.c_str(), nullptr, targetDir.c_str(), SW_SHOW);
	return true;
}

// 清理功能（命令行 /cleanup）==========
void PerformCleanup(const fs::path& peDir) {
	RestoreBCD(peDir / BACKUP_FILE_NAME);

	wstring startupLink = L"C:\\Users\\" + GetCurrentUserName() +
		L"\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\PEOutside_Cleanup.lnk";
	if (fs::exists(startupLink)) {
		fs::remove(startupLink);
	}

	const wstring serviceDir = L"C:\\Program Files (x86)\\Seewo\\SeewoService2";
	if (fs::exists(serviceDir)) {
		fs::rename(serviceDir, L"C:\\Program Files (x86)\\Seewo\\SeewoService");
	}
}

// 交互菜单
enum class MenuAction { Exit = 0, AutoRun = 1, Config, Query, RegisterPE, ManualBoot, Backup, Restore };
void ShowMenu() {
	wcout << L" ===== HugoWinPE PEOutside  =====\n";
	wcout << L"1. 自动化           - 立即重启，自动进入\n";
	wcout << L"2. 配置             - 配置WinPE行为\n";
	wcout << L"3. 查询             - 查询注册状态\n";
	wcout << L"4. 注册             - 注册WinPE\n";
	wcout << L"5. 手动执行         - 重启，手动进入\n";
	wcout << L"6. 备份             - 备份 BCD\n";
	wcout << L"7. 还原             - 还原 BCD\n";
	wcout << L"0. 退出\n";
	wcout << L"请选择操作: ";
}

bool RunMenuLoop(const fs::path& peDir) {
	int choice = -1;
	while (choice != 0) {
		ShowMenu();
		if (!(wcin >> choice)) {
			wcin.clear();
			wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');
			wcout << L"输入无效，请输入数字。\n";
			continue;
		}

		switch (static_cast<MenuAction>(choice)) {
		case MenuAction::AutoRun: {
			if (IsPEBootRegistered()) {
				wcerr << L"[错误] HugoWinPE 启动项已存在，无法重复注册！\n";
				break;
			}
			fs::path configPath = peDir / CONFIG_FILE_NAME;
			fs::path backupPath = peDir / BACKUP_FILE_NAME;
			if (!fs::exists(configPath)) {
				wcerr << L"[错误] 配置文件不存在，请先执行配置。\n";
				break;
			}
			if (!fs::exists(backupPath)) {
				wcerr << L"[错误] BCD备份文件不存在，请先执行备份。\n";
				break;
			}
			wstring uuid;
			if (CreatePEBoot(peDir, uuid)) {
				SetBootSequenceToPEAndRestart(uuid);
			}
			else {
				wcout << L"注册失败\n";
			}
			break;
		}
		case MenuAction::Config: {
			fs::path configPath = peDir / CONFIG_FILE_NAME;
			if (fs::exists(configPath)) {
				wchar_t answer;
				wcout << L"配置文件已存在，是否覆盖重新生成？(Y/N): ";
				wcin >> answer;
				if (towupper(answer) != L'Y') {
					wcout << L"取消配置。\n";
					break;
				}
			}
			GeneratePEConfig(configPath);
			break;
		}
		case MenuAction::Query:
			QueryBCDStatus();
			break;
		case MenuAction::RegisterPE: {
			if (IsPEBootRegistered()) {
				wcerr << L"[错误] HugoWinPE 启动项已存在，无法重复注册！\n";
				break;
			}
			fs::path configPath = peDir / CONFIG_FILE_NAME;
			fs::path backupPath = peDir / BACKUP_FILE_NAME;
			if (!fs::exists(configPath)) {
				wcerr << L"[错误] 配置文件不存在，请先执行配置。\n";
				break;
			}
			if (!fs::exists(backupPath)) {
				wcerr << L"[错误] BCD备份文件不存在，请先执行备份。\n";
				break;
			}
			wstring uuid;
			if (CreatePEBoot(peDir, uuid)) {
				wcout << L"注册成功\n";
			}
			else {
				wcout << L"注册失败\n";
			}
			break;
		}
		case MenuAction::ManualBoot:
			ManualBootToPE();
			break;
		case MenuAction::Backup:
			BackupBCDWithOverwritePrompt(peDir / BACKUP_FILE_NAME);
			break;
		case MenuAction::Restore:
			RestoreBCD(peDir / BACKUP_FILE_NAME);
			break;
		case MenuAction::Exit:
			wcout << L"退出程序。\n";
			break;
		default:
			wcout << L"无效选项，请重新输入。\n";
			continue;
		}
		if (choice != 0) {
			wcout << L"\n按任意键继续...";
			system("pause > nul");
		}
		system("cls");
	}
	return true;
}

// 主函数
int wmain(int argc, wchar_t* argv[]) {
	RequireAdminPrivilege(true);
	Console console;
	console.setLocale();

	CmdParser parser;
	if (!parser.parse(ExtractArguments(GetCommandLine()))) return 0;

	fs::path currentExeDir = GetCurrentProcessDir();
	bool isInsidePEDir = (currentExeDir.wstring().substr(1) == PE_DIR_NAME2);

	optional<fs::path> existingPEDir = FindHugoWinPEDirectory();

	// 确定最终使用的 PE 目录
	fs::path peDir;
	if (isInsidePEDir) {
		peDir = currentExeDir;
	}
	else if (existingPEDir.has_value()) {
		peDir = *existingPEDir;
		wcout << L"[找到] PE 目录: " << peDir.wstring() << L'\n';
	}

	// 命令行 custom 处理（用于启动 Launcher.exe 的副本）
	if (parser.hasCommand(L"custom")) {
		fs::path launchPath = GetCurrentProcessFSDir() / L"Launcher.exe";
		if (fs::exists(launchPath)) {
			RunExternalProgram(launchPath.wstring(), L"open", L"", GetCurrentProcessFSDir().wstring(), SW_SHOW);
		}
	}

	// 命令行 launch 处理
	if (parser.hasCommand(L"launch")) {
		if (existingPEDir.has_value()) {
			if ((int)RunExternalProgram((peDir / L"PEOutside.exe").wstring(), L"runas") <= 32) {
				wcerr << L"[错误] 无法启动 PEOutside.exe！\n";
				system("pause");
			};
		}
		else {
			wcerr << L"[错误] 未找到 PE 目录，无法启动！\n";
			system("pause");
		}
		return 0;
	}

	// 命令行 cleanup 处理
	if (parser.hasCommand(L"cleanup")) {
		PerformCleanup(peDir);
		return 0;
	}

	// 检查 boot.wim / boot.sdi 的有效性
	fs::path sdiPath = GetCurrentProcessDir();
	sdiPath = sdiPath / BOOT_SDI;
	fs::path wimPath = GetCurrentProcessDir();
	wimPath = wimPath / BOOT_WIM;
	if (!fs::exists(sdiPath) || !fs::exists(wimPath)) {
		wcerr << L"[错误] boot 文件（boot.sdi 或 boot.wim）不存在\n";
		system("pause");
		return 1;
	}
	if (fs::file_size(wimPath) <= 1024) {
		wcerr << L"[错误] 请使用合法的 boot.wim 文件\n";
		system("pause");
		return 1;
	}

	// 如果是首次运行且不在 PE 目录内，则执行安装
	if (!isInsidePEDir && !existingPEDir.has_value()) {
		if (FirstTimeInstall()) {
			return 0;
		}
		else {
			wcerr << L"[错误] 首次安装失败！\n";
			system("pause");
			return 1;
		}
	}

	// 如果程序不在 PE 目录内，但有已存在的 PE 目录，则启动其中的副本
	if (!isInsidePEDir && existingPEDir.has_value()) {
		if ((int)RunExternalProgram((peDir / L"PEOutside.exe").wstring(), L"runas") <= 32) {
			wcerr << L"[错误] 无法启动 PEOutside.exe！\n";
			system("pause");
		};
		return 0;
	}

	// 进入交互菜单
	RunMenuLoop(peDir);
	return 0;
}