"""Publish this game's pre-release using Git's existing GitHub credential helper.

Credentials stay in memory and are never printed, saved or sent to other hosts.
Creates a draft first; existing published releases/assets are never replaced.
"""
import argparse
import hashlib
import http.client
import json
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
REPO='lyxocs/UEProject'
API='/repos/'+REPO


def git(*args):
    return subprocess.check_output(['git',*args],cwd=ROOT,text=True).strip()


def credential():
    result=subprocess.run(['git','credential','fill'],cwd=ROOT,input='protocol=https\nhost=github.com\npath='+REPO+'.git\n\n',text=True,capture_output=True)
    values=dict(line.split('=',1) for line in result.stdout.splitlines() if '=' in line)
    if result.returncode or not values.get('password'):
        raise RuntimeError('GitHub credential helper did not provide credentials')
    return values['password']


def request(token,method,path,data=None):
    headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2026-03-10','User-Agent':'WarlockArena-Release'}
    body=None
    if data is not None:
        body=json.dumps(data,ensure_ascii=False).encode('utf8');headers['Content-Type']='application/json'
    req=urllib.request.Request('https://api.github.com'+path,body,headers,method=method)
    try:
        with urllib.request.urlopen(req,timeout=60) as response:return json.load(response)
    except urllib.error.HTTPError as error:
        if error.code==404:return None
        raise RuntimeError(f'GitHub API {method} returned HTTP {error.code}') from None


def upload(token,release,path):
    size=path.stat().st_size
    digest=hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
    existing=next((a for a in release['assets'] if a['name']==path.name),None)
    if existing:
        if existing.get('digest')!='sha256:'+digest or existing['size']!=size:
            raise RuntimeError('Existing asset differs; use a new version, do not overwrite')
        return existing
    url=urllib.parse.urlsplit(release['upload_url'].split('{')[0])
    if url.scheme!='https' or url.hostname!='uploads.github.com':raise RuntimeError('Unexpected upload host')
    connection=http.client.HTTPSConnection('uploads.github.com',timeout=600)
    connection.putrequest('POST',url.path+'?'+urllib.parse.urlencode({'name':path.name}))
    for key,value in {'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','User-Agent':'WarlockArena-Release','X-GitHub-Api-Version':'2026-03-10','Content-Type':'application/zip' if path.suffix=='.zip' else 'text/plain','Content-Length':str(size)}.items():connection.putheader(key,value)
    connection.endheaders()
    sent=0;next_progress=32*1024*1024
    with path.open('rb') as source:
        while chunk:=source.read(1024*1024):
            connection.send(chunk);sent+=len(chunk)
            if sent>=next_progress:
                print(f'Uploading {path.name}: {sent//1024//1024}/{size//1024//1024} MiB',flush=True);next_progress+=32*1024*1024
    response=connection.getresponse();payload=response.read();connection.close()
    if response.status!=201:raise RuntimeError(f'Asset upload returned HTTP {response.status}')
    asset=json.loads(payload)
    if asset['size']!=size or asset.get('digest')!='sha256:'+digest:raise RuntimeError('Uploaded asset size/digest mismatch; draft left unpublished')
    return asset


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspect',action='store_true')
    parser.add_argument('--tag');parser.add_argument('--commit');parser.add_argument('--notes',type=Path)
    parser.add_argument('--asset',type=Path,action='append',default=[])
    args=parser.parse_args();token=credential()
    repo=request(token,'GET',API)
    if not repo:raise RuntimeError('Repository not accessible')
    if args.inspect:
        releases=request(token,'GET',API+'/releases?per_page=20')
        print(json.dumps({'repository':repo['full_name'],'private':repo['private'],'push':repo.get('permissions',{}).get('push'),'releases':[{'tag':r['tag_name'],'draft':r['draft']} for r in releases]},ensure_ascii=False));return
    if git('branch','--show-current')!='game/warlock-arena':raise RuntimeError('Wrong project branch')
    if not args.tag or not args.tag.startswith('warlock-arena-') or not re.fullmatch(r'[0-9a-f]{40}',args.commit or '') or not args.notes or not args.asset:parser.error('tag, full commit, notes and assets required')
    if git('rev-parse','HEAD')!=args.commit:raise RuntimeError('Commit is not current HEAD')
    if git('status','--porcelain'):raise RuntimeError('Commit the project before publishing')
    if git('ls-remote','origin','refs/heads/game/warlock-arena').split()[0]!=args.commit:raise RuntimeError('Push source branch before publishing')
    if not all(p.is_file() and p.stat().st_size<2*1024**3 for p in args.asset):raise RuntimeError('Missing or oversized asset')
    release=request(token,'GET',API+'/releases/tags/'+urllib.parse.quote(args.tag,safe=''))
    if release:
        if not release['draft']:raise RuntimeError('Release already published; create a new version')
        if release['target_commitish']!=args.commit:raise RuntimeError('Existing draft targets another commit')
    else:
        release=request(token,'POST',API+'/releases',{'tag_name':args.tag,'target_commitish':args.commit,'name':args.tag,'body':args.notes.read_text(encoding='utf-8-sig'),'draft':True,'prerelease':True})
    uploaded=[upload(token,release,p) for p in args.asset]
    published=request(token,'PATCH',API+'/releases/'+str(release['id']),{'draft':False,'prerelease':True,'make_latest':'false'})
    result={'url':published['html_url'],'tag':args.tag,'commit':args.commit,'assets':[{'name':a['name'],'size':a['size'],'sha256':a['digest'],'url':a['browser_download_url']} for a in uploaded]}
    output=ROOT/'Saved/Distribution/published-release.json';output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False))


if __name__=='__main__':main()
