"""Small localhost-only MCP client for editor bootstrap and repeatable checks."""
import argparse
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / 'Saved' / 'Mcp'
URL = 'http://127.0.0.1:8100/mcp'


def request(payload, session=None):
    headers = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'}
    if session:
        headers['Mcp-Session-Id'] = session
        headers['MCP-Protocol-Version'] = '2025-06-18'
    req = urllib.request.Request(URL, json.dumps(payload).encode(), headers=headers)
    with urllib.request.urlopen(req, timeout=180) as response:
        session = response.headers.get('Mcp-Session-Id', session)
        if response.status == 202:
            return {}, session
        if 'text/event-stream' in response.headers.get('Content-Type', ''):
            for raw in response:
                line = raw.decode().strip()
                if line.startswith('data: '):
                    obj = json.loads(line[6:])
                    if obj.get('id') == payload.get('id'):
                        return obj, session
            return {}, session
        raw = response.read()
        return json.loads(raw) if raw else {}, session


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--match', default='')
    parser.add_argument('--call', type=Path)
    parser.add_argument('--python', type=Path, help='Execute an editor Python source file via the discovered official tool')
    args = parser.parse_args()
    STATE.mkdir(parents=True, exist_ok=True)
    response, session = request({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {'protocolVersion': '2025-06-18', 'capabilities': {}, 'clientInfo': {'name': 'WarlockBootstrap', 'version': '1.0'}}})
    if 'error' in response:
        raise RuntimeError(response)
    request({'jsonrpc': '2.0', 'method': 'notifications/initialized'}, session)
    if args.call or args.python:
        if args.python:
            params = {'name': 'call_tool', 'arguments': {'toolset_name': 'editor_toolset.toolsets.python_scripting.PythonScriptingTools', 'tool_name': 'execute_python', 'arguments': {'script': {'content': args.python.read_text(encoding='utf-8-sig'), 'language': 'Python'}}}}
        else:
            params = json.loads(args.call.read_text(encoding='utf-8-sig'))
        result, _ = request({'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call', 'params': params}, session)
        (STATE / 'last_result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        # Screenshots remain in the saved response, not dumped into terminal output.
        for block in result.get('result', {}).get('content', []):
            if block.get('type') == 'image':
                import base64
                image_path = STATE / 'last_screenshot.png'
                image_path.write_bytes(base64.b64decode(block['data']))
                print('Screenshot:', image_path)
            elif block.get('type') == 'text':
                print(block['text'])
        if 'error' in result:
            print(json.dumps(result, ensure_ascii=False))
            raise SystemExit(1)
        if result.get('result', {}).get('isError'):
            raise SystemExit(1)
    else:
        result, _ = request({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list', 'params': {}}, session)
        (STATE / 'tools.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        for tool in result.get('result', {}).get('tools', []):
            if args.match.lower() in (tool['name'] + tool.get('description', '')).lower():
                print(json.dumps(tool, ensure_ascii=False))


if __name__ == '__main__':
    main()
