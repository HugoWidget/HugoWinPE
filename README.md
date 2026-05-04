# HugoWinPE 维护工具集

本项目提供了一套用于**希沃教学设备**的 Windows PE 维护方案。通过自动配置 BCD 启动项，在系统重启后进入 WinPE 环境，自动执行解除冰点还原、禁用希沃管家、创建清理任务等操作，便于 IT 运维人员进行系统维护。

## 功能概览

- [**PEOutside**](docs/PEOutside.md)：主控程序，运行在正常 Windows 系统中。负责首次部署、BCD 启动项管理、配置生成、备份/恢复、触发重启进入 PE。
- [**PEInside**](docs/PEInside.md)：PE 内执行程序，随 WinPE 启动后自动运行。根据预先生成的配置文件执行实际的维护操作。

## 编译依赖

- Visual Studio 2022
- Windows SDK

## 快速开始

1. 将 `PEOutside.exe`、`PEInside.exe`、`boot.wim`、`boot.sdi` 放置于同一目录。
2. **以管理员身份运行 `PEOutside.exe`**。
3. 首次运行会提示选择目标盘符（不能为 C 盘），程序将自动复制自身及 PE 文件并重新启动。
4. 在主菜单中选择：
   - **配置** → 设置 PE 启动后要执行的动作（解除冰点、重命名希沃服务、创建清理任务）
   - **备份** → 备份当前系统 BCD
   - **注册** → 创建 Ramdisk OSLoader 启动项
   - **自动化** → 立即重启并自动进入 PE 执行任务

## 注意事项

- 本工具需要**管理员权限**运行。
- PE 启动后会修改系统文件（解除冰点、禁用服务），请提前做好数据备份。
- 清理任务会在下次正常启动时恢复 BCD 和希沃服务，并将 `PEOutside -cleanup` 从启动项移除。
- boot.wim 不在软件包中给出，但可以[自行制作](docs/make-wim.md)

## 项目依赖

[HugoUtils](https://github.com/HugoWidget/HugoUtils)(Hugo系列核心库)

## 许可证

本项目采用 GPLv3 许可证，详情参见 [LICENSE](LICENSE) 文件。

HugoUtils: [LGPLv3 许可证](licenses/LICENSE.LESSER-HugoUtils)

WinUtils:  [MIT 许可证](licenses/LICENSE-WinUtils)

hash-library: [zlib 许可证](licenses/LICENSE-hash-library)

swhelper：[MIT 许可证](licenses/LICENSE-swhelper)

cpp-httplib: [MIT 许可证](licenses/LICENSE-cpp-httplib)

mINI: [MIT 许可证](licenses/LICENSE-mINI)

WinReg: [MIT 许可证](licenses/LICENSE-WinReg)
