#!/usr/bin/env python3
"""Named-save management with original fixtures and isolated user storage."""
import pathlib,os,subprocess,json,time,shutil,hashlib
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/save-management'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero';checks=[]

def run(name,commands,env,story=None,stories=None,extra=(),expected=0):
    folder=work/name;folder.mkdir();drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    args=['--engine',str(story)] if story else ['--stories',str(stories)]
    p=subprocess.run([str(binary),*args,'--drive',str(drive),*extra],env=env,capture_output=True,timeout=20)
    (folder/'log').write_bytes(p.stdout+p.stderr);assert p.returncode==expected,(name,p.returncode,p.stderr.decode(errors='replace'))
    return folder

def hashes(folder):return {str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.rglob('*') if p.is_file()}

open_saves='wait 100\ntext 2\nkey Right\ntext 3\n'
rename='text m\ntext 2\nkey Home\n'+'key Delete\n'*48
for theme in ('dark','light'):
    base=work/theme;base.mkdir();stories=base/'stories';stories.mkdir();story=stories/'Lantern.z5';shutil.copyfile(root/'build/fixtures/lantern.z5',story)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(base/'config'),XDG_DATA_HOME=str(base/'data'))
    config=base/'config/frotz-zero';config.mkdir(parents=True);(config/'settings.conf').write_text('theme='+theme+'\nmute=1\n')
    run('setup-'+theme,'wait 150\ntext north\nkey Return\ntext save\nkey Return\ntext portable\nkey Return\nwait 150\nkey ctrl+Escape\nkey Home\nkey Down\nkey Return\ntext chapter\nkey Return\nkey Return\ntext draft\nquit\n',env,story=story)
    data=next((base/'data/frotz-zero/stories').iterdir());saves=data/'saves';automatic=hashes(data/'resume')
    portable=(saves/'portable.qzl').read_bytes();checkpoint=hashes(saves/'chapter.fzsave')
    shutil.copyfile(saves/'portable.qzl',saves/'occupied.qzl')
    # Some mounted filesystems round copied timestamps to whole seconds.
    # Keep fixture dates distinct so the intended rows remain unambiguous.
    stamp=int(time.time())
    for name,offset in [('portable.qzl',0),('chapter.fzsave',10),('occupied.qzl',20)]:os.utime(saves/name,(stamp+offset,stamp+offset))
    folder=run('save-list-'+theme,open_saves+'capture {folder}/list.png\nkey End\ntext m\ntext 3\ncapture {folder}/confirm.png\nkey Return\ncapture {folder}/cancelled.png\nquit\n',env,stories=stories)
    assert (saves/'portable.qzl').read_bytes()==portable and hashes(data/'resume')==automatic
    run('rename-collision-'+theme,open_saves+'key End\n'+rename+'text occupied\nkey Return\ncapture {folder}/refused.png\nkey Return\nquit\n',env,stories=stories)
    assert (saves/'portable.qzl').read_bytes()==portable and (saves/'occupied.qzl').read_bytes()==portable
    run('rename-portable-'+theme,open_saves+'key End\n'+rename+'text Travel\nkey Return\ncapture {folder}/result.png\nkey Return\ncapture {folder}/renamed.png\nquit\n',env,stories=stories)
    assert not (saves/'portable.qzl').exists() and (saves/'Travel.qzl').read_bytes()==portable
    run('rename-checkpoint-'+theme,open_saves+'key Home\nkey Down\n'+rename+'text Chapter two\nkey Return\nkey Return\ncapture {folder}/renamed.png\nquit\n',env,stories=stories)
    assert not (saves/'chapter.fzsave').exists() and hashes(saves/'Chapter two.fzsave')==checkpoint
    assert hashes(data/'resume')==automatic
    checks.append(theme+': save names, dates and types; Cancel, rename without overwrite, and byte-identical Quetzal/checkpoint contents')
    # Exercise the existing quick-load path through the in-game save picker.
    for kind,keys,filename in [('quetzal','key End\n','Travel.qzl'),('checkpoint','key Home\nkey Down\n','Chapter two.fzsave')]:
        loadbase=work/(theme+'-load-data-'+kind);shutil.copytree(base/'data',loadbase)
        loadenv=dict(env,XDG_DATA_HOME=str(loadbase));loadfolder=loadbase/'frotz-zero/stories'/data.name
        run(theme+'-choose-'+kind,'wait 100\nkey ctrl+Escape\nkey Home\nkey Down\nkey Down\nkey Return\n'+keys+'key Return\n',loadenv,story=story,extra=('--resume',),expected=77)
        target=pathlib.Path((loadfolder/'launch-request').read_text());assert target.name==filename
        run(theme+'-load-'+kind,'wait 100\ntext look\nkey Return\nquit\n',loadenv,story=story,extra=('--load',str(target)))
        report=json.loads((loadfolder/'working/drive-report.json').read_text());assert 'Turns: 2. Score: 1.' in report['transcript'],report
    checks.append(theme+': renamed Quetzal and checkpoint both load through the picker and actual interpreter')
    run('delete-portable-'+theme,open_saves+'key End\ntext m\ntext 3\ntext 2\nkey Return\ncapture {folder}/remaining.png\nquit\n',env,stories=stories)
    assert not (saves/'Travel.qzl').exists() and (saves/'occupied.qzl').read_bytes()==portable
    run('delete-checkpoint-'+theme,open_saves+'key End\ntext m\ntext 3\ntext 2\nkey Return\nquit\n',env,stories=stories)
    assert not (saves/'Chapter two.fzsave').exists() and hashes(data/'resume')==automatic
    assert (saves/'occupied.qzl').read_bytes()==portable
    checks.append(theme+': confirmed named-file and checkpoint-directory deletion preserve other saves and automatic Continue')
result={'passed':len(checks),'checks':checks,'screenshots':str(work),'scope':'Original CC0 story, real interpreter and save UI, isolated named saves and automatic Continue'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
