#!/usr/bin/env python3
"""Exercise the actual Help/About/credits paths, scrolling and exit behaviour."""
import json
import os
import pathlib
import subprocess
import time
from PIL import Image

root=pathlib.Path(__file__).resolve().parents[1]
work=root/'local/credits'/str(time.time_ns());work.mkdir(parents=True)
binary=root/'build/desktop/frotz-zero'
checks=[]

def run(name,commands,theme='dark'):
    folder=work/name;folder.mkdir()
    config=folder/'config/frotz-zero';config.mkdir(parents=True)
    (config/'settings.conf').write_text(f'theme={theme}\n')
    drive=folder/'drive';drive.write_text(commands.replace('{folder}',str(folder)))
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(config.parent),XDG_DATA_HOME=str(folder/'data'))
    result=subprocess.run([str(binary),'--stories',str(root/'build/fixtures'),'--drive',str(drive)],env=env,capture_output=True,timeout=20)
    (folder/'output.log').write_bytes(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.stderr.decode(errors='replace'))
    return folder

def pixels(folder,name):
    im=Image.open(folder/f'{name}.png')
    assert im.size==(320,170)
    return im.tobytes()

for theme in ('dark','light'):
    folder=run(theme,'''wait 100
capture {folder}/home.png
key Down
key Right
capture {folder}/home-help.png
key Return
capture {folder}/help.png
key Down
key Return
capture {folder}/about.png
key Return
capture {folder}/credits.png
key Return
capture {folder}/project.png
key Escape
key Down
key Return
capture {folder}/contributors.png
key End
capture {folder}/contributors-end.png
key Home
capture {folder}/contributors-home.png
key Escape
key Down
key Return
capture {folder}/libraries.png
key Escape
key Down
key Return
capture {folder}/licence.png
# Native Fn+M and Fn+N output KEY_PAGEDOWN and KEY_END.
evkey 109 1
evkey 109 0
capture {folder}/licence-page.png
evkey 107 1
evkey 107 0
capture {folder}/licence-end.png
key PageDown
capture {folder}/licence-clamped.png
key Home
capture {folder}/licence-home.png
key Escape
key Escape
capture {folder}/about-return.png
key Escape
key Escape
capture {folder}/home-return.png
quit
''',theme)
    assert len({pixels(folder,n) for n in ('home','help','about','credits','project','contributors','libraries','licence')})==8
    assert pixels(folder,'about')==pixels(folder,'about-return')
    assert pixels(folder,'home-help')==pixels(folder,'home-return')
    assert pixels(folder,'contributors')!=pixels(folder,'contributors-end')
    assert pixels(folder,'contributors')==pixels(folder,'contributors-home')
    assert len({pixels(folder,n) for n in ('licence','licence-page','licence-end')})==3
    assert pixels(folder,'licence')==pixels(folder,'licence-home')
    assert pixels(folder,'licence-end')==pixels(folder,'licence-clamped')
    checks.append(f'{theme}: all credit pages reachable; Escape restores About and home exactly')
    checks.append(f'{theme}: long credits and GPL licence scroll/page/end/home correctly with native keycodes')

# Closing from a nested reader must propagate through both menu loops.
run('nested-quit','text 5\nkey Down\nkey Down\nkey Return\nkey Down\nkey Return\nquit\n')
checks.append('Quit exits directly from a nested credit reader without extra keypresses')

for theme in ('dark','light'):
    commands='wait 100\ntext 5\ncapture {folder}/help.png\nkey Return\n'
    for topic in range(5):
        commands+='key Home\n'+'key Down\n'*topic
        commands+=f'capture {{folder}}/selected-{topic}.png\nkey Right\ncapture {{folder}}/topic-{topic}-first.png\n'
        # Native PageDown/PageUp, then Home/End and number shortcuts.
        commands+='evkey 109 1\nevkey 109 0\n'+f'capture {{folder}}/topic-{topic}-second.png\n'
        commands+='evkey 104 1\nevkey 104 0\n'+f'capture {{folder}}/topic-{topic}-again.png\n'
        commands+='key End\n'+f'capture {{folder}}/topic-{topic}-last.png\n'
        commands+='key Down\n'+f'capture {{folder}}/topic-{topic}-clamped.png\n'
        commands+='text 1\n'+f'capture {{folder}}/topic-{topic}-number.png\n'
        # Capture all pages for visual review, retaining boundary checks above.
        for page in range(1,7):
            commands+=f'text {page}\ncapture {{folder}}/topic-{topic}-page-{page}.png\n'
        commands+='key Left\n'+f'capture {{folder}}/returned-{topic}.png\n'
    commands+='key Home\ntext 2\ncapture {folder}/number-open.png\nkey Return\nkey Escape\ncapture {folder}/help-return.png\nquit\n'
    folder=run('controls-'+theme,commands,theme)
    for topic in range(5):
        assert pixels(folder,f'topic-{topic}-first')==pixels(folder,f'topic-{topic}-again')==pixels(folder,f'topic-{topic}-number')
        assert pixels(folder,f'topic-{topic}-first')!=pixels(folder,f'topic-{topic}-second')
        assert pixels(folder,f'topic-{topic}-last')==pixels(folder,f'topic-{topic}-clamped')
        assert pixels(folder,f'selected-{topic}')==pixels(folder,f'returned-{topic}')
    assert pixels(folder,'number-open')==pixels(folder,'topic-1-first')
    assert pixels(folder,'help')==pixels(folder,'help-return')
    checks.append(f'{theme}: all five Controls topics open directly with arrows/numbers and return to the same topic; no forced sequence')
    checks.append(f'{theme}: Controls pages support native PageUp/PageDown, Home/End and numbers; page boundaries clamp without losing content')
run('controls-nested-quit','text 5\nkey Return\nkey Return\nquit\n')
checks.append('Quit from a Controls topic propagates directly through the topic and Help menus')
result={'passed':len(checks),'checks':checks,'screenshots':str(work),'scope':'Actual 320x170 SDL UI with isolated user data; physical legibility checked separately'}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n')
(work.parent/'results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
