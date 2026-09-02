# SPlite

一个面向 Windows 的商业级桌面宠物（Pet-on-Desktop）项目。

## 技术栈

- C++20
- Win32 / DirectX
- Spine Runtime（计划集成）
- Steamworks（计划集成）

## 当前状态

项目当前完成阶段 C：已经具备可交互的透明置顶桌宠窗口。

已实现：

- Win32 桌面应用骨架与无阻塞消息循环
- D3D11 渲染管线（设备、交换链、顶点/像素着色器、纹理、alpha 混合）
- 通过 WIC 加载带 alpha 通道的 PNG 精灵
- HLSL 着色器从独立文件加载，便于后续扩展
- DirectComposition 逐像素透明合成
- 无边框置顶窗口、透明像素点击穿透、实体区域拖动
- 右键菜单切换置顶状态或退出程序

## 构建

使用 Visual Studio 打开 `SPlite.slnx`，选择 `x64` + `Debug` 配置编译即可。

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
