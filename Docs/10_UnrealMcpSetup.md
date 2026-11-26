# WarlockArena Unreal MCP 配置

## 当前状态

2026年11月26日，用户授权提前安装和配置MCP。选择UE 5.9 Preview 2随引擎提供的官方Unreal MCP，不额外安装第三方同名插件。

检查时引擎尚在下载，未发现Build.version、UnrealEditor.exe或ModelContextProtocol插件目录；本项目也还没有.uproject。因此目前是预配置完成，不是插件已加载、服务器已启动或工具调用已通过。

## 已准备的文件

| 文件 | 用途 |
|---|---|
| `C:\Users\johnyxliu\.codex\config.toml` | 已登记Codex连接unreal_warlock，地址http://127.0.0.1:8100/mcp，当前禁用 |
| `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` | 为后续编辑器配置自动启动、端口8100和/mcp路径；Saved属于本机生成配置 |

Codex连接当前设置`enabled = false`，避免引擎未就绪时出现连接错误。首次尝试的子目录配置未被当前CLI读取，因此改为在用户配置中登记；保留已有其他服务器配置，只增加这一条连接，并移除了尚未生效的重复项目配置。该连接名称和端口专用于WarlockArena，启用后先核对目标项目。

## 下载完成后的启用步骤

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
