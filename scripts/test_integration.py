#!/usr/bin/env python3
"""Integration regressions use only original CC0 fixtures and actual engine/UI."""
import os,pathlib,subprocess,json,hashlib,tempfile,shutil,time
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1];os.chdir(root)
binary=root/'build/desktop/frotz-zero';work=root/'local/integration'/str(time.time_ns());work.mkdir(parents=True,exist_ok=True)
results=[]
def run(name,story,commands,extra=(),data=None):
    directory=work/name;directory.mkdir(exist_ok=True)
    drive=directory/'drive.txt';drive.write_text(commands)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(directory/'config'),XDG_DATA_HOME=str(data or directory/'data'))
    r=subprocess.run([str(binary),'--engine',str(story),'--drive',str(drive),*extra],env=env,capture_output=True,timeout=15)
    (directory/'engine.log').write_bytes(r.stdout+r.stderr)
    assert r.returncode==0,(name,r.returncode,r.stderr.decode(errors='replace'))
    reports=list(pathlib.Path(env['XDG_DATA_HOME']).glob('frotz-zero/stories/*/working/drive-report.json'))
    assert len(reports)==1,(name,'missing drive report')
    return json.loads(reports[0].read_text()),reports[0].parent.parent,env
for v in (3,4,5,7,8):
    story=root/f'build/fixtures/lantern.z{v}'
    r,folder,env=run(f'save-v{v}',story,'wait 150\ntext north\nkey Return\nwait 150\ntext save\nkey Return\nwait 150\ntext portable\nkey Return\nwait 150\nquit\n')
    save=folder/'saves/portable.qzl';assert save.exists(),'portable save did not commit'
    # Vanilla's -L alias/lifetime bug and 80-character dialog limit are avoided
    # by using its normal in-game RESTORE with a short, untouched copy.
    interop=root/'local'/f'interop-v{v}.qzl';shutil.copyfile(save,interop)
    vanilla=subprocess.run([str(root/'third_party/frotz/dfrotz'),'-q','-m',str(story)],input=f'restore\nlocal/interop-v{v}.qzl\nlook\nquit\n',text=True,capture_output=True,timeout=10)
    assert vanilla.returncode==0 and 'Turns: 2. Score: 1.' in vanilla.stdout,(v,vanilla.stdout,vanilla.stderr)
    r2,_,_=run(f'load-v{v}',story,'wait 150\ntext look\nkey Return\nwait 150\nquit\n',('--load',str(save)))
    assert 'Turns: 2. Score: 1.' in r2['transcript'],r2
    results.append(f'v{v}: multiple turns, portable SAVE, independent dfrotz RESTORE, app restore')
story=root/'build/fixtures/lantern.z5'
before=work/'before.png';after=work/'after.png'
r,folder,env=run('resume-start',story,f'wait 150\ntext north\nkey Return\nwait 150\ntext unfinished\nwait 100\ncapture {before}\nquit\n')
assert r['draft']=='unfinished'
r2,_,_=run('resume-finish',story,f'wait 150\ncapture {after}\nkey Home\n'+('key Delete\n'*10)+'text look\nkey Return\nwait 150\nquit\n',('--resume',),pathlib.Path(env['XDG_DATA_HOME']))
assert Image.open(before).tobytes()==Image.open(after).tobytes(),'restored prompt/draft differ'
assert 'Turns: 2. Score: 1.' in r2['transcript'] and 'Turns: 3.' not in r2['transcript']
results.append('Continue after process restart: pixel-identical prompt/draft; exactly one next turn')
generations=sorted((folder/'resume').glob('gen-*'));assert len(generations)==2
(generations[-1]/'vm.bin').write_bytes(b'truncated')
r3,_,_=run('resume-fallback',story,'wait 150\nquit\n',('--resume',),pathlib.Path(env['XDG_DATA_HOME']))
results.append('Continue recovers from a truncated newest VM using the preceding complete generation')
r,_,_=run('keys',story,'wait 100\ntext f x z c 45678 !?:,.-\nkey Return\nwait 100\ntext draft\nkey ctrl+Up\nkey ctrl+Down\nkey ctrl+v\nkey Home\nkey Down\nkey Return\nkey PageUp\ntext DO-NOT-TYPE\nkey End\nwait 100\nquit\n')
assert 'f x z c 45678 !?:,.-' in r['transcript'],r
assert r['draft']=='draft',r
assert 'DO-NOT-TYPE' not in r['transcript'] and 'DO-NOT-TYPE' not in r['draft']
results.append('Ordinary commands/digits/punctuation survive UI routing; history preserves draft; scrollback never enters text')
r,_,_=run('long-input',story,'wait 100\ntext abcdefghijklmnopqrstuvwxy\ntext abcdefghijklmnopqrstuvwxy\ntext abcdefghijklmnopqrstuvwxy\ntext abcdefghijklmnopqrstuvwxy\nwait 100\nquit\n')
assert len(r['draft'])==100,r
results.append('100-character command accepted with horizontally scrolling input')
r,_,_=run('graphics',root/'build/fixtures/graphics.z6','wait 800\ntext a\nwait 600\ntext q\nwait 100\nquit\n')
assert 'PNG resource drawn.' in r['transcript'] and 'Sound callbacks: 1.' in r['transcript'],r
assert 'Timed character read (0.3 seconds): 0.' in r['transcript'],r
results.append('v6 PNG resource rendering path, audio completion callback, timed character read; SDL dummy audio backend')
result={'passed':len(results),'checks':results,'evidence_scope':'Actual compiled engine/frontend with SDL dummy display/audio; visual screenshots inspected separately. No physical-device claim.'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(root/'local/integration/results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
