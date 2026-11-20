#!/usr/bin/env python3
"""Online Library UI and HTTPS failure tests, using original CC0 fixtures.

--live additionally fetches public catalogue metadata, never game binaries.
"""
import argparse,hashlib,http.server,json,os,pathlib,shutil,ssl,subprocess,threading,time
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
args=argparse.ArgumentParser();args.add_argument('--live',action='store_true');args=args.parse_args()
work=root/'local/online-tests'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero';helper=root/'build/desktop/online-tests';checks=[]
source=work/'source';(source/'gamefiles').mkdir(parents=True)
story=bytearray((root/'build/fixtures/lantern.z5').read_bytes());story[2:4]=(2).to_bytes(2,'big')
filename='lantern-online-r2.z5';(source/'gamefiles'/filename).write_bytes(story)
row={'dir':'gamefiles','type':'z5','title':'The Lantern Room - Online demo','release':'2','serial':story[18:24].decode(),'md5':hashlib.md5(story).hexdigest(),'filename':filename,'comment':'An original CC0 demonstration. Download it, then choose New Game from the home screen.'}
(source/'catalog.json').write_text(json.dumps([row]))
def ui(name,commands,session=None,theme='dark',empty=False):
    folder=work/name;folder.mkdir()
    env=session or dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
    config=pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero';config.mkdir(exist_ok=True,parents=True)
    if not (config/'settings.conf').exists():(config/'settings.conf').write_text('theme='+theme+'\n')
    stories=pathlib.Path(env['XDG_DATA_HOME'])/'downloads';stories.mkdir(exist_ok=True,parents=True)
    if not empty and not list(stories.iterdir()):shutil.copyfile(root/'build/fixtures/lantern.z3',stories/'local.z3')
    drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    result=subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive),'--online-fixture',str(source)],env=env,capture_output=True,timeout=20)
    (folder/'output.log').write_bytes(result.stdout+result.stderr);assert result.returncode==0,(name,result.stderr)
    return folder,env,stories
start='wait 100\ntext 3\nkey Right\nwait 100\n'
folder,session,stories=ui('download',start+'capture {folder}/online.png\nkey Return\nwait 400\nkey Return\nkey Return\nwait 100\ncapture {folder}/release.png\nkey Down\nkey Return\nwait 100\ncapture {folder}/notes.png\nkey Escape\nkey Return\nwait 400\nkey Return\nwait 100\ncapture {folder}/ready.png\nquit\n')
assert (stories/filename).read_bytes()==story
assert 'recent=' not in (pathlib.Path(session['XDG_CONFIG_HOME'])/'frotz-zero/settings.conf').read_text()
checks.append('Downloads > Infocom > release notes > verified download > story page; last played unchanged')
cache=pathlib.Path(session['XDG_DATA_HOME'])/'frotz-zero/online/catalog.json';before=cache.read_bytes()
(source/'catalog.json').write_text('broken catalogue')
folder,_,_=ui('offline-cache',start+'key Down\nkey Down\nkey Return\nwait 300\ncapture {folder}/error.png\nkey Escape\nkey Home\nkey Return\nkey Return\nkey Return\nwait 100\ncapture {folder}/cached.png\nquit\n',session=session)
assert cache.read_bytes()==before
checks.append('Malformed refresh preserves the cached catalogue and offline browse still works')
(source/'catalog.json').write_bytes(before)
# Follow the link without connecting, in light mode, through the same UI.
folder,_,_=ui('manual-qr',start+'key Down\nkey Down\nkey Down\nkey Return\nkey Down\nkey Return\nwait 100\ncapture {folder}/manual.png\nquit\n',theme='light')
qr_image=folder/'manual.png';assert Image.open(qr_image).size==(320,170)
checks.append('Manual link and QR render on the real 320x170 light theme')
if shutil.which('zbarimg'):
    decoded=subprocess.run(['zbarimg','--quiet',str(qr_image)],capture_output=True,text=True,check=True).stdout.strip()
    assert decoded=='QR-Code:https://infodoc.plover.net/manuals/index.html',decoded
    checks.append('Independent QR decoder recovers the exact manual URL from the rendered screen')
