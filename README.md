# SPlite

一个面向 Windows 的商业级桌面宠物（Pet-on-Desktop）项目。

## 技术栈

- C++20
- Win32 / DirectX
- Spine Runtime（计划集成）
- Steamworks（计划集成）

## 当前状态

项目已完成可运行原型：透明宿主窗口能够统一更新和绘制多个可独立拖动的角色。

已实现：

- Win32 桌面应用骨架与无阻塞消息循环
- D3D11 渲染管线（设备、交换链、顶点/像素着色器、纹理、alpha 混合）
- 通过 WIC 加载带 alpha 通道的 PNG 精灵
- HLSL 着色器从独立文件加载，便于后续扩展
- DirectComposition 逐像素透明合成
- 无边框置顶窗口、透明像素点击穿透、实体区域拖动
- 右键菜单切换置顶状态或退出程序
- `Ctrl + Shift + F12` 可在任何时候紧急退出桌宠
- 统一动画接口与待机、交互、拖动、睡眠状态
- 单设备多角色调度和共享纹理
- 系统托盘、单实例运行、设置持久化和开机启动开关
- 独立核心测试、Windows CI 和可复现 Release 打包
- 可选 Steamworks 生命周期与成就接口

## Spine 接入说明

公开仓库默认不包含 Spine Runtime。`SpineAnimator` 已预留 spine-cpp 4.3 的接入边界，
正式集成与向 Steam 用户分发前，请先确认拥有符合 Spine Runtime License 的许可证，
并确保 Spine Editor 导出版本与 Runtime 版本一致。

## 构建

使用 Visual Studio 打开 `SPlite.slnx`，选择 `x64` + `Debug` 配置编译即可。

生成经过测试的发布目录：

```powershell
.\scripts\package-release.ps1
```

架构说明见 `docs/ARCHITECTURE.md`，Steamworks 接入见 `docs/STEAM.md`，发布前逐项检查
`docs/RELEASE_CHECKLIST.md`。

## 目录结构

```
src/                 C++ 源码
src/graphics/        D3D11 渲染器
src/shaders/         HLSL 着色器
res/                 资源文件（图标、资源脚本等）
assets/              运行时素材
docs/                项目文档
```

## 编码约定

所有 C++ 源码与头文件必须保存为 **UTF-8 带 BOM**，并且**不要**在工程里启用 `/utf-8`。

这是为了避免 MSVC 在解析 Windows SDK 头文件时出现 `C3513: raw string 分隔符` 的偶发误判。项目的 `.gitattributes` 已统一要求文本文件使用 LF 行尾。
