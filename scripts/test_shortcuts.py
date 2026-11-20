#!/usr/bin/env python3
"""Single-modifier controls through the shared Linux-keycode/xkb path.

The driver supplies events inside the application, never through OS input devices.
Only original CC0 stories and isolated preferences/saves are used.
"""
import pathlib,os,subprocess,json,time,re
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/shortcuts'/str(time.time_ns());work.mkdir(parents=True)
codes={name:int(code) for name,code in re.findall(r'^#define\s+KEY_(\w+)\s+(\d+)\b',pathlib.Path('/usr/include/linux/input-event-codes.h').read_text(),re.M)}
checks=[]

def tap(name):
    return f'evkey {codes[name]} 1\nevkey {codes[name]} 0\n'

def ctrl(name):
    return f'evkey {codes["LEFTCTRL"]} 1\n'+tap(name)+f'evkey {codes["LEFTCTRL"]} 0\n'

def run(name,commands,story='lantern.z4',size=20):
    folder=work/name;folder.mkdir();config=folder/'config/frotz-zero';config.mkdir(parents=True)
    (config/'settings.conf').write_text(f'text_size={size}\nmute=1\n')
    drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(config.parent),XDG_DATA_HOME=str(folder/'data'))
    result=subprocess.run([str(root/'build/desktop/frotz-zero'),'--engine',str(root/'build/fixtures'/story),'--drive',str(drive)],env=env,capture_output=True,timeout=20)
    (folder/'engine.log').write_bytes(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.returncode,result.stderr.decode(errors='replace'))
    report=json.loads(next((folder/'data').rglob('drive-report.json')).read_text())
    return report,folder

r,folder=run('size-history','wait 200\ntext north\nkey Return\ntext draft\ncapture {folder}/draft.png\n'+
    ctrl('P')+'capture {folder}/previous.png\n'+ctrl('N')+'capture {folder}/next.png\n'+
    ctrl('U')+'wait 1600\ncapture {folder}/large.png\n'+ctrl('D')+'wait 1600\ncapture {folder}/small.png\n'+
    ''.join(tap(c) for c in 'UDPNFXZC')+'quit\n')
assert r['reading_size']==20 and r['draft']=='draftudpnfxzc' and 'Turns: 1. Score: 1.' in r['transcript'],r
assert Image.open(folder/'draft.png').tobytes()!=Image.open(folder/'previous.png').tobytes()
assert Image.open(folder/'draft.png').tobytes()==Image.open(folder/'next.png').tobytes()
assert Image.open(folder/'large.png').tobytes()!=Image.open(folder/'small.png').tobytes()
checks.append('Linux Ctrl+U/D changes size; Ctrl+P/N retrieves history then the draft; released plain letters remain story input')

r,folder=run('pan','wait 200\ntext draft\n'+ctrl('F')*16+ctrl('Z')+ctrl('C')*2+ctrl('X')*3+ctrl('Z')+ctrl('F')+'quit\n')
assert r['pan_x']==24 and r['pan_y']==48 and r['draft']=='draft',r
checks.append('Linux Ctrl+F/X/Z/C pans all four directions without Fn, Sym or Shift and without changing the draft')

r,folder=run('menu-help','wait 200\ntext draft\ncapture {folder}/before.png\n'+ctrl('ESC')+'capture {folder}/menu.png\n'+tap('ENTER')+
    tap('HELP')+'capture {folder}/help.png\n'+tap('ENTER')+tap('PAGEDOWN')+tap('ESC')+tap('ESC')+'capture {folder}/after.png\nquit\n')
assert r['draft']=='draft',r
assert Image.open(folder/'menu.png').tobytes()!=Image.open(folder/'help.png').tobytes()
assert Image.open(folder/'before.png').tobytes()==Image.open(folder/'after.png').tobytes()
checks.append('Linux Ctrl+Esc opens the story menu; firmware Help opens the topic list; paging and returning preserve the exact story view and draft')

r,folder=run('pointer','wait 1000\ntext a\nwait 500\n'+ctrl('SPACE')+tap('RIGHT')+tap('ENTER')+tap('ESC')+ctrl('0')+'text q\n',story='graphics.z6',size=12)
assert 'Pointer: 165,86.' in r['transcript'] and 'Sound callbacks: 1.' in r['transcript'],r
checks.append('Linux Ctrl+0 and Ctrl+Space retain overview/pointer; firmware arrow and Enter events click the correct coordinates')

result={'passed':len(checks),'checks':checks,'scope':'Compiled application using the native Linux keycode/xkb translator; no physical key-press claim'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n')
(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