folder,_,_=ui('empty-library','wait 100\nkey Return\nkey Return\nkey Right\nwait 100\ncapture {folder}/empty-online.png\nquit\n',empty=True)
assert Image.open(folder/'empty-online.png').tobytes()==Image.open(work/'download/online.png').tobytes()
checks.append('An empty local library exposes Online Library without needing a story first')
# File collision: the existing file has the same name and different bytes.
folder=work/'conflict-session';folder.mkdir();env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
conflict=folder/'data/downloads'/filename;conflict.parent.mkdir(parents=True);conflict.write_bytes(b'keep this original')
out,_,_=ui('conflict',start+'key Return\nwait 300\nkey Return\nkey Return\nkey Return\nwait 300\ncapture {folder}/conflict.png\nquit\n',session=env)
assert conflict.read_bytes()==b'keep this original'
checks.append('Actual UI download refuses a different existing file and preserves its exact bytes')

cert=work/'cert.pem';key=work/'key.pem'
subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self,*unused):pass
    def do_GET(self):
        code=302 if self.path=='/redirect' else 404 if self.path=='/missing' else 200
        self.send_response(code)
        if code==302:self.send_header('Location','/ok')
        self.end_headers()
        try:
            if self.path=='/slow':
                for _ in range(40):self.wfile.write(b'x'*1024);self.wfile.flush();time.sleep(.05)
            else:self.wfile.write(bytes(story))
        except (BrokenPipeError,ConnectionResetError,ssl.SSLError):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key);server.socket=context.wrap_socket(server.socket,server_side=True)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start();base=f'https://localhost:{server.server_port}'
def fetch(name,url,limit=8*1024*1024,ca=cert,cancel=0,error=None):
    out=work/(name+'.bin');r=subprocess.run([str(helper),'fetch',url,str(limit),str(ca),str(cancel),str(out)],capture_output=True,text=True,timeout=20)
    (work/(name+'.log')).write_text(r.stdout+r.stderr)
    if error:assert r.returncode and error.lower() in r.stderr.lower() and not out.exists(),(name,r.returncode,r.stderr)
    else:assert r.returncode==0,(name,r.stderr)
    return out
try:
    assert fetch('https-ok',base+'/ok').read_bytes()==story
    fetch('oversize',base+'/ok',limit=10,error='size limit')
    fetch('untrusted',base+'/ok',ca='-',error='certificate')
    fetch('wrong-host',base.replace('localhost','127.0.0.1')+'/ok',error='certificate')
    fetch('redirect',base+'/redirect',error='HTTP 302')
    fetch('http-error',base+'/missing',error='HTTP 404')
    fetch('insecure',base.replace('https:','http:')+'/ok',error='Only HTTPS')
    began=time.monotonic();fetch('cancelled',base+'/slow',cancel=100,error='cancelled');assert time.monotonic()-began<5
finally:server.shutdown();server.server_close();thread.join()
checks.append('Real HTTPS bytes, trusted CA/hostname enforcement, size limit, redirect/HTTP errors, insecure URL rejection and prompt cancellation')
if args.live:
    public=fetch('live-catalogue','https://eblong.com/infocom/catalog.json',limit=4*1024*1024,ca='-')
    r=subprocess.run([str(helper),'catalog',str(public)],capture_output=True,text=True,check=True);(work/'live-parse.log').write_text(r.stdout)
    checks.append('Live public HTTPS catalogue fetch and parse: '+r.stdout.strip())
result={'passed':len(checks),'checks':checks,'artifacts':str(work),'qr_image':str(qr_image),'scope':'Original fixture stories only; live option fetches catalogue metadata, not commercial games.'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
