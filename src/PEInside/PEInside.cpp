#include "WinUtils/WinPch.h"

#include "WinUtils/INI.h"
#include "WinUtils/Console.h"
#include "HugoUtils/HInfo.h"
#include <iostream>
using namespace std;
using namespace WinUtils;
#include <Windows.h>
#include <shobjidl.h>
#include <objbase.h>
#include <optional>
#include <filesystem>
#include <WinUtils/StrConvert.h>

// Namespace alias
namespace fs = std::filesystem;
using namespace WinUtils;

// Configuration Struct
struct PEConfig
{
	bool Unfreeze = true;
	bool RenameSSA = true;
	bool CreateLink = true;
	std::string UserName;
};

// Utility: Find Real Physical System Drive
std::optional<fs::path> FindRealSystemDrive()
{
	char drives[256] = { 0 };
	if (!GetLogicalDriveStringsA(ARRAYSIZE(drives), drives))
		return std::nullopt;

	char* pDrive = drives;
	while (*pDrive)
	{
		if (GetDriveTypeA(pDrive) == DRIVE_FIXED)
		{
			fs::path drivePath = pDrive;

			// Skip WinPE RAM disk X:\
			// if (drivePath.string()[0] == 'X' || drivePath.string()[0] == 'x')
			//{
				//pDrive += strlen(pDrive) + 1;
				//continue;
			//}

			// Check if it's a real Windows system drive
			fs::path system32Path = drivePath / "ProgramData" / "SeewoFreezeKernelConfig";
			if (fs::exists(system32Path) && fs::is_directory(system32Path))
			{
				cout << "Found real system drive: " << drivePath << endl;
				return drivePath;
			}
		}
		pDrive += strlen(pDrive) + 1;
	}
	return std::nullopt;
}

// Utility: Traverse Drives to Find HugoWinPE
std::optional<fs::path> FindHugoWinPEDirectory()
{
	char drives[256] = { 0 };
	if (!GetLogicalDriveStringsA(ARRAYSIZE(drives), drives))
		return std::nullopt;

	char* pDrive = drives;
	while (*pDrive)
	{
		if (GetDriveTypeA(pDrive) == DRIVE_FIXED)
		{
			fs::path targetDir = pDrive;
			targetDir /= "HugoWinPE";
			if (fs::exists(targetDir) && fs::is_directory(targetDir))
				return targetDir;
		}
		pDrive += strlen(pDrive) + 1;
	}
	return std::nullopt;
}

// Utility: Read peconfig.ini Configuration
std::optional<PEConfig> ReadPEConfig(const fs::path& iniFilePath)
{
	if (!fs::exists(iniFilePath))
	{
		cerr << "Error: Configuration file not found -> " << iniFilePath << endl;
		return std::nullopt;
	}

	INIStructure iniData;
	INIReader reader(iniFilePath);
	if (!(reader >> iniData))
	{
		cerr << "Error: Failed to read INI file" << endl;
		return std::nullopt;
	}

	if (!iniData.has(L"Config"))
	{
		cerr << "Error: INI file missing [Config] section" << endl;
		return std::nullopt;
	}

	auto& configSection = iniData[L"Config"];
	PEConfig config;

	config.Unfreeze = configSection.get_as<bool>(L"Unfreeze", true);
	config.RenameSSA = configSection.get_as<bool>(L"RenameSSA", true);
	config.CreateLink = configSection.get_as<bool>(L"CreateLink", true);
	config.UserName = ConvertString<string>(configSection.get(L"UserName"));

	if (config.UserName.empty())
	{
		cerr << "Error: UserName configuration is empty" << endl;
		return std::nullopt;
	}

	return config;
}

// Feature 1: Unfreeze
bool ExecuteUnfreeze(const fs::path& systemDrive)
{
	fs::path targetFile = systemDrive / "ProgramData" / "SeewoFreezeKernelConfig" / "VolumeInfo.config";
	try
	{
		if (fs::exists(targetFile))
		{
			fs::remove(targetFile);
			cout << "Successfully deleted file: " << targetFile << endl;
		}
		else
		{
			cout << "File does not exist, skip deletion: " << targetFile << endl;
		}
		return true;
	}
	catch (const fs::filesystem_error& e)
	{
		cerr << "Failed to delete file: " << e.what() << endl;
		return false;
	}
}

// Feature 2: RenameSSA
bool ExecuteRenameSSA(const fs::path& systemDrive)
{
	fs::path srcFile = systemDrive / "Program Files (x86)" / "Seewo" / "SeewoService";
	fs::path destFile = systemDrive / "Program Files (x86)" / "Seewo" / "SeewoService2";
	try
	{
		if (fs::exists(srcFile))
		{
			fs::rename(srcFile, destFile);
			cout << "Successfully renamed: " << srcFile << " → " << destFile << endl;
		}
		else
		{
			cout << "Source directory does not exist, skip rename: " << srcFile << endl;
		}
		return true;
	}
	catch (const fs::filesystem_error& e)
	{
		cout << "Failed to rename directory: " << e.what() << endl;
		return false;
	}
}

