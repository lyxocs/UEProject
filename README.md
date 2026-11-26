# WarlockArena

基于《术士之战 1.00b 汉化版》规则开发的 UE 游戏。目标包含法术击退、岩浆竞技场、回合购买、机器人，以及双手柄本地双人分屏。

已创建 UE C++ 项目并进入可玩技术样机阶段，引擎基线为 UE 5.9 Preview 2。键鼠与 Xbox 手柄均保留，支持键盘/鼠标分开双人、键鼠＋一只手柄及两只手柄的双本地玩家布局。当前完成度与待验收内容见 [本机开发记录](Docs/12_ImplementationStatus.md)，不是正式画质首版的完成声明。

**只想玩游戏：** 到[Releases下载区](https://github.com/lyxocs/UEProject/releases)下载 `WarlockArena-Win64-*.zip`，完整解压后运行 `WarlockArena.exe`。不需要 UE 或编译环境；首次可能需安装附带的 Microsoft 运行库。私有仓库需要访问权限，GitHub自动生成的Source code压缩包不是可玩包。四种单/双人启动入口及发布流程见[成品交付说明](Docs/18_DistributionAndReleases.md)。

从项目目录启动（本机已保存引擎路径；其他电脑传入 `-EngineRoot` 或设置 `UE_ENGINE_ROOT`）：

```powershell
.\Scripts\Build.ps1
.\Scripts\Run.ps1 -Mode Solo
.\Scripts\Run.ps1 -Mode Mixed   # 键鼠 P1 + 第一只手柄 P2
.\Scripts\Run.ps1 -Mode TwoPads # 第一只手柄 P1 + 第二只手柄 P2
.\Scripts\Run.ps1 -Mode Editor  # 编辑器及 MCP
.\Scripts\Run.ps1 -Mode Solo -Packaged # 已打包的开发样机
.\Scripts\Run.ps1 -Mode Mixed -Packaged
.\Scripts\Run.ps1 -Mode KeyboardMouse -Packaged # 键盘P1，鼠标P2
```

开局有40秒购买时间，Tab / View打开商店。鼠标右键点击移动，鼠标瞄准，左键火球；手柄左摇杆移动并决定施法朝向，不用右摇杆。F1查看完整操作，F2/F3/F8在准备阶段加入本地第二人，F4重开并切换合作/FFA，F5切换分屏布局。Esc/Menu暂停；S/中键/R3停止并取消施法。F8模式下P2滚轮选技能、左键释放、右键移动。初始20金，每轮补给10金。技能必须实际购买，不能直接使用全部八个。

准备阶段 H / 手柄左摇杆按下切换赤曜、霜月、幽林术士。Esc/Menu 暂停后点「画面与性能」或按 Tab/手柄X，调整五档画质、窗口分辨率、显示模式、场景比例、帧率上限、VSync、阴影、光照、反射和特效；即时保存，本地双人共享。右上角常驻 FPS 与帧时间。新安装默认均衡、120 FPS 上限、关闭VSync；已有设置保留。F6/F7仍可快速切换，UI保持输出分辨率。较慢PC先选流畅，需要时再降场景比例；实际测量与限制见[客户端性能](Docs/23_ClientPerformance.md)。

最新火球、全技能特效和角色更新见[第16章](Docs/16_SpellAndCharacterPolish.md)。火球已改为透明多层火焰，速度600单位/秒；碰撞范围保持原值。

准备阶段点击顶部“准备完毕”，或按空格/手柄↑；商店内也可按手柄Y。所有真人准备后立即开战，AI默认准备。最新设备分配修复、紧凑战斗技能栏、AI假动作与闪现接天罚见[第17章](Docs/17_ReadyDevicesAndAITactics.md)。

ESC/Menu暂停界面可选大师、困难、普通、简单、入门五档AI；点击按钮、数字1～5或手柄十字键左右调整，默认普通。强化学习策略只需随包模型文件，玩家不用安装训练环境。训练方式与评估范围见[第19章](Docs/19_ReinforcementLearningAndDifficulty.md)。

双人默认左右分屏：P1在左、P2在右；F5仍可切换为上下分屏。

当前八个基础技能均有明显抬杖和释放前取消窗口，S/中键/R3可取消，已释放的效果不能撤回。时长与原地图证据边界见[第20章](Docs/20_AllSkillAnticipation.md)，该章覆盖旧版较短前摇预设。

- [开发文档入口](Docs/README.md)
- [原地图提取记录](Analysis/README.md)
- [项目协作约定](AGENTS.md)

## Git 管理

本目录保留独立的本地仓库。上级 `D:\UE` 是工作区，不建立总仓库；其他游戏也在各自目录中管理。

远程统一使用 [lyxocs/UEProject](https://github.com/lyxocs/UEProject)，本游戏使用 `game/warlock-arena` 分支。其他游戏各自使用不同的 `game/<英文短名>` 分支，不把游戏内容合并到远程main，也不把其他游戏分支检出到本目录。

使用 Git LFS 管理原版地图及 UE 二进制资产。其他电脑安装Git LFS后，按本游戏分支克隆：

```powershell
git clone --single-branch --branch game/warlock-arena https://github.com/lyxocs/UEProject.git WarlockArena
git -C WarlockArena lfs pull
```

访问该私有仓库需要相应权限。每次推送仅针对本游戏分支，不使用`--all`或强制推送。

提交文档、源码、必要配置、原地图、分析证据和维护脚本。忽略 UE 的 Saved/Intermediate/Binaries/缓存、IDE产物、Python构建和本机MCP配置。已有本机Saved配置仍在磁盘上；复建方法见MCP文档。项目插件若依赖预编译二进制，单独决定跟踪范围，不因清理规则漏掉必需文件。

工作区根目录 `AGENTS.md` 是本机多个游戏共享的约定，不属于本仓库；本游戏的协作与开发规则也记录在本目录中。

## 地图解析依赖

`Scripts/vendor/dclimplode-0.0.1.0/` 保留解压器源码及原有版权声明。为适配本机Python 3.14，其setup.py禁用了旧distutils monkeypatch并移除了不适用于Windows链接器的参数。编译产物和本机安装目录不纳入版本。

在有C++编译环境和Python构建依赖的机器上，从项目根目录安装本地源码：

```powershell
python -m pip install --target Scripts/vendor Scripts/vendor/dclimplode-0.0.1.0
```

然后按 [构建与开发文档](Docs/08_BuildAndDelivery.md) 运行分析脚本。仅解析地图数据，不执行地图中的JASS代码。
