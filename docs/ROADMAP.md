# SPlite 开发路线图

本文档记录项目的功能里程碑、模块划分与当前进度。

## 里程碑

| 阶段 | 目标 | 状态 |
|---|---|---|
| A | Win32 窗口骨架 | 已完成 |
| B | D3D11 渲染透明 PNG 精灵 | 已完成 |
| C | 透明置顶桌宠窗口 | 规划中 |
| D | Spine 动画 | 规划中 |
| E | 多角色系统 | 规划中 |

## 模块划分

- `src/app`：程序入口与应用生命周期
- `src/platform/windows`：窗口、透明、置顶、拖拽、DPI
- `src/graphics`：D3D11 设备、交换链、着色器、纹理
- `src/shaders`：HLSL 着色器文件
- `src/model`：桌宠角色逻辑与状态机
- `src/data`：配置与资源缓存
- `src/ui`：设置界面与调试面板
- `src/animation`：Spine 适配层
- `src/steam`：Steamworks 封装

## 编码约定

- C++ 源码与头文件使用 **UTF-8 带 BOM** 编码。
- 工程**不要**开启 `/utf-8` 编译选项。
- 文本文件统一 LF 行尾（见 `.gitattributes`）。

## 当前状态

阶段 B 已完成：在普通 Win32 窗口内使用 D3D11 渲染一张带 alpha 的 PNG 精灵图。
渲染管线包括设备、交换链、顶点/像素着色器、纹理采样与 alpha 混合。