// Utility: Create Shortcut
bool CreateShortcut(
	const std::string& targetExe,
	const std::string& args,
	const std::string& lnkPath
)
{
	HRESULT hr;
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		cerr << "COM initialization failed" << endl;
		return false;
	}

	IShellLinkA* pShellLink = nullptr;
	IPersistFile* pPersistFile = nullptr;

	hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID*)&pShellLink);
	if (SUCCEEDED(hr))
	{
		pShellLink->SetPath(targetExe.c_str());
		pShellLink->SetArguments(args.c_str());

		hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
		if (SUCCEEDED(hr))
		{
			std::wstring wideLnk = std::wstring(lnkPath.begin(), lnkPath.end());
			hr = pPersistFile->Save(wideLnk.c_str(), TRUE);
			pPersistFile->Release();
		}
		pShellLink->Release();
	}

	CoUninitialize();
	return SUCCEEDED(hr);
}

// Feature 3: Create Startup Shortcut
bool ExecuteCreateLink(const fs::path& hugoDir, const std::string& userName, const fs::path& systemDrive)
{
	fs::path outsideExe = hugoDir / "PEOutside.exe";
	if (!fs::exists(outsideExe))
	{
		cerr << "Error: PEOutside.exe not found -> " << outsideExe << endl;
		return false;
	}

	fs::path startupDir = systemDrive / "Users" / userName / "AppData" / "Roaming" / "Microsoft" / "Windows" / "Start Menu" / "Programs" / "Startup";
	if (!fs::exists(startupDir))
	{
		cout << "Error: User directory does not exist, check UserName configuration" << endl;
		return false;
	}

	fs::path shortcutPath = startupDir / "PEOutside_Cleanup.lnk";
	bool ret = CreateShortcut(outsideExe.string(), "-cleanup", shortcutPath.string());

	if (ret)
		cout << "Successfully created shortcut: " << shortcutPath << endl;
	else
		cerr << "Failed to create shortcut" << endl;

	return ret;
}

// Main Function
int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
)
{
	Console c;
	c.alloc();

	cout << "===== HugoWinPE PEInside =====\n" << endl;

	// Find real system drive
	auto systemDrive = FindRealSystemDrive();
	if (!systemDrive)
	{
		cerr << "Error: Real Windows system drive not found! " << endl;
		cerr << "Tip: You may not have installed the Seewo component." << endl;
		system("pause");
		return 1;
	}
	cout << "Current system drive: " << *systemDrive << "\n" << endl;

	// Find HugoWinPE directory
	auto hugoDir = FindHugoWinPEDirectory();
	if (!hugoDir)
	{
		cerr << "Error: HugoWinPE folder not found on any drive" << endl;
		system("pause");
		return 1;
	}
	cout << "Found configuration directory: " << *hugoDir << "\n" << endl;

	// Read INI config
	fs::path iniPath = *hugoDir / "peconfig.ini";
	auto config = ReadPEConfig(iniPath);
	if (!config)
	{
		system("pause");
		return 1;
	}

	cout << "Configuration loaded successfully:\n"
		<< "  Unfreeze: " << (config->Unfreeze ? "true" : "false") << "\n"
		<< "  RenameSSA: " << (config->RenameSSA ? "true" : "false") << "\n"
		<< "  CreateLink: " << (config->CreateLink ? "true" : "false") << "\n"
		<< "  UserName: " << config->UserName << "\n" << endl;

	// Execute features
	cout << "===== Starting Feature Execution =====\n" << endl;

	if (config->Unfreeze)
	{
		cout << "[1/3] Executing Unfreeze feature..." << endl;
		ExecuteUnfreeze(*systemDrive);
	}
	else
		cout << "[1/3] Unfreeze disabled, skipping\n" << endl;

	if (config->RenameSSA)
	{
		cout << "[2/3] Executing RenameSSA feature..." << endl;
		ExecuteRenameSSA(*systemDrive);
	}
	else
		cout << "[2/3] RenameSSA disabled, skipping\n" << endl;

	if (config->CreateLink)
	{
		cout << "[3/3] Executing CreateLink feature..." << endl;
		ExecuteCreateLink(*hugoDir, config->UserName, *systemDrive);
	}
	else
		cout << "[3/3] CreateLink disabled, skipping\n" << endl;

	cout << "\n===== All Features Executed Successfully =====" << endl;
	system("pause");
	return 0;
}