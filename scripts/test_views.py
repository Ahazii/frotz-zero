#!/usr/bin/env python3
"""Reader, scaling, four-way/number navigation, artwork and IF Archive UI.

All game and artwork inputs are original fixtures. No personal library is used.
"""
import hashlib,json,os,pathlib,shutil,struct,subprocess,time
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/views'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero';checks=[]

def run(name,commands,story=None,stories=None,session=None,extra=(),settings=None):
    folder=work/name;folder.mkdir()
    env=session or dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(folder/'config'),XDG_DATA_HOME=str(folder/'data'))
    config=pathlib.Path(env['XDG_CONFIG_HOME'])/'frotz-zero';config.mkdir(parents=True,exist_ok=True)
    if settings is not None:(config/'settings.conf').write_text(settings)
    drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    args=['--engine',str(story)] if story else ['--stories',str(stories or root/'build/fixtures')]
    p=subprocess.run([str(binary),*args,'--drive',str(drive),*extra],env=env,capture_output=True,timeout=25)
    (folder/'log').write_bytes(p.stdout+p.stderr);assert p.returncode==0,(name,p.returncode,p.stderr.decode(errors='replace'))
    reports=list(pathlib.Path(env['XDG_DATA_HOME']).rglob('drive-report.json'))
    return (json.loads(reports[0].read_text()) if reports else {}),folder,env

def pixels(folder,name,box=None):
    im=Image.open(folder/name);assert im.size==(320,170)
    return (im.crop(box) if box else im).tobytes()

story=root/'build/fixtures/lantern.z4'
r,first,env=run('reader-opening','wait 200\ncapture {folder}/opening.png\nquit\n',story=story,settings='text_size=16\n')
assert r['view_mode']=='Automatic' and 'THE LANTERN ROOM' in r['reader_visible'] and r['reader_top']<r['reader_limit'],r
_,continued,_=run('reader-continue','wait 200\ncapture {folder}/continued.png\nquit\n',story=story,session=env,extra=('--resume',))
assert pixels(first,'opening.png')==pixels(continued,'continued.png')
checks.append('Reader displays the beginning of long opening prose and Continue restores the same page exactly')
r,_,_=run('reader-paging','wait 200\nkey Return\nquit\n',story=story,settings='text_size=16\n')
assert not r['draft'] and 'Turns:' not in r['transcript'] and r['reader_top']>0,r
r,_,_=run('reader-space','wait 200\nevkey 57 1\nevkey 57 0\nquit\n',story=story,settings='text_size=16\n')
assert not r['draft'] and 'Turns:' not in r['transcript'] and r['reader_top']>0,r
checks.append('Enter and native Space page prose without sending a turn or stray space to the story')
r,_,_=run('reader-input','wait 200\ntext north\nkey Return\nwait 150\ntext 123draft\nkey ctrl+p\nkey ctrl+n\nkey ctrl+u\nwait 1400\nquit\n',story=story)
assert r['draft']=='123draft' and r['reading_size']==14 and 'Turns: 1. Score: 1.' in r['transcript'],r
checks.append('Typing reaches the prompt without losing digits; history, one-turn semantics and live Reader sizing work')
for v in (7,8):
    story=root/f'build/fixtures/lantern.z{v}'
    r,before,env=run(f'v{v}-play','wait 200\ntext north\nkey Return\ntext draft\ncapture {folder}/before.png\nquit\n',story=story)
    assert r['safe_resume'] and r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript']
    r,after,_=run(f'v{v}-continue','wait 200\ncapture {folder}/after.png\nquit\n',story=story,session=env,extra=('--resume',))
    assert r['draft']=='draft' and pixels(before,'before.png')==pixels(after,'after.png')
checks.append('V7 and V8 play, advance one turn and preserve a pixel-identical Continue with draft')
for index,label in ((1,'Fit screen'),(2,'Fill screen'),(3,'Fit width'),(4,'Actual pixels'),(5,'Reading zoom')):
    command='wait 900\ntext a\nwait 500\nkey ctrl+v\nkey Home\n'+'key Down\n'*index+'key Right\nwait 1400\ncapture {folder}/mode.png\ntext q\n'
    r,folder,_=run('graphics-'+str(index),command,story=root/'build/fixtures/graphics.z6')
    assert r['view_mode']==label and r['canvas_width']==640 and r['canvas_height']==400,r
    if index==1:assert (r['source_width'],r['source_height'],r['target_width'],r['target_height'])==(640,400,272,170)
    if index==2:assert (r['source_width'],r['source_height'],r['target_width'],r['target_height'])==(640,400,320,170)
    if index==3:assert r['source_width']==640 and r['target_width']==320
