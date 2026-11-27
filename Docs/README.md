# WarlockArena 开发文档

本项目以用户提供的《术士之战 1.00b 汉化版》为玩法与数值基准，在 UE 中开发独立的术士竞技游戏。法术命中、击退累积、岩浆、回合购买与构筑是核心。已进入 UE 技术样机开发，实际完成度与验证记录见第12章；设计规格不等于所有功能已经实现。未进行 Warcraft III 原版运行测试。

## 阅读顺序

| 文档 | 用途 |
|---|---|
| [项目目标和版本范围](01_ProjectAndScope.md) | 明确复刻边界、完整目标和第一版交付内容 |
| [技能和购买规则](02_SkillsAndShop.md) | 21 个技能族、价格、等级、模式、冷却、门槛和脚本入口 |
| [物品精通和外观](03_ItemsAndMasteries.md) | 13 类战斗物品、出售、互斥、精通和外观价格 |
| [战斗击退和竞技场](04_CombatAndArena.md) | 伤害与击退公式、固定步模拟、岩浆、缩圈、障碍物 |
| [回合经济和胜负](05_RoundsAndEconomy.md) | 金币、积分、击杀归属、回合流程与配置 |
| [缺陷修正和待确认行为](06_BugsAndConfirmation.md) | 原版偏差、修正要求、确认顺序与验收证据 |
| [UE 架构和数据设计](07_UEArchitecture.md) | 模块职责、权威端、数据结构、资产组织和网络边界 |
| [构建开发和验收](08_BuildAndDelivery.md) | 环境、构建模板、里程碑、测试与完成标准 |
| [机器人与本地双人](09_AIAndLocalMultiplayer.md) | AI对手、双手柄、分屏、独立UI与新增验收 |
| [Unreal MCP配置](10_UnrealMcpSetup.md) | 官方MCP预配置、项目端口、启用步骤与验证要求 |
| [次世代画面和光影](11_VisualQualityAndLighting.md) | 现代材质、角色、光影与特效目标，双分屏画质和性能验收 |
| [当前实现与验证](12_ImplementationStatus.md) | 本机工具链、样机启动、实际验证、UI调整和待完成范围 |
| [术士角色与画面迭代](13_CharacterAndPictureIteration.md) | 原模型转换、骨骼动画、三套外观、分辨率与画质预设 |
| [战斗流程与动作回归](14_CombatFlowValidation.md) | 保护期击退修正、生命条、死亡观战、整场与动作验证 |
| [施法、输入与火焰表现](15_CastingInputAndFireFX.md) | 真实前摇与取消、AI延迟、键鼠分开、朝向施法、暂停与火焰粒子 |
| [技能表现与角色细化](16_SpellAndCharacterPolish.md) | 多层火焰、冲击波、电弧、残影、分阶段动作与角色材质修复 |
| [准备、设备分配与AI战术](17_ReadyDevicesAndAITactics.md) | 准备按钮、严格设备路由、紧凑技能栏、假动作连招与动态岩浆 |
| [Windows成品与GitHub发布](18_DistributionAndReleases.md) | 无需UE的玩家包、运行库、四种启动入口与版本化Release |
| [强化学习与五档AI](19_ReinforcementLearningAndDifficulty.md) | 原生Actor-Critic、快速无渲染采样、难度菜单、模型部署与评估 |
| [整体视觉与准备练习](22_VisualDirectionAndPreparation.md) | 四套回合配色、人物与特效、UI、准备移动和无伤害击退 |
| [客户端画面与性能](23_ClientPerformance.md) | ESC画质菜单、常驻FPS、帧率上限、跨启动保存和实测性能 |
| [本地玩家与键盘改键](24_LocalPlayersAndKeyboard.md) | ESC单人／双人、设备确认认领、回退、反向设备分配与22项改键 |
| [八人参战设置](25_ParticipantCount.md) | ESC总人数2～8、自动补AI、出生布局、合作下限与验证 |
| [轮廓与材质清晰度](26_RenderingClarity.md) | 独立TAA/TSR、渲染比例、程序化细纹过滤和模型折面边界 |
| [音乐、快速闪现与配色](27_AudioApparitionAndColors.md) | 低沉恢弘配乐、技能/碰撞声音、快速流星/闪现、八席颜色及新模型接入 |
| [逐行商店与技能引导](28_ShopRowsAndTargetGuides.md) | 滚轮/手柄滚动、开发中路线、按住瞄准、松开/左键确认及取消 |
| [专用服务器与联机大厅](29_DedicatedServerAndLobby.md) | 联机分支、Linux DS构建部署、房主与准备、独立客户端和回归验收 |
| [4K预算、全技能族与金币复查](30_ClientBudgetSkillsAndEconomy.md) | 4K四人120FPS目标、实际渲染比例、技能族、收入明细与击杀/胜利反馈 |
| [六档AI与宗师训练](31_ExpertAIAndSixTiers.md) | 保留前五档、180ms最低感知延迟、多头策略、购物和冻结快照联赛 |
| [联机平滑与键位预设](32_NetworkPresentationAndKeyPresets.md) | 镜头/角色/弹道快照插值、英雄联盟键位和快速闪现 |
| [商店与构筑](21_ShopAndBuilds.md) | 属性对比、精通前置、退款、石头、免费双模式与C槽路线 |
| [全技能抬杖与取消](20_AllSkillAnticipation.md) | 原地图核对、八技能可见前摇、S取消、持杖特效及回归 |

