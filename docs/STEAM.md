# Steamworks 接入与发布

## 本地 SDK 接入

Steamworks SDK 不提交到公开仓库。下载 SDK 后，通过 MSBuild 属性传入根目录：

```powershell
MSBuild.exe SPlite.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:SteamworksSdkDir="D:\SDK\Steamworks"
```

设置该属性后，工程会定义 `SPLITE_ENABLE_STEAM`、链接 `steam_api64.lib`，
并把 `steam_api64.dll` 复制到输出目录。未设置时使用空实现，其他贡献者仍可构建。

本地脱离 Steam 客户端启动目录时，可临时在 exe 同目录创建 `steam_appid.txt`。
它只写真实 App ID；不要提交，也不要放进 Steam depot。

## 打包

```powershell
.\scripts\package-release.ps1 -SteamworksSdkDir "D:\SDK\Steamworks"
```

脚本会重新编译 Release、运行核心测试，并生成 `dist/SPlite`。SteamPipe 模板位于
`tools/steam`，复制 `.example` 文件后替换真实 App ID 和 Depot ID。首次上传保持
`Preview=1`，验证文件映射后再上传，并先发布到有密码的测试分支。
