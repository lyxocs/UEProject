# Windows 成品与 GitHub 发布

源码保留在 `lyxocs/UEProject` 的 `game/warlock-arena` 分支，其他游戏仍独立建仓、使用各自分支。玩家成品发布为 GitHub Release 附件，不把 exe/pak/zip 放进源码的“产出目录”，避免二进制版本持续膨胀。GitHub 自动提供的 Source code.zip 不是可玩包。

## 玩家下载

进入 https://github.com/lyxocs/UEProject/releases ，选择以 `warlock-arena-` 开头的预览版，下载 `WarlockArena-Win64-*.zip`。仓库为私有，下载者须有仓库权限；本轮不改变可见性。

完整解压后双击根目录 `WarlockArena.exe` 或 `Start-Solo.bat`；不需要安装 UE、Visual Studio、Python、Git 或 Warcraft III。不能只复制一个 exe，也不要直接在 ZIP 内启动。首次启动可能需要管理员权限安装附带的 Microsoft VC++/GameInput 运行库；必要时运行 `Install-Runtimes.bat`。运行仍要求 Windows 10/11 x64、DX12显卡与兼容驱动，未验证的低配/ARM/Linux/Mac不作兼容承诺。

附带四个入口：`Start-Solo.bat`、`Start-Mixed.bat`、`Start-TwoPads.bat`、`Start-KeyboardMouse.bat`。具体操作和限制在包内 `README-PLAY.txt`。当前使用已经过本机游戏测试的 Development 预览构建，保留控制台；不是正式 Shipping 发行版，也未完成干净PC或真实双手柄验收。

## 开发者产出流程

1. 完成本轮相关测试，关闭编辑器，运行 `Scripts/Package.ps1 -LowMemory`。默认打包附带官方引擎目录中的 VC++ 与 GameInput 安装程序、排除调试符号。`-ReuseCook` 只用于资源/配置未变的代码迭代。
2. 检查 Git 变更与敏感信息，将本项目源码、配置、资源及文档提交并推送 `game/warlock-arena`；不混入其他游戏、不强推、不改 main。按用户授权执行提交、推送和发布。
3. `python Scripts/make_distribution.py --version v0.1.0-preview.20261126`。版本名不可覆盖；后续更换版本。输出在 `Saved/Distribution/`，生成ZIP、SHA256文件和本地报告，不入源码库。
4. 产出脚本只读取打包目录，排除PDB、日志、Saved玩家设置与测试缓存；加入游戏说明、模式入口与源码提交/文件校验清单。验证ZIP CRC和必要的exe、ucas、utoc及运行库文件。再解压到独立目录进行实际渲染启动检查。
5. 使用 `Scripts/github_release.py` 的 `--inspect` 只读检查仓库权限；发布时传入 `--tag warlock-arena-<版本>`、`--commit <完整HEAD>`、`--notes <发布说明路径>` 和两个 `--asset <ZIP/SHA256路径>`。脚本要求干净工作区且源码已推送，先建草稿，逐个验证附件大小和GitHub返回的SHA256后再公布为预览版。不会覆盖已发布版本。

GitHub连接器不支持Release附件上传，本项目发布脚本使用GitHub官方REST接口；仅从本机Git Credential Manager获取已有GitHub凭据，在内存中使用，不写文件或打印凭据。脚本固定发布本仓库，上传主机限制为GitHub官方域名。无需把令牌写进仓库。

Release以项目名和版本区分同一仓库中的不同游戏，固定到对应源码提交。未来引入自动化时，UE构建机需要安装引擎；普通玩家电脑不需要。尚未部署GitHub Actions或持续构建服务。

## 本轮交付记录

2026-11-26已推送完整源码及125个LFS对象，并发布 [v0.1.0预览版](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.1.0-preview.20261126)，游戏包绑定源码提交 `55f4069f710a924c66bda0341078a0bbd263aba7`。玩家ZIP为419,485,957字节（约400.05MiB），SHA256为 `fc778d9a42f483e23feaa57a21943bb166a2d882aaa062bea1f69b13bd71875f`，GitHub上传回执的大小与校验值一致。

已验证ZIP CRC和必要文件，并从包含空格的独立解压目录、仅含Windows系统目录的PATH实际启动1280×720战斗并检查渲染成功，无材质回退或致命错误。测试电脑仍装有UE与运行库，不能据此声称干净PC实测通过；截图出现本机显存预算接近上限的警告，性能限制继续保留。未执行运行库安装或修改系统组件。

同日已发布 [v0.2.0 强化学习与五档AI预览版](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.2.0-preview.20261126)，固定源码 `07c4d33f740fa332e3527d8024ad01a774bb1999`，保留旧版。ZIP为419,506,803字节，SHA256 `fefa5a9270d54623719798b0a91982c7e817e00e15c17f420de8b76596b0158b`；GitHub正式附件的大小和摘要已核对，ZIP内源码清单与模型摘要也已核对。228项回归、五档校准和局限见第19章。

[v0.2.1 左右分屏预览版下载入口](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.2.1-preview.20261126)：默认P1左、P2右，F5仍可切换上下；实际战斗和双视口暂停菜单已渲染检查。运行方式和模型沿用v0.2.0。

[v0.3.0 全技能抬杖预览版下载入口](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.3.0-preview.20261126)：当前八基础技能均先抬杖再释放，释放前S/中键/R3可取消，左右分屏保留。68项新检查加原228项共296项通过，证据和地图核对边界见第20章。前摇改变后的AI胜率未重新校准，不沿用旧数据作新版本保证。

依据：[Epic 打包与运行库选项](https://dev.epicgames.com/documentation/en-us/unreal-engine/project-section-of-the-unreal-engine-project-settings)、[GitHub Releases接口](https://docs.github.com/en/rest/releases/releases)、[附件上传接口](https://docs.github.com/en/rest/releases/assets)。


[v0.4.0 商店构筑预览版入口](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.4.0-preview.20261126)：属性与升级对比、具体前置和金币缺口、整件装备出售、乔丹之石，以及免费闪现／折跃、连珠／齐射和C槽急行路线。规则及实际范围见第21章；491项检查通过，尚未完成其余14个原图技能族、物理双手柄与干净PC验收。


[v0.5.0 整体表现与准备练习预览版入口](https://github.com/lyxocs/UEProject/releases/tag/warlock-arena-v0.5.0-preview.20261127)：四套回合配色、石材与岩浆、人物材质与重施法动作、受击反馈和UI细化；准备可移动／施法／互相无伤击退，开战清空练习状态。手柄练习时↑准备、Y仍释放技能，商店内Y准备。527项回归通过，实际范围与画面检查见第22章。
