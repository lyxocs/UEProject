"""Create a versioned player ZIP from the cooked Windows archive, without UE/SDK.

Generated files remain under Saved/Distribution and never enter source control.
Does not delete or change the package, saves or user settings.
"""
import argparse
import hashlib
import json
import re
import subprocess
import zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    with path.open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version',required=True)
    args=parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*',args.version):parser.error('Use a filename-safe version')
    package=ROOT/'Saved/Builds/Windows';output=ROOT/'Saved/Distribution';output.mkdir(parents=True,exist_ok=True)
    target=output/f'WarlockArena-Win64-{args.version}.zip'
    if target.exists():raise RuntimeError('Version ZIP already exists; use a new version')
    required=['WarlockArena.exe','WarlockArena/Binaries/Win64/WarlockArena.exe','WarlockArena/Content/Paks/WarlockArena-Windows.utoc','WarlockArena/Content/Paks/WarlockArena-Windows.ucas','WarlockArena/Content/WarlockArena/AI/TacticalPolicy.json','Engine/Extras/Redist/en-us/vc_redist.x64.exe','Engine/Extras/Redist/en-us/GameInputRedist.msi']
    assert all((package/p).is_file() for p in required),'Package incomplete; run Package.ps1 with prerequisites'
    files={}
    for path in sorted(package.rglob('*')):
        if not path.is_file():continue
        relative=path.relative_to(package)
        if 'Saved' in relative.parts or path.suffix.lower() in {'.pdb','.log','.debug','.sym'} or path.name.startswith('Manifest_'):continue
        files[relative.as_posix()]=path
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    manifest={'game':'WarlockArena','version':args.version,'source_commit':commit,'platform':'Windows x64','configuration':'Development preview','engine':'UE 5.9 Preview 2 (bundled runtime, editor not required)','files':{name:{'size':path.stat().st_size,'sha256':sha(path)} for name,path in files.items()}}
    readme=f'''术士之战 WarlockArena · {args.version}

这是可直接游玩的 Windows x64 开发预览包，不需要 UE、Visual Studio、Python、Git 或 Warcraft III。
1. 先把整个 ZIP 解压到本地文件夹；不要在压缩包内运行，不要单独复制 exe。
2. 双击 WarlockArena.exe 或下面的启动批处理。
3. 首次启动若提示安装运行库，允许正常安装；如缺少 VCRUNTIME/MSVCP DLL，运行 Install-Runtimes.bat。
   安装程序来自本机 UE 官方附带的 Microsoft 运行库，安装可能需要管理员权限。

Start-Solo.bat            单人：键鼠或第一只手柄
Start-Mixed.bat           双人：P1键鼠，P2第一只手柄
Start-TwoPads.bat         双人：P1第一只手柄，P2第二只手柄
Start-KeyboardMouse.bat   双人：P1键盘，P2鼠标

双人默认左右分屏（P1左、P2右），F5可切换上下分屏。

准备阶段：点击顶部准备完毕，或空格/手柄↑；商店内也可手柄Y。所有真人准备后立即开战，AI自动准备。
Tab/View商店；方向键或鼠标右键移动；手柄左摇杆移动并决定施法朝向，不用右摇杆。
G F D R T E C Y 对应八个技能；RT LT X A Y B LB RB 对应手柄八技能。
S/鼠标中键/R3取消；Esc/Menu暂停；F1操作指南；F6分辨率；F7画质；F5分屏布局。
ESC暂停界面可选5大师、4困难、3普通、2简单、1入门；鼠标点击、数字1至5或手柄十字键左右调整。
强化学习模型随包提供，只进行本地C++推理，不需要Python、训练环境或在线AI服务。
准备阶段可移动和施法，击退不扣血；越界会安全返回，开战重置练习状态。
键盘空格准备；手柄练习时按十字键上准备，打开商店后也可Y准备。
商店：A/Enter购买，M/X切换闪现或火焰喷射模式，Q/R3查看C槽反射盾/急行路线。
Del/RT出售整件装备；购买乔丹之石后，选择已学技能按J/LT启用。石头不可售出。
技能不能在本场更换已学路线；金币保留，每回合增加10金，无利息。
鼠标单独控制P2时：右键移动，滚轮选技能，左键释放。

硬件：Windows 10/11 64位、支持DX12的显卡及较新的显卡驱动；没有免显卡/低配兼容保证。
当前仍是开发预览，不是正式完成版。真实双Xbox手柄和干净PC尚未实测。
本版本的测试、强化学习评估与性能记录见对应GitHub Release说明。
本版本的短测帧率见Release说明，不保证其他PC帧率；卡顿时用F7降画质、F6降分辨率。
这是Development构建，保留测试控制台；正式版将另外提供Shipping构建。

此包不含源码、原地图、调试符号、开发缓存、测试日志或本机玩家设置。
BUILD-MANIFEST.json记录版本、源码提交与游戏文件校验值。
源码分支：https://github.com/lyxocs/UEProject/tree/game/warlock-arena
下载页：https://github.com/lyxocs/UEProject/releases
当前仓库为私有，下载者须有仓库访问权限。
源码提交：{commit}
'''
    extra={'README-PLAY.txt':readme.encode('utf-8-sig'),'BUILD-MANIFEST.json':json.dumps(manifest,ensure_ascii=False,indent=2).encode('utf8')}
    modes={'Solo':'','Mixed':'?LocalPlayers=2?Input=Mixed','TwoPads':'?LocalPlayers=2?Input=TwoPads','KeyboardMouse':'?LocalPlayers=2?Input=KeyboardMouse'}
    for name,options in modes.items():
        extra[f'Start-{name}.bat']=('@echo off\r\ncd /d "%~dp0"\r\nstart "" "%~dp0WarlockArena.exe" /Game/WarlockArena/Maps/Arena'+options+'\r\n').encode('ascii')
    extra['Install-Runtimes.bat']=b'@echo off\r\ncd /d "%~dp0"\r\necho Installing Microsoft Visual C++ x64 runtime...\r\nstart /wait "" "%~dp0Engine\\Extras\\Redist\\en-us\\vc_redist.x64.exe"\r\necho Installing Microsoft GameInput runtime...\r\nmsiexec.exe /i "%~dp0Engine\\Extras\\Redist\\en-us\\GameInputRedist.msi"\r\npause\r\n'
    with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
        for name,path in files.items():archive.write(path,'WarlockArena/'+name)
        for name,data in extra.items():archive.writestr('WarlockArena/'+name,data)
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None,'ZIP integrity failure'
        assert not any('/Saved/' in name or name.endswith('.pdb') for name in archive.namelist())
    checksum=sha(target)
    checksum_path=output/(target.stem+'.sha256.txt');checksum_path.write_text(checksum+'  '+target.name+'\n',encoding='ascii')
    report={'zip':str(target),'size':target.stat().st_size,'sha256':checksum,'checksum_file':str(checksum_path),'source_commit':commit,'payload_files':len(files),'runtime_files_present':True,'zip_crc_verified':True}
    (output/'bundle-report.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print(json.dumps(report,ensure_ascii=False))


if __name__=='__main__':main()
