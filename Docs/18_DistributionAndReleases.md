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

依据：[Epic 打包与运行库选项](https://dev.epicgames.com/documentation/en-us/unreal-engine/project-section-of-the-unreal-engine-project-settings)、[GitHub Releases接口](https://docs.github.com/en/rest/releases/releases)、[附件上传接口](https://docs.github.com/en/rest/releases/assets)。