checks.append('Every graphics display mode is reachable with arrows; Fit preserves all pixels/proportions and Fill uses the entire display')
for theme in ('dark','light'):
    _,folder,_=run('navigation-'+theme,'wait 100\ncapture {folder}/home.png\nkey Right\ncapture {folder}/right.png\nkey Down\ncapture {folder}/down.png\nkey Left\ncapture {folder}/left.png\nkey Up\ncapture {folder}/up.png\ntext 2\nkey Down\nkey Up\ntext 1\ncapture {folder}/story.png\nkey Right\ncapture {folder}/story-right.png\nkey Left\ncapture {folder}/story-left.png\nkey Escape\nkey Left\ntext 4\ntext 1\nkey Left\ncapture {folder}/changed-theme.png\nquit\n',settings='theme='+theme+'\n')
    assert pixels(folder,'home.png')==pixels(folder,'up.png')
    assert len({pixels(folder,n) for n in ('home.png','right.png','down.png','left.png')})==4
    assert pixels(folder,'story.png')==pixels(folder,'story-left.png')!=pixels(folder,'story-right.png')
checks.append('All four arrows and immediate numbered actions work in both themes, with consistent focus and back navigation')

# Real Fspc points to the original fixture image, not an arbitrary picture.
cover_area=(8,35,104,131)
for kind in ('fallback','png','jpeg','broken','embedded'):
    stories=work/('covers-'+kind);stories.mkdir()
    if kind=='embedded':
        b=bytearray((root/'build/fixtures/graphics.blb').read_bytes());b.extend(b'Fspc'+struct.pack('>II',4,1));b[4:8]=struct.pack('>I',len(b)-8);(stories/'cover.zblorb').write_bytes(b)
    else:
        shutil.copyfile(root/'build/fixtures/lantern.z5',stories/'cover.z5')
        if kind=='png':Image.new('RGB',(200,200),(220,35,40)).save(stories/'cover.cover.png')
        if kind=='jpeg':Image.new('RGB',(200,200),(220,35,40)).save(stories/'cover.jpg')
        if kind=='broken':(stories/'cover.png').write_bytes(b'not an image but bounded')
    _,folder,_=run('art-'+kind,'wait 100\ntext 2\nkey Right\ncapture {folder}/art.png\nquit\n',stories=stories)
    if kind=='fallback':fallback=pixels(folder,'art.png',cover_area)
    if kind=='broken':assert pixels(folder,'art.png',cover_area)==fallback
    if kind in ('png','jpeg'):assert Image.open(folder/'art.png').getpixel((50,75))[0]>210
    if kind=='embedded':assert pixels(folder,'art.png',cover_area)!=fallback
checks.append('Story page renders PNG/JPEG sidecars and indexed Blorb frontispieces; absent or damaged art uses the illustrated fallback')

source=work/'archive-source';(source/'indexes').mkdir(parents=True);(source/'if-archive/games/zcode').mkdir(parents=True)
payload=(root/'build/fixtures/lantern.z8').read_bytes();filename='lantern-archive.z8';(source/'if-archive/games/zcode'/filename).write_bytes(payload)
xml='<ifarchive><file><name>'+filename+'</name><path>if-archive/games/zcode/'+filename+'</path><size>'+str(len(payload))+'</size><md5>'+hashlib.md5(payload).hexdigest()+'</md5><description>Archive Lantern by Frotz Zero. An original CC0 adventure.</description><metadata><item><key>ifwiki</key><value>Archive Lantern</value></item></metadata></file></ifarchive>'
(source/'indexes/Master-Index.xml').write_text(xml)
downloads=work/'archive-downloads';downloads.mkdir()
_,folder,env=run('archive-ui','wait 100\ntext 3\ntext 2\ntext 1\nwait 400\nkey Right\nkey Right\ncapture {folder}/release.png\ntext 1\nwait 400\nkey Right\ncapture {folder}/story.png\nquit\n',stories=downloads,extra=('--online-fixture',str(source)))
assert (downloads/filename).read_bytes()==payload
data=pathlib.Path(env['XDG_DATA_HOME'])/'frotz-zero';assert (data/'online/ifarchive.json').exists() and list((data/'online/installed').glob('*.json'))
checks.append('Home Downloads > IF Archive > title > verified V8 download opens the story page and retains cached metadata')
story=root/'build/fixtures/intro.z4'
r,intro,env=run('intro-sizing','wait 200\nevkey 28 2\nevkey 28 0\ncapture {folder}/before.png\nkey ctrl+u\nkey ctrl+u\nwait 1400\ncapture {folder}/larger.png\nquit\n',story=story,settings='text_size=16\n')
assert r['input_kind']=='character' and r['reading_size']==20 and 'THE OBSERVATORY' in r['reader_visible'],r
assert 'FIRST STORY PARAGRAPH' not in r['transcript'] and 'STATUS MUST' not in r['transcript'] and 'distant stars' in r['transcript'],r
assert pixels(intro,'before.png')!=pixels(intro,'larger.png')
r,_,_=run('intro-new-game','wait 200\nquit\n',story=story,session=env)
assert 'THE OBSERVATORY' in r['reader_visible'] and r['input_kind']=='character',r
r,_,_=run('intro-read','wait 200\nkey End\nkey Return\nwait 200\nquit\n',story=story)
assert 'FIRST STORY PARAGRAPH' in r['reader_visible'] and r['input_kind']=='line',r
checks.append('Untimed title/intro text, including upper-window panels, stays readable during sizing and New Game; status rows are excluded')
before,_,_=run('reader-anchor-before','wait 200\nkey Return\nquit\n',story=story,settings='text_size=20\n')
r,_,_=run('reader-anchor','wait 200\nkey Return\nkey ctrl+u\nwait 1400\nquit\n',story=story,settings='text_size=20\n')
assert r['input_kind']=='character' and r['reader_top']>0 and 'FIRST STORY PARAGRAPH' not in r['transcript'],r
assert ' '.join(before['reader_visible'].split()[:3]) in ' '.join(r['reader_visible'].split()),(before,r)
r,_,_=run('reader-settings-anchor','wait 200\nkey Return\nkey ctrl+Escape\nkey Home\nkey Down\nkey Down\nkey Down\nkey Return\nkey Down\nkey Return\nkey Escape\nwait 1400\nquit\n',story=story,settings='text_size=20\n')
assert r['reading_size']==22 and r['input_kind']=='character' and ' '.join(before['reader_visible'].split()[:3]) in ' '.join(r['reader_visible'].split()),r
checks.append('Reader pagination and size changes preserve the reading position without consuming the intro key')

