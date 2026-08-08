# 使用其他 WinPE

没有键盘，也不想自己做：

1. 找到`Winre.wim`：

   首先执行在`Powershell`执行`reagentic /info`（可以跳过这一步）

   打开`DiskGenius.exe`或使用其他方法将不可见的分区中的`Winre.wim`复制出来

2. 改名为`boot.wim`

3. 在非冰冻盘根目录创建`HugoWinPE`文件夹，示例中为E盘

4. 文件树如下：

   ```
   E:\HUGOWINPE
   │  boot.sdi
   |  boot.wim
   │  Launcher.exe
   │  Launcher.ini <-这是在回到主系统时额外启动程序的配置文件
   │  msvcp140.dll
   │  msvcp140_atomic_wait.dll
   │  PEInside.exe
   │  PELauncher.exe
   │  PELauncher.txt
   │  PEMenu.exe
   │  PEOutside.exe
   │  Screenboard.exe <-屏幕键盘，需触摸屏在WinPE下可用（一般可以）
   │  vcruntime140_1.dll
   │
   └─_internal <-一定要保证文件夹存在
   ```

直接在 HugoSetup 仓库下载即可

5. 打开`E:\HugoWinPE\PEOutside.exe`
6. 按 PEOutside.md 中的方法配置

7. 进入WinRE后，找到 命令提示符

8. 挨着尝试 `cd /d 盘符:\HugoWinPE`，直到找到为止，一般为原盘符

9. 打开`PELauncher.exe`，会启动`PEInside.exe`（不可以直接启动）

10. 操作结束后使用 `wpeutil reboot` 或 已经运行的`PEMenu` 重启

如果键盘或者有其他WinPE就不需要使用这种方法了，直接操作说不定会快一些......