# 制作 WinPE

## 一、下载并安装工具

1. 下载 **Windows ADK** 和 **ADK 的 Windows PE 加载项**。  
3. 安装完成后，在开始菜单中找到 **“部署和映像工具环境”**，**右键以管理员身份运行**。

---

## 二、生成基础 Windows PE 映像

在打开的命令提示符中依次执行：

```cmd
cd "..\Windows Preinstallation Environment\amd64"
copype amd64 C:\WinPE
MakeWinPEMedia /ISO C:\WinPE C:\WinPE\pe.iso
```

此时已在 `C:\WinPE\pe.iso` 生成可启动的官方 PE。

---

## 三、定制前的挂载操作

```cmd
dism /Mount-Image /ImageFile:"C:\WinPE\media\sources\boot.wim" /index:1 /MountDir:"C:\WinPE\mount"
```

> 若 `C:\WinPE\mount` 不存在，会自动创建该文件夹。

---

## 四、加入程序

1. 最终文件树：

```txt
C:\WinPE\mount\Windows
├─AppCompat
├─...
├─PETools                   <--------- PETools 仓库中的程序
│  └─_internal              <--------- PyInstaller 打包后的资源
│      └─...
│  msvcp140_atomic_wait.dll <--------- PEInside 依赖
│  msvcp140.dll
│  vcruntime140_1.dll
│  PEInside.exe
│  PELauncher.exe
│  PEMenu.exe
│  Screenboard.exe
│  PELauncher.txt
├─PolicyDefinitions
├─...
├─System32
│  startnet.cmd             <--------- 启动命令
├─SystemResources
├─...
└─zh-CN
```

2. **替换**`startnet.cmd`：

```cmd
@echo off
title Windows PE Maintenance Environment

echo ===============================================================================
echo                         MICROSOFT SOFTWARE LICENSE TERMS
echo ===============================================================================
echo.
echo This Windows Preinstallation Environment (WinPE) is provided for
echo MAINTENANCE, RECOVERY, AND TROUBLESHOOTING PURPOSES ONLY.
echo.
echo You may NOT:
echo   - Distribute this WinPE image (WIM, ISO, USB, or any form) as a standalone
echo     product to third parties.
echo   - Use it as a general-purpose operating system.
echo   - Remove or alter this legal notice.
echo.
echo This copy is generated from Microsoft Windows Assessment and Deployment Kit
echo (Windows ADK). You must have a valid Windows license to use it.
echo.

wpeinit

cd ../PETools

echo Launching PELauncher...
start PELauncher

echo.
echo ===============================================================================
echo Available maintenance tools (for authorized use only):
echo ===============================================================================
dir *.exe /b
echo.
echo ===============================================================================
echo To exit, close this window or type 'exit'.
echo ===============================================================================

@echo on
```
3. 编辑`PELauncher.txt`：
```txt
PEInside.exe
PEMenu.exe
```
---

## 五、添加中文语言包（可选）

```cmd
dism /Add-Package /Image:"C:\WinPE\mount" /PackagePath:"WinPE_OCs\zh-cn\lp.cab"
dism /Add-Package /Image:C:\WinPE\mount /PackagePath:"WinPE_OCs\WinPE-FontSupport-ZH-CN.cab"
dism /Set-AllIntl:zh-CN /Image:"C:\WinPE\mount"
```

> 其中 `WinPE_OCs` 路径通常在安装 ADK 后的目录中，例如 `C:\Program Files (x86)\Windows Kits\10\Assessment and Deployment Kit\Windows Preinstallation Environment\amd64\WinPE_OCs`。请根据实际位置修改 `/PackagePath`。

---

## 六、提交修改并重新生成 ISO

1. **关闭所有已打开的文件资源管理器窗口**，确保无占用。  
2. 提交挂载的映像：

```cmd
dism /Unmount-Image /MountDir:"C:\WinPE\mount" /commit
```

如果只需要.wim文件，此时已经可以在C:\WinPE\source下获取

3. 重新生成 ISO：

```cmd
MakeWinPEMedia /ISO C:\WinPE C:\WinPE\pe_new.iso
```

---

## 七、启动测试

方式：

1. 使用PEOutside进入

2. 将生成的 `pe_new.iso` 挂载到虚拟机或刻录到 U 盘启动