## 规则的裁决顺序

1. 原始地图实际运行行为与可复现记录是最终依据。
2. 对应运行路径中的脚本、物体字段、初始化和配置共同解释行为。不能只看商店文字，也不能把脚本中所有遗留分支都当成默认功能。
3. 已确认缺陷按 [缺陷文档](06_BugsAndConfirmation.md) 修正，保留“原版行为”和“开发版行为”的区别。修 bug 不等于重新平衡。
4. 本文档中的 UE 架构、UI 组织、网络和开发阶段属于实现方案，不能反过来证明原版的数值或功能。

遇到不确定行为时，先直接检查源地图对应等级、模式、装备和游戏模式的脚本与字段。禁止以其他版本攻略或记忆填充原版数值。用户不要求准备 Warcraft III 运行环境，因此原版试玩不是开工前置条件。静态证据不足的行为明确标记；用户已将手感交给代理决定，相关取舍记录为开发决策，再在 UE 中验证，不伪称已与原版实测一致。伤害、冷却和经济等核心数值仍以地图为准。

## 基准文件与可追溯数据

- 原地图：[Warlock1.00b CN.w3x](../Warlock1.00b%20CN.w3x)，4,027,446 字节。
- SHA-256：`f1e5af933797d6936dd6040fd718fffec06a88c06707007583b7539e0fefe981`。
- [完整结构化参考](../Analysis/Warlock1.00b%20CN/reference_data.json)：技能、物品、研究、单位全部修改字段及其原始对象定位；并非可直接导入 UE 的已审核配置。
- [脚本可读副本](../Analysis/Warlock1.00b%20CN/Scripts/war3map.readable.j)：仅规范化换行，保留混淆标识符。
- [提取清单](../Analysis/Warlock1.00b%20CN/inspection.json)：源文件哈希与提取状态。

对象修改表不包含 Warcraft III 的全部基础数据。字段缺省表示“沿用基础对象”，不等于数值 0。脚本包含保护、混淆、重复命名和遗留片段，不能将整个文本直接视作可独立编译的无歧义源程序。正文按可追踪的功能路径解释，疑点列入确认清单。

## 文档维护

本次文档基准日期为 2026 年 11 月 26 日。修改任何数值或规则时，记录地图哈希、对象 ID 或函数与行号、条件、原值、新值以及理由。原地图保持只读基准；原版 bug 在开发版修复，不覆盖这份证据文件。

表格由 `Scripts/build_reference.py` 生成，输入为已提取的数据。人工规则文档与生成文档分开维护，避免重新生成覆盖人工结论。原始浮点字段保留在 JSON 中，文档显示时只消除 float32 的显示噪声，例如 4.8000001907 显示为 4.8。

新增原版运行证据后，先处理冲突、更新缺陷状态，再更新下游实现和测试。不得把“文档已写”标成“游戏行为已验证”。
