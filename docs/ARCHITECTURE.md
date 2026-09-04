# SPlite 架构说明

## 运行时数据流

```text
Win32 消息与计时
        |
        v
PetSystem ------> IAnimationPlayer / SpineAnimator
        |
        | SpriteTransform 列表
        v
RendererD3D11 --> 共享纹理、几何与管线状态
        |
        v
DirectComposition --> Windows 桌面
```

## 模块边界

- `animation`：动画状态、时间推进与 Spine 适配边界，不创建窗口。
- `model`：角色实例、独立位置和拖动状态，不持有 D3D 对象。
- `graphics`：GPU 资源、纹理、着色器和绘制，不决定角色行为。
- `platform/windows`：托盘、开机启动及后续 Windows 专属能力。
- `data`：用户本地配置与后续角色清单、资源缓存索引。
- `steam`：Steamworks 生命周期和平台功能，始终允许空实现。

## 多角色原则

一个显示器使用一个透明宿主窗口和一个 D3D11 设备。角色实例只保存自身状态，
纹理、图集和着色器由渲染层共享。当前原型按实例更新常量并绘制；角色数量增长后，
下一项性能优化是把相同图集和混合模式的角色整理为实例化批次。

当前原型暂时使用仅包围测试角色的紧凑透明宿主窗口，避免透明窗口覆盖整个桌面。
正式多显示器版本将拆分“只负责显示的合成窗口”和“只负责命中的交互窗口”，
在不裁切 DirectComposition 内容的前提下实现可靠的跨进程逐像素穿透。

## 外部依赖原则

Spine Runtime 和 Steamworks SDK 都不直接提交到公开仓库。工程通过编译开关和本地
MSBuild 属性接入，使没有商业 SDK 的贡献者仍能编译、运行测试和开发通用模块。
