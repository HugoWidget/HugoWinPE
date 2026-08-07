# PEOutside

PEOutside 是主控程序，运行于正常的 Windows 系统中，用于管理 HugoWinPE 的完整生命周期：首次部署、BCD 启动项配置、配置文件生成、备份/恢复、触发重启进入 PE，以及清理恢复。

## 使用方法

### 1. 首次部署

将以下文件放在同一目录：
- `PEOutside.exe`
- `PEInside.exe`（可选，但 PE 运行需要）
- `boot.sdi`
- `boot.wim`（有效的 Windows PE 映像）

**以管理员身份运行** `PEOutside.exe`。  
程序检测到未安装 HugoWinPE 时，会询问目标盘符（如 D、E、F，不能为 C 盘）。

确认后会自动安装到目标位置并启动。

### 2. 交互菜单

程序成功在 `HugoWinPE` 目录中运行后，显示主菜单：

```
 ===== HugoWinPE PEOutside  =====
1. 自动化           - 立即重启，自动进入
2. 配置             - 配置WinPE行为
3. 查询             - 查询注册状态
4. 注册             - 注册WinPE
5. 手动执行         - 重启，手动进入
6. 备份             - 备份 BCD
7. 还原             - 还原 BCD
0. 退出
```

**推荐操作流程**：
1. 选择 `2` → 配置所需功能
2. 选择 `6` → 备份当前 BCD
3. 选择 `1` → 立即重启自动维护

如果选择 4 + 5，请在启动界面点击 HugoWinPE 启动项

### 3. 清理恢复

在正常 Windows 启动后，若启动目录中存在 `PEOutside_Cleanup.lnk`，系统会自动执行：
```cmd
PEOutside.exe -cleanup -custom
```
该命令将：
- 从备份文件恢复 BCD

- 如果存在 `SeewoService2` 目录，将其重命名回 `SeewoService`

- 删除启动目录中的快捷方式

- 执行同目录下的`Launcher.exe`，你可以修改`Launcher.ini`实现自定义操作

  [文件格式见此](https://github.com/howdy213/WinTools)

也可手动以管理员身份运行：
```cmd
PEOutside.exe /cleanup
```

### 4. 其他命令行选项

| 参数       | 作用                                                         |
| ---------- | ------------------------------------------------------------ |
| `/launch`  | 自动搜索系统盘中的 `HugoWinPE\PEOutside.exe` 并以管理员权限启动 |
| `/cleanup` | 执行清理恢复操作                                             |
| `/custom`  | 当在清理模式下附带此选项时，会执行`Launcher.exe`             |
| 无参数     | 正常模式，显示交互菜单                                       |

## 配置文件说明

`peconfig.ini` 由 PEOutside 的“配置”功能生成，存储于 `HugoWinPE` 目录。生成时用户需依次回答三个问题：

1. **解除冰点，操作不可还原 (Y/N)**  
   对应 `Unfreeze = true/false`
2. **禁用希沃管家以取消锁屏 (Y/N)**  
   对应 `RenameSSA = true/false`
3. **启用清理（开启后自动还原希沃管家） (Y/N)**  
   对应 `CreateLink = true/false`

一般 2 和 3 会同时勾选，否则需手动将希沃管家目录名还原

生成的配置示例：
```ini
[Config]
Unfreeze = true
RenameSSA = true
CreateLink = true
UserName = Administrator
```

## 依赖文件与目录结构

- **PEOutside.exe** 主程序
- **boot.sdi**   Ramdisk 启动所需的初始系统映像
- **boot.wim**   Windows PE 映像（需包含 PEInside.exe 等工具）
- **peconfig.ini**  配置文件（由 PEOutside 生成）
- **BCD_Backup**   BCD 备份文件（由“备份”功能创建）

程序运行期间会在 `HugoWinPE` 目录中读写上述文件。

## 注意事项

1. **管理员权限**  
   所有涉及 BCD 编辑、文件复制到系统盘和重启的操作均需要管理员权限。程序启动时会自动请求提权
2. **防误操作**  
   - 不允许在 C 盘安装 HugoWinPE，程序不检查目标是否为已经冰冻的磁盘，请手动保证。  
   - 若目标盘已存在 `HugoWinPE` 目录，首次安装会报错，需手动删除。  
   - 重复注册 PE 启动项会被阻止，必须先卸载（通过 BCD 编辑手动删除）才能重新注册。
3. **BCD 备份重要性**  
   强烈建议在注册 PE 前执行备份。清理功能依赖备份文件恢复原始启动配置。
4. **`boot.wim` 校验**  
   程序会检查 `boot.wim` 文件大小是否大于 1024 字节，防止因占位文件导致的启动失败。
5. **UEFI / Legacy 兼容**  
   `CreatePEBoot` 函数会根据固件类型动选择 `winload.efi`（UEFI）或 `winload.exe`（Legacy）。

## 许可证

GPLv3，详见项目主目录下的 LICENSE 文件。