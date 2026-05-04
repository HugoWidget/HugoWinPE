# PEInside

PEInside 是运行于 WinPE 环境中的核心执行程序，负责根据预先生成的配置文件自动执行系统维护操作，如解除冰点还原、禁用希沃管家、以及创建清理任务等。

## 使用方法

完全自动化，无需操作

## 命令行参数

PEInside 无需额外命令行参数，所有行为均由 `peconfig.ini` 控制。

## 配置文件说明

`peconfig.ini` 必须位于 `HugoWinPE` 目录，编码为 UTF-8（带 BOM 或无 BOM 均可），格式如下：

```ini
[Config]
Unfreeze = true          ; true/false，是否解除冰点
RenameSSA = true         ; true/false，是否重命名希沃服务目录
CreateLink = true        ; true/false，是否创建清理快捷方式
UserName = Administrator ; 当前登录的用户名（用于定位启动文件夹）
```

- `UserName` 必须为正确的本地用户名，否则无法创建清理任务快捷方式。  
- 若某项功能为 `false`，则对应步骤将被跳过。

## 依赖文件

- `PEInside.exe` 自身  
- `msvcp140.dll`、`vcruntime140_1.dll`、`vcruntime140.dll` 
- `HugoWinPE\peconfig.ini` 配置文件（由 PEOutside 生成）  

## 注意事项

1. **系统盘识别逻辑**  
   程序通过检查 `ProgramData\SeewoFreezeKernelConfig` 是否存在来确定真实的 Windows 系统盘。因此本工具仅适用于安装了希沃冰点组件的系统。
2. **重命名操作的可逆性**  
   `RenameSSA` 仅将服务目录重命名为 `SeewoService2`，不清除数据。清理任务（PEOutside -cleanup）会将其改回 `SeewoService`。
3. **清理任务的触发时机**  
   清理任务通过启动项中的 `PEOutside -cleanup` 在正常 Windows 启动后执行，还原 BCD 和服务目录，并删除自身快捷方式。

## 运行示例

```
===== HugoWinPE PEInside =====

Current system drive: C:\

Found configuration directory: D:\HugoWinPE

Configuration loaded successfully:
  Unfreeze: true
  RenameSSA: true
  CreateLink: true
  UserName: Administrator

===== Starting Feature Execution =====

[1/3] Executing Unfreeze feature...
Successfully deleted file: C:\ProgramData\SeewoFreezeKernelConfig\VolumeInfo.config

[2/3] Executing RenameSSA feature...
Successfully renamed: C:\Program Files (x86)\Seewo\SeewoService → C:\Program Files (x86)\Seewo\SeewoService2

[3/3] Executing CreateLink feature...
Successfully created shortcut: C:\Users\Administrator\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\PEOutside_Cleanup.lnk

===== All Features Executed Successfully =====
```
