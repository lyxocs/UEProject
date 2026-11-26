# WarlockArena

基于《术士之战 1.00b 汉化版》规则开发的 UE 游戏。目标包含法术击退、岩浆竞技场、回合购买、机器人，以及双手柄本地双人分屏。

当前阶段为地图分析与开发规格，尚未创建 UE 游戏项目。引擎基线为 UE 5.9 Preview 2。

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
