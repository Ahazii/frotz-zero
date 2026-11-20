#!/usr/bin/env python3
"""Exercise dpkg install/reinstall/remove/purge inside an isolated owned root.
No host or device package database is modified; no application is executed.
"""
import pathlib,subprocess,tempfile,json,hashlib
root=pathlib.Path(__file__).resolve().parents[1]
version=json.loads((root/'app-builder.json').read_text())['version'];deb=root/'dist'/f'frotz-zero_{version}_arm64.deb'
work=root/'local/package-lifecycle';work.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='root-',dir=work) as tmp:
    stage=pathlib.Path(tmp);admin=stage/'var/lib/dpkg';admin.mkdir(parents=True);(admin/'status').touch()
    sentinels=['home/test/Games/FrotzZero/stories/user.z5','home/test/.local/share/frotz-zero/stories/test/saves/user.qzl','home/test/.config/frotz-zero/settings.conf']
    for name in sentinels:
        p=stage/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('user-data-must-survive\n')
    command=['dpkg','--force-not-root','--force-bad-path','--force-depends','--force-architecture',f'--root={stage}',f'--admindir={admin}']
    logs=[]
    for action,args in [('install',['-i',str(deb)]),('reinstall',['-i',str(deb)]),('remove',['--remove','frotz-zero']),('purge',['--purge','frotz-zero'])]:
        r=subprocess.run(command+args,text=True,capture_output=True);logs.append(action+'\n'+r.stdout+r.stderr)
        assert r.returncode==0,(action,r.stdout,r.stderr)
        assert all((stage/name).read_text()=='user-data-must-survive\n' for name in sentinels),action
        if action in ('install','reinstall'):assert (stage/'usr/bin/frotz-zero').exists()
        else:assert not (stage/'usr/bin/frotz-zero').exists()
(work/'dpkg.log').write_text('\n'.join(logs))
result={'passed':['isolated dpkg install','same-version upgrade/reinstall','remove','purge','story/save/settings sentinels preserved throughout'],'scope':'Owned temporary filesystem and isolated dpkg database; not physical-device removal','package_sha256':hashlib.sha256(deb.read_bytes()).hexdigest()}
(work/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
