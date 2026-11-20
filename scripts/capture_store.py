#!/usr/bin/env python3
"""Capture actual UI surfaces with only original CC0 demonstration content."""
import pathlib,subprocess,os,shutil,json,hashlib,time
root=pathlib.Path(__file__).resolve().parents[1];out=root/'store/screenshots';out.mkdir(parents=True,exist_ok=True)
work=root/'local/store-capture';work.mkdir(parents=True,exist_ok=True);stories=work/'stories';stories.mkdir(exist_ok=True)
for source,dest in [('lantern.z5','The Lantern Room.z5'),('graphics.z6','Graphics and Sound.z6'),('graphics.blb','Graphics and Sound.blb')]:shutil.copyfile(root/'build/fixtures'/source,stories/dest)
binary=root/'build/desktop/frotz-zero'
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',XDG_CONFIG_HOME=str(work/'config'),XDG_DATA_HOME=str(work/'data'))
drive=work/'library.drive';drive.write_text('wait 200\ncapture '+str(out/'home.png')+'\nkey Return\nkey End\nwait 200\ncapture '+str(out/'library.png')+'\nkey Right\ncapture '+str(out/'story.png')+'\nquit\n')
subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive)],env=env,check=True,timeout=10)
drive=work/'lantern.drive';drive.write_text('wait 200\ntext look\nkey Return\nwait 200\nquit\n')
subprocess.run([str(binary),'--engine',str(stories/'The Lantern Room.z5'),'--drive',str(drive),'--capture',str(out/'lantern.png')],env=env,check=True,timeout=10)
config=work/'light-config/frotz-zero';config.mkdir(parents=True,exist_ok=True);(config/'settings.conf').write_text('theme=light\n')
drive=work/'settings.drive';drive.write_text('wait 200\ntext 4\nwait 200\ncapture '+str(out/'settings-light.png')+'\nquit\nkey Escape\n')
subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive)],env=dict(env,XDG_CONFIG_HOME=str(config.parent)),check=True,timeout=10)
online=work/('online-'+str(time.time_ns()));(online/'source/gamefiles').mkdir(parents=True)
payload=(root/'build/fixtures/lantern.z5').read_bytes();(online/'source/gamefiles/lantern.z5').write_bytes(payload)
catalog=[{'dir':'gamefiles','type':'z5','title':'The Lantern Room','release':str(int.from_bytes(payload[2:4],'big')),'serial':payload[18:24].decode(),'md5':hashlib.md5(payload).hexdigest(),'filename':'lantern.z5','comment':'Explore a small room, visit the garden and try saving your adventure. An original CC0 demonstration.'}]
(online/'source/catalog.json').write_text(json.dumps(catalog))
online_env=dict(env,XDG_DATA_HOME=str(online/'data'))
drive=online/'release.drive';drive.write_text('wait 200\ntext 3\nkey Right\nkey Return\nwait 300\nkey Return\nkey Return\nwait 100\ncapture '+str(out/'online-release.png')+'\nquit\n')
subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive),'--online-fixture',str(online/'source')],env=online_env,check=True,timeout=10)
drive=online/'manual.drive';drive.write_text('wait 200\ntext 3\nkey Right\nkey Down\nkey Down\nkey Down\nkey Return\nkey Down\nkey Return\nwait 100\ncapture '+str(out/'manual-link.png')+'\nquit\n')
subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive)],env=dict(online_env,XDG_CONFIG_HOME=str(config.parent)),check=True,timeout=10)
drive=work/'about.drive';drive.write_text('wait 200\ntext 5\nkey Down\nkey Return\nwait 100\ncapture '+str(out/'about.png')+'\nquit\n')
subprocess.run([str(binary),'--stories',str(stories),'--drive',str(drive)],env=env,check=True,timeout=10)
print('Captured actual public fixture screens:',out)
