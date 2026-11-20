#!/usr/bin/env python3
"""Stateful UI, media, failure and archive regressions on original fixtures."""
import pathlib,os,subprocess,tempfile,json,hashlib,zipfile,stat,time
from PIL import Image
from inspect_archive import extract
root=pathlib.Path(__file__).resolve().parents[1];os.chdir(root)
work=root/'local/extended'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero';checks=[]
def run(name,commands,story='lantern.z5',data=None,settings='',extra=(),audio='dummy',expected=0):
    folder=work/name;folder.mkdir();drive=folder/'drive';drive.write_text(commands)
    config=folder/'config/frotz-zero';config.mkdir(parents=True);(config/'settings.conf').write_text(settings)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER=audio,XDG_CONFIG_HOME=str(config.parent),XDG_DATA_HOME=str(data or folder/'data'))
    args=[str(binary),'--engine',str(root/'build/fixtures'/story),'--drive',str(drive),*extra] if story else [str(binary),'--stories',str(root/'build/fixtures'),'--drive',str(drive)]
    r=subprocess.run(args,env=env,capture_output=True,timeout=20);(folder/'output.log').write_bytes(r.stdout+r.stderr)
    assert r.returncode==expected,(name,r.returncode,r.stderr.decode(errors='replace'))
    reports=list(pathlib.Path(env['XDG_DATA_HOME']).glob('frotz-zero/stories/*/working/drive-report.json'))
    return (json.loads(reports[0].read_text()) if reports else {}), (reports[0].parent.parent if reports else folder),env

r,folder,env=run('named','wait 200\ntext save\nkey Return\nwait 150\ntext keepsake\nkey Return\nwait 150\nquit\n')
save=folder/'saves/keepsake.qzl';before=save.read_bytes()
run('cancel-overwrite','wait 200\ntext north\nkey Return\ntext save\nkey Return\nwait 150\ntext keepsake\nkey Return\nwait 100\nkey Return\nwait 100\nquit\n',data=pathlib.Path(env['XDG_DATA_HOME']))
assert save.read_bytes()==before
checks.append('Named Quetzal overwrite defaults to Cancel and preserves prior bytes')
r,folder,_=run('typeahead-save','wait 200\ntext delay\nkey Return\nwait 50\ntext queued-name\nkey Return\nwait 7000\nquit\n')
assert (folder/'saves/queued-name.qzl').exists(),r
checks.append('Keyboard typeahead survives busy VM processing into the Save name dialog')
r,folder,_=run('menu-checkpoint','wait 200\ntext draft\nkey ctrl+Escape\nkey Down\nkey Return\nwait 100\ntext menu-save\nkey Return\nkey Return\nwait 100\nquit\n')
cp=folder/'saves/menu-save.fzsave';assert cp.is_dir()
r,_,_=run('menu-checkpoint-load','wait 200\nquit\n',extra=('--load',str(cp)));assert r['draft']=='draft'
checks.append('App menu creates a named checkpoint and restores its draft')
before=work/'view-before.png';after=work/'view-after.png'
r,folder,env=run('view-save',f'wait 150\ntext draft\nkey ctrl+=\nkey ctrl+Right\nwait 1400\ncapture {before}\nquit\n')
r,_,_=run('view-resume',f'wait 200\ncapture {after}\nquit\n',data=pathlib.Path(env['XDG_DATA_HOME']),settings='theme=light\ntext_size=14\n',extra=('--resume',))
assert Image.open(before).tobytes()==Image.open(after).tobytes()
checks.append('Continue preserves the exact screen, draft and manual pan when reading size is unchanged')
r,_,_=run('return-library','wait 150\nkey ctrl+Escape\nkey End\nkey Up\nkey Return\n')
r,_,_=run('exit-launcher','wait 150\nkey ctrl+Escape\nkey End\nkey Return\n',expected=78)
checks.append('Repeated launch, menu return and explicit launcher exit return the intended statuses')
_,folder,env=run('settings','wait 150\ntext 4\nkey Return\nkey Down\nkey Return\nkey Down\nkey Return\nkey Escape\nkey Escape\n',story=None)
settings=(pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero/settings.conf').read_text()
assert 'theme=light' in settings and 'text_size=14' in settings and 'mute=1' in settings,settings
checks.append('Real home/settings menu persists light theme, reading size and mute')
for name,settings,audio in [('muted','mute=1\n','dummy'),('no-device','', 'not-an-audio-driver')]:
    r,folder,_=run(name,'wait 1000\ntext a\nwait 600\ntext q\n',story='graphics.z6',settings=settings,audio=audio)
    assert 'Sound callbacks: 1.' in r['transcript'],r
    if name=='no-device':assert (folder/'audio-status').exists()
checks.append('Muted and unavailable audio retain one natural completion callback')
r,_,_=run('pointer','wait 1000\ntext a\nwait 600\nclick 100 85\nkey ctrl+0\n'+('key ctrl+f\n'*20)+'key ctrl+c\nclick 100 85\nkey ctrl+Space\nkey Right\nkey Return\nkey Escape\ntext q\n',story='graphics.z6')
assert 'Pointer: 179,201.' in r['transcript'] and 'Pointer: 125,86.' in r['transcript'] and 'Pointer: 129,86.' in r['transcript'],r
checks.append('V6 fixed-window/font3 fixture and inverse pointer mapping at fitted and zoomed/panned scales')
r,folder,_=run('v6-save','wait 1000\ntext a\nwait 600\ntext s\nwait 100\ntext six\nkey Return\nwait 150\ntext q\n',story='graphics.z6')
save6=folder/'saves/six.qzl';assert save6.exists()
r,_,_=run('v6-load','wait 400\ntext q\n',story='graphics.z6',extra=('--load',str(save6)))
assert 'Save result: 2.' in r['transcript'],r
checks.append('V6 story-provided Quetzal save and restore preserve the VM continuation')
bad=work/'bad.qzl';bad.write_bytes(save6.read_bytes()[:25]);digest=hashlib.sha256(bad.read_bytes()).hexdigest()
run('reject-corrupt','quit\n',story='graphics.z6',extra=('--load',str(bad)),expected=1)
assert hashlib.sha256(bad.read_bytes()).hexdigest()==digest
checks.append('Truncated portable save rejected without modifying the source')
for i,name in enumerate(('../escape','/absolute','C:/windows','folder/../../escape')):
    archive=work/f'unsafe-{i}.zip'
    with zipfile.ZipFile(archive,'w') as z:z.writestr(name,'unsafe')
    try:extract(archive,work/f'extracted-{i}')
    except ValueError:pass
    else:raise AssertionError('Traversal archive accepted')
archive=work/'link.zip'
with zipfile.ZipFile(archive,'w') as z:
    entry=zipfile.ZipInfo('link');entry.create_system=3;entry.external_attr=(stat.S_IFLNK|0o777)<<16;z.writestr(entry,'../escape')
try:extract(archive,work/'links')
except ValueError:pass
else:raise AssertionError('Symlink archive accepted')
checks.append('Archive traversal, absolute/Windows drive paths and symlinks rejected')
result={'passed':len(checks),'checks':checks,'scope':'Actual desktop frontend/engine, original CC0 fixtures; no hardware claim'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
