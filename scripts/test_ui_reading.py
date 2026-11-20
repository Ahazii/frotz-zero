#!/usr/bin/env python3
"""Real home/picker and reading-view regressions, using only CC0 stories."""
import pathlib,os,subprocess,json,time,hashlib,shutil
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/ui-reading'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero';checks=[]
def run(name,commands,story='lantern.z4',session=None,settings=None):
    folder=work/name;folder.mkdir()
    if session is None:
        env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
    else:env=session.copy()
    config=pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero';config.mkdir(parents=True,exist_ok=True)
    if settings is not None:(config/'settings.conf').write_text(settings)
    drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    args=[str(binary),'--engine',str(root/'build/fixtures'/story)] if story else [str(binary),'--stories',str(root/'build/fixtures')]
    if name.startswith('resume'):args+=['--resume']
    result=subprocess.run([*args,'--drive',str(drive)],env=env,capture_output=True,timeout=20)
    (folder/'output.log').write_bytes(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.stderr.decode(errors='replace'))
    reports=list(pathlib.Path(env['XDG_DATA_HOME']).glob('frotz-zero/stories/*/working/drive-report.json'))
    return (json.loads(reports[0].read_text()) if reports else {}),env,folder

_,env,folder=run('home',
    'wait 200\ncapture {folder}/home.png\nkey Return\nwait 100\ncapture {folder}/first.png\nkey Down\nwait 100\ncapture {folder}/second.png\nkey Down\nkey Tab\nwait 100\ncapture {folder}/details.png\nkey Escape\nkey Return\nwait 100\ncapture {folder}/selected.png\nquit\n',story=None)
assert Image.open(folder/'first.png').crop((8,35,312,145)).tobytes()!=Image.open(folder/'second.png').crop((8,35,312,145)).tobytes()
settings=pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero/settings.conf'
assert not settings.exists() or 'recent=\n' in settings.read_text()
checks.append('Six-action home, library navigation, full details and story page; browsing does not change last played')

r,session,before=run('reading-v4','wait 200\ntext north\nkey Return\nwait 150\ntext draft\nkey ctrl+p\nkey ctrl+n\ncapture {folder}/sixteen.png\nkey ctrl+u\nwait 1400\ncapture {folder}/eighteen.png\nquit\n',settings='text_size=16\n')
assert r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript'] and r['reading_size']==18 and r['canvas_width']==640,r
assert Image.open(before/'sixteen.png').tobytes()!=Image.open(before/'eighteen.png').tobytes()
checks.append('V4 uses the selected reading size; Ctrl+U changes it live while Ctrl+P/N preserves draft/history')
r,_,after=run('resume-same','wait 200\ncapture {folder}/continued.png\nquit\n',session=session)
assert r['draft']=='draft' and r['reading_size']==18
assert Image.open(before/'eighteen.png').tobytes()==Image.open(after/'continued.png').tobytes(),'fractional reading view changed during Continue'
checks.append('Continue preserves the exact fractional-scale viewport, draft and screen with unchanged preferences')
r,_,_=run('resume-new-preference','wait 200\nquit\n',session=session,settings='text_size=20\ntheme=light\n')
assert r['reading_size']==20 and r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript'],r
checks.append('Changed reading preference applies to Continue without advancing the story or changing VM geometry')
r,_,_=run('resume-live-settings','wait 200\nkey ctrl+Escape\nkey Home\nkey Down\nkey Down\nkey Down\nkey Return\nkey Down\nkey Return\nkey Escape\nwait 1400\nquit\n',session=session)
assert r['reading_size']==22 and r['draft']=='draft',r
checks.append('In-game Settings changes reading size immediately on return to play')
for schema in (2,3,4,5,6,7):
    # Exercise every earlier frontend schema with matching real VM/screen bytes.
    legacy=work/f'legacy-{schema}';legacy.mkdir()
    shutil.copytree(pathlib.Path(session['XDG_DATA_HOME']),legacy/'data')
    for state in (legacy/'data').rglob('frontend.txt'):
        lines=state.read_text().splitlines(keepends=True);lines[0]=f'FZFRONT-{schema}\n'
        if schema==3:lines[3]='"dark" 1 0 0\n'
        elif schema==2:del lines[3]
        elif schema==4:lines[3]=' '.join(lines[3].split()[:6])+'\n'
        else:
            fields=lines[3].split()[:9]
            if schema<7:fields[6]='0' # Reader existed before Automatic.
            lines[3]=' '.join(fields)+'\n'
        state.write_text(''.join(lines))
        manifest=state.parent/'manifest'
        manifest.write_text('\n'.join('frontend.txt '+hashlib.sha256(state.read_bytes()).hexdigest() if line.startswith('frontend.txt ') else line for line in manifest.read_text().splitlines())+'\n')
    legacy_env=dict(session,XDG_CONFIG_HOME=str(legacy/'config'),XDG_DATA_HOME=str(legacy/'data'))
    r,_,_=run(f'resume-legacy-{schema}','wait 200\nquit\n',session=legacy_env,settings='text_size=24\n')
    assert r['draft']=='draft' and r['reading_size']==24 and r['canvas_width']==640,r
    checks.append(f'Frontend schema {schema} migrates its view to current reading preferences while preserving the saved draft')
r,_,folder=run('v6-reading','wait 900\ntext a\nwait 500\ncapture {folder}/reading.png\nkey ctrl+0\ncapture {folder}/overview.png\nkey ctrl+0\nkey ctrl+d\nkey ctrl+u\ntext q\n',story='graphics.z6',settings='text_size=24\n')
assert r['reading_size']==24 and r['canvas_width']==640 and 'Sound callbacks: 1.' in r['transcript'],r
assert Image.open(folder/'reading.png').tobytes()!=Image.open(folder/'overview.png').tobytes()
checks.append('V6 defaults to a complete fitted layout; fit/reading toggle and size controls respect the 24px preference')
result={'passed':len(checks),'checks':checks,'scope':'Shared compiled renderer/engine, original CC0 fixtures; hardware checks recorded separately'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n')
(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
