#!/usr/bin/env python3
"""Exercise universal Automatic presentation through the real interpreter."""
import hashlib,json,os,pathlib,subprocess,time,shutil
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/automatic'/str(time.time_ns());work.mkdir(parents=True)
story=root/'build/fixtures/automatic.z6';checks=[]

def run(name,commands,session=None,preference=None,theme='dark',extra=()):
    folder=work/name;folder.mkdir()
    env=session or dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
    config=pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero';config.mkdir(parents=True,exist_ok=True)
    if not session:(config/'settings.conf').write_text(f'text_size=18\nmute=1\ntheme={theme}\n')
    if preference is not None:
        data=pathlib.Path(env['XDG_DATA_HOME'])/'frotz-zero/stories'/hashlib.sha256(story.read_bytes()).hexdigest();data.mkdir(parents=True)
        (data/'display.conf').write_text(preference)
    drive=folder/'drive';drive.write_text('wait 100\n'+commands.replace('{folder}',str(folder))+'quit\n')
    if '--resume' in extra:
        incoming=pathlib.Path(env['XDG_DATA_HOME'])/'frotz-zero/stories'/hashlib.sha256(story.read_bytes()).hexdigest()/'resume'
        generations=sorted(incoming.glob('gen-*'))
        if generations:shutil.copyfile(generations[-1]/'frontend.txt',folder/'incoming-frontend.txt')
    p=subprocess.run([str(root/'build/desktop/frotz-zero'),'--engine',str(story),'--drive',str(drive),*extra],env=env,capture_output=True,timeout=18)
    (folder/'log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,(name,p.stderr)
    report=json.loads(next(pathlib.Path(env['XDG_DATA_HOME']).rglob('drive-report.json')).read_text())
    (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return report,folder,env

def pixels(folder,name):
    im=Image.open(folder/name);assert im.size==(320,170);return im.tobytes()

r,art,_=run('artwork','capture {folder}/art.png\n')
assert r['view_mode']=='Automatic' and not r['reader_active'] and (r['source_width'],r['source_height'],r['target_width'],r['target_height'])==(640,400,272,170),r
r,fit,_=run('fit','capture {folder}/art.png\n',preference='mode=1\nautomatic_version=2\n')
assert pixels(art,'art.png')==pixels(fit,'art.png')
checks.append('Automatic artwork is pixel-identical to Fit, with the full canvas and original proportions')

for theme in ('dark','light'):
    r,folder,_=run('question-'+theme,'key Return\ncapture {folder}/question.png\nkey ctrl+u\nwait 1400\ncapture {folder}/larger.png\n',theme=theme)
    assert r['reader_active'] and r['input_kind']=='character' and r['reading_size']==20 and 'THE ADAPTIVE' in r['reader_visible'] and 'Would you like to begin?' in r['transcript'],r
    assert 'Answer code' not in r['transcript'] and pixels(folder,'question.png')!=pixels(folder,'larger.png')
checks.append('Non-main-window questions use readable text in both themes; resizing neither skips nor answers them')

r,_,_=run('choice-letter','key Return\ntext n\n')
assert r['reader_active'] and r['input_kind']=='line' and 'Answer code: 110.' in r['transcript'],r
r,_,_=run('choice-arrow','key Return\nkey Up\n')
assert 'Answer code: 129.' in r['transcript'],r
r,_,_=run('choice-escape','key Return\nkey Escape\n')
assert 'Answer code: 27.' in r['transcript'],r
checks.append('Character-menu letters, arrows and Escape reach the story exactly once')

r,folder,_=run('paging-and-art','key Return\ntext n\nkey Return\ncapture {folder}/page.png\nkey Tab\ncapture {folder}/original.png\nkey Tab\ncapture {folder}/page-again.png\ntext draft\nkey Tab\nkey Tab\n')
assert pixels(folder,'page.png')==pixels(folder,'page-again.png')!=pixels(folder,'original.png')
assert r['reader_active'] and r['draft']=='draft' and 'Command received' not in r['transcript'],r
checks.append('Reading pages and Tab artwork toggle preserve position and draft without advancing the story')

r,folder,_=run('later-art','key Return\ntext n\ntext art\nkey Return\ncapture {folder}/art.png\n')
assert not r['reader_active'] and r['input_kind']=='character' and pixels(art,'art.png')==pixels(folder,'art.png'),r
checks.append('A later artwork-only pause returns to full fitted graphics despite earlier transcript text')

r,folder,_=run('timed','key Return\ntext n\ntext timed\nkey Return\nwait 100\ncapture {folder}/timed.png\n')
assert r['reader_active'] and r['input_timeout']==20 and r['input_kind']=='character',r
r,_,_=run('timed-expiry','key Return\ntext n\ntext timed\nkey Return\nwait 2400\n')
assert r['input_kind']=='line' and 'Timer result: 0.' in r['transcript'],r
r,_,_=run('timed-key','key Return\ntext n\ntext timed\nkey Return\nwait 100\nkey Return\n')
assert r['input_kind']=='line' and 'Timer result: 13.' in r['transcript'],r
checks.append('Timed text remains readable; timer expiry and immediate story keys retain their original semantics')

r,_,_=run('legacy-zoom','',preference='mode=5\n')
assert r['view_mode']=='Automatic',r
r,_,env=run('manual-choice','key ctrl+v\nkey Home\nkey Down\nkey Down\nkey Return\nwait 1400\n')
assert r['view_mode']=='Fill screen',r
r,_,_=run('manual-retained','',session=env)
assert r['view_mode']=='Fill screen',r
checks.append('Legacy V6 zoom adopts Automatic; explicit new manual choices persist across launches')

for version in (3,4,5,7,8):
    story=root/f'build/fixtures/lantern.z{version}'
    r,opening,env=run(f'v{version}-opening','capture {folder}/opening.png\n',preference='mode=5\nautomatic_version=1\n')
    assert r['view_mode']=='Automatic' and r['reader_active'] and r['reading_size']==18 and 'THE LANTERN ROOM' in r['reader_visible'],r
    assert r['input_kind']=='line' and r['safe_resume'],r
    r,before,env=run(f'v{version}-play','text north\nkey Return\ntext draft\nkey ctrl+u\nwait 1400\ncapture {folder}/text.png\n',session=env)
    assert r['view_mode']=='Automatic' and r['reader_active'] and r['reading_size']==20 and r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript'],r
    r,after,_=run(f'v{version}-continue','capture {folder}/text.png\n',session=env,extra=('--resume',))
    assert pixels(before,'text.png')==pixels(after,'text.png') and r['draft']=='draft',r
    r,original,_=run(f'v{version}-original','key Tab\ncapture {folder}/original.png\n',session=env,extra=('--resume',))
    assert not r['reader_active'] and r['view_mode']=='Automatic' and r['draft']=='draft',r
    r,restored,_=run(f'v{version}-original-continue','capture {folder}/original.png\nkey Tab\n',session=env,extra=('--resume',))
    assert pixels(original,'original.png')==pixels(restored,'original.png') and r['reader_active'] and r['draft']=='draft',r
    checks.append(f'V{version}: Automatic default, selected/live reading size, one turn, draft and pixel-identical Continue in text and original-layout views')

# A later explicit display choice belongs to the story, even when a named
# checkpoint was made with a different mode. Its VM state must remain intact.
data=pathlib.Path(env['XDG_DATA_HOME'])/'frotz-zero/stories'/hashlib.sha256(story.read_bytes()).hexdigest()
named=data/'saves/prior-view.fzsave';shutil.copytree(data/'resume',named)
r,_,_=run('later-manual','key ctrl+v\nkey Home\nkey Down\nkey Return\nwait 1400\n',session=env,extra=('--resume',))
assert r['view_mode']=='Fit screen',r
r,_,_=run('load-prior-view','',session=env,extra=('--load',str(named)))
assert r['view_mode']=='Fit screen' and r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript'],r
r,_,_=run('universal-toggle','key ctrl+0\nwait 1400\n',session=env,extra=('--resume',))
assert r['view_mode']=='Automatic' and r['reader_active'],r
checks.append('Explicit manual choice survives older checkpoint loading; Ctrl+0 returns text games to Automatic')

for version in (5,7,8):
    story=root/f'build/fixtures/text-modes.z{version}'
    r,_,_=run(f'v{version}-upper-menu','')
    assert r['reader_active'] and r['input_kind']=='character' and 'THE TEXT OBSERVATORY' in r['transcript'] and 'ABCD' not in r['transcript'] and 'STATUS HEADER' not in r['transcript'],r
    r,_,_=run(f'v{version}-timed','text n\nwait 100\n')
    assert r['reader_active'] and r['input_timeout']==20 and 'Menu answer: 110.' in r['transcript'],r
    r,_,_=run(f'v{version}-timer-expiry','text n\nwait 2400\n')
    assert r['reader_active'] and r['input_kind']=='line' and 'Timer result: 0.' in r['transcript'],r
    checks.append(f'V{version}: upper-panel menu and timed question are readable; status strips and font3 symbols stay in the original view')

story=root/'build/fixtures/pagination.z3'
r,_,_=run('more-boundary','text b\nkey Return\n')
assert r['input_kind']=='character' and r['reader_active'] and not r['safe_resume'] and 'LONG LINE' in r['transcript'],r
r,_,_=run('more-completed','text b\nkey Return\n'+('key End\nkey Return\n'*12))
assert r['input_kind']=='line' and r['safe_resume'] and 'LONG OUTPUT COMPLETE' in r['transcript'],r
checks.append('V3 interpreter More pages remain readable; checkpoints are unavailable mid-output and resume at the next complete command boundary')

result={'passed':len(checks),'checks':checks,'screenshots':str(work),'scope':'Real V3-V8 interpreter, original CC0 artwork/text, isolated data, 320x170 renderer'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
