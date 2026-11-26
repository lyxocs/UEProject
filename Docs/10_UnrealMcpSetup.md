# WarlockArena Unreal MCP 配置

## 当前状态

2026年11月26日已验证完整引擎安装，使用UE 5.9自带官方 ModelContextProtocol、AllToolsets、PythonScriptPlugin，不依赖第三方同名插件。项目插件仅对Editor目标启用。

本机 http://127.0.0.1:8100/mcp 已完成标准JSON-RPC握手、tools/list、schema发现、当前项目路径查询，且实际创建材质/关卡、启动PIE、查询运行对象和截取画面。可复现客户端为 Scripts/unreal_mcp.py，场景脚本为 Scripts/bootstrap_arena.py。

用户级 C:\Users\johnyxliu\.codex\config.toml 中 unreal_warlock 已设 enabled=true。当前对话通过本机HTTP客户端调用官方MCP；Codex原生工具列表若未热加载新配置，在重新加载连接或下一次会话时读取。不要把配置已启用与原生会话工具已刷新混为一谈。

本机Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini正确设置节为：

```ini
[/Script/ModelContextProtocol.ModelContextProtocolSettings]
bAutoStartServer=True
ServerPortNumber=8100
ServerUrlPath=/mcp
```

Saved不入库。Scripts/Run.ps1 -Mode Editor也传入启动和端口参数，因此新机器可以显式启动。关闭编辑器后MCP离线是预期行为。本轮验证完成后已退出测试编辑器释放显存，连接配置继续保留。

## 其他电脑的配置

## 首次连接步骤

1. 确认引擎完整安装并创建用户授权的UE项目。创建时保留已有文档、原地图和预配置文件。
2. 检查.uproject或插件界面，确认`ModelContextProtocol`与`AllToolsets`或所需具体toolset启用。未启用时合并已有Plugins数组，不能覆盖原项目文件。
3. 打开项目，在编辑器偏好设置搜索MCP，核对Auto Start Server、端口8100、路径/mcp。Saved配置可能在项目生成或编辑器启动中变化，应以实际界面及日志为准。
4. 若服务未运行，在UE控制台执行`ModelContextProtocol.StartServer`。也可在启动编辑器时使用`-ModelContextProtocolStartServer -ModelContextProtocolPort=8100`，但不要在同一端口同时启动另一个编辑器。
5. 将用户配置中`[mcp_servers.unreal_warlock]`的`enabled`改为`true`，重新加载Codex连接。不要再次添加同名服务器或另建一条重复连接。
6. 执行`codex mcp get unreal_warlock`或`codex mcp list`确认客户端已读取配置。这只能证明配置加载，不能证明服务器连通。
7. 实际执行MCP握手和工具发现，先只读查询当前项目与关卡，确认操作的是WarlockArena，再进行编辑。端口打开、HTTP返回或服务器名称出现在列表中均不等于工具已验证。

工具发现的API名称以实际引擎提供的schema为准，不根据旧教程假设固定工具名。关闭编辑器后实时MCP调用不可用。首版先直连，不提前安装代理服务。

## 多游戏约定

WarlockArena固定使用8100。后续其他游戏分配各自端口与具名连接，例如8101、8102；不要让多个编辑器争用8000或8100。可以在确认客户端正确加载项目级配置后再迁移连接作用域。每次开始编辑先用只读工具核对项目路径。

服务仅连接本机127.0.0.1，本地开发不需要公网端口或防火墙入站开放。此处不涉及外部大模型API密钥，也不自动配置Unreal Assistant的模型供应商。

## 官方依据

- [Epic官方MCP配置指南](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin/blob/main/skills/unreal-mcp/references/setup.md)
- [UE 5.9 Preview更新说明](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-release-notes)
- [Codex MCP配置说明](https://learn.chatgpt.com/docs/extend/mcp)