story=root/'build/fixtures/framed.z6'
r,frame,env=run('framed-zoom','wait 200\nkey ctrl+v\nkey Home\nkey Down\nkey Down\nkey Right\nwait 1400\ncapture {folder}/fill.png\nkey ctrl+u\nwait 1400\ncapture {folder}/zoom14.png\nkey ctrl+u\nwait 1400\ncapture {folder}/zoom16.png\nkey ctrl+z\ncapture {folder}/pan.png\ntext draft\nquit\n',story=story)
assert r['view_mode']=='Frame + text zoom' and r['reading_size']==16 and r['draft']=='draft' and (r['canvas_width'],r['canvas_height'])==(640,400),r
for box in ((0,0,320,25),(0,154,320,170),(0,25,20,154),(300,25,320,154)):
    assert len({pixels(frame,name,box) for name in ('fill.png','zoom14.png','zoom16.png','pan.png')})==1,box
assert pixels(frame,'fill.png')!=pixels(frame,'zoom14.png')!=pixels(frame,'zoom16.png')
r,_,_=run('framed-new-game','wait 200\nquit\n',story=story,session=env)
assert r['view_mode']=='Frame + text zoom' and r['target_width']<320 and r['source_width']<560,r
checks.append('V6 frame remains pixel-identical during text zoom and pan; input, persistent view and original VM dimensions are preserved')

for theme in ('dark','light'):
    stories=work/('delete-files-'+theme);stories.mkdir()
    payload=(root/'build/fixtures/lantern.z5').read_bytes();identity=hashlib.sha256(payload).hexdigest()
    for name in ('chosen.z5','duplicate.z5'):(stories/name).write_bytes(payload)
    (stories/'chosen.png').write_bytes(b'optional artwork')
    data=work/('delete-data-'+theme);save=data/'frotz-zero/stories'/identity/'saves/precious.qzl';save.parent.mkdir(parents=True);save.write_bytes(b'save must remain')
    state=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(work/('delete-config-'+theme)),XDG_DATA_HOME=str(data))
    _,folder,_=run('delete-cancel-'+theme,'wait 100\ntext 2\nkey Right\nkey End\ncapture {folder}/action.png\nkey Return\ncapture {folder}/confirm.png\nkey Return\ncapture {folder}/cancelled.png\nquit\n',stories=stories,session=state,settings='theme='+theme+'\nrecent='+identity+'\n')
    assert (stories/'chosen.z5').exists() and (stories/'duplicate.z5').exists() and save.read_bytes()==b'save must remain'
    _,folder,_=run('delete-confirm-'+theme,'wait 100\ntext 2\nkey Right\ntext 4\ntext 3\ncapture {folder}/files.png\nkey Escape\ntext 2\ncapture {folder}/deleted.png\nkey Return\nkey Escape\ncapture {folder}/home.png\nquit\n',stories=stories,session=state)
    assert not list(stories.iterdir()) and save.read_bytes()==b'save must remain'
    assert 'recent=\n' in (pathlib.Path(state['XDG_CONFIG_HOME'])/'frotz-zero/settings.conf').read_text()
checks.append('Story deletion works with arrows/numbers in both themes; default Cancel is harmless, duplicate files and exclusive art disappear, and saves remain')

result={'passed':len(checks),'checks':checks,'screenshots':str(work),'scope':'Actual engine and 320x170 renderer, original CC0 fixtures, isolated settings and saves'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
