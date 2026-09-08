# SkyTycoon · 晴空地产

Unreal Engine 5.7.4 原创回合制地产游戏。最终目标仍为微信小游戏；当前交付是 UE 本地原型，尚未验证微信导出链路。

## 当前实现

- 无第三方资源的 Canvas 棋盘、鼠标与触摸按钮、键盘快捷键。
- 2～4 席位本机回合制，默认 1 真人 + 1 人机；支持三种购买策略。掷骰、买地、租金、机会事件、税费、工资和破产结算。
- 可序列化的地图与对局结构；地图闭环、位置与经济参数校验。
- 规则自动化测试。联机、玩家地图编辑器和云存档尚未实现。

## 启动

使用 UE 5.7 打开 `SkyTycoon.uproject` 并编译模块。新建一个空白关卡，World Settings 的 GameMode Override 保持 None，然后 Play。棋盘由 HUD 绘制，不需要场景美术；关卡尚未作为二进制资源随工程提供，因此当前不能直接打包成成品。

Space：掷骰/买地/结束回合；E：跳过购买或结束回合；R：重新开始。也可点击底部按钮。当前英文占位界面避免依赖额外字体，后续补中文 UI 与原创美术。

自定义地图暂通过继承 `TycoonGameMode` 的蓝图默认值编辑 `CustomMap`：数组顺序就是行走路径，第一格必须为 Start，各格需首尾相邻且不重叠。PlayerCount 可设为 2～4。

BotCount 设置人机数（最多总人数减一），BotDifficulty 选择购买策略；0 为纯真人本机轮流操作。正式联机上限为 4 真人，总席位含人机仍最多 4。

策划见 [玩法策划](Docs/GameDesign.md)、[数值规划](Docs/BalancePlan.md)。未实现的联机和云服务已明确标注。

## 本机环境

完整引擎为 UE 5.7.4，路径 `D:\Epic Games\UE_5.7`。旧注册表项 `D:\Program Files\UE_5.6` 仅剩残留文件，不能用于构建。本项目已关联 5.7，无需重装引擎。

已检测到 Visual Studio 2022 17.14、MSVC 14.44 和 Windows SDK 10.0.22621。Visual Studio SDK 编辑器集成警告不阻止 C++ 编译。内存压力触发 UBA 重试时，使用以下低并发命令：

```powershell
& 'D:\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' SkyTycoonEditor Win64 Development -Project='F:\UEproject\SkyTycoon\SkyTycoon.uproject' -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1
```

## 架构与后续实施

2026-09-08 验证：SkyTycoonEditor Win64 Development 编译成功；无界面 UE 自动化测试 `SkyTycoon.Rules.TurnAndEconomy` 通过（1 项通过、0 失败），覆盖回合归属、非法骰点、购买/重复购买、租金、工资、破产胜负、人机资金保留和人数上限。项目 Skill 校验通过。尚未完成图形界面试玩、打包或微信真机验证。

`TycoonRules` 负责独立规则与数据，`TycoonGame` 负责本地输入和显示。掷骰结果由规则调用方提供；正式联机时必须由服务器生成，客户端只能提交操作意图。

联机下一阶段采用服务端权威的回合命令、对局版本号与断线快照；地图保存携带作者、版本和内容校验；云存档采用鉴权用户与存档槽隔离。云服务商、部署和认证需在接入时落实，当前没有上传任何数据。

UE Canvas、原生网络和存储在微信运行环境中的兼容性尚未验证。先验证最小 UE 场景在微信真机的可用转换链路，再决定发布适配器；缺少转换链路时需要评估独立微信客户端或云渲染的成本，不能承诺 UE 工程直接导入微信开发者工具。
