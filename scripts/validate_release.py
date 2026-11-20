#!/usr/bin/env python3
"""Run pinned official policy checks locally; never invokes publish or login."""
import pathlib,subprocess,tempfile,json,shutil,sys,os,tarfile,io,hashlib,re
from check_public_tree import FORBIDDEN_SUFFIXES
root=pathlib.Path(__file__).resolve().parents[1]
lock=json.loads((root/'dependencies.lock.json').read_text())
for key,folder in [('appbuilder','AppBuilder'),('registry_inspected','packages')]:
    path=root/'.cache'/folder
    if not (path/'.git').exists():
        subprocess.run(['git','clone','--no-checkout',lock[key]['url'],str(path)],check=True)
        subprocess.run(['git','-C',str(path),'checkout','--detach',lock[key]['commit']],check=True)
    actual=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
    if actual!=lock[key]['commit']:raise SystemExit(f'{folder} revision differs; review/update the lock before validation')
sys.path.insert(0,str(root/'.cache/AppBuilder/scripts'))
from czdev.publish import check_desktop,find_root_services,extract_metadata,is_valid_package_name,store_meta_from
app=json.loads((root/'app-builder.json').read_text());pkg=app['package_name'];deb=root/'dist'/f"{pkg}_{app['version']}_arm64.deb"
metadata=extract_metadata(str(deb));assert metadata['architecture']=='arm64' and metadata['version']==app['version'] and metadata['package']==pkg
assert is_valid_package_name(pkg) and check_desktop(str(deb)) and not find_root_services(str(deb))
out=root/'local/release-validation';out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='fz-policy-') as tmp:
    stage=pathlib.Path(tmp)/pkg;stage.mkdir();(stage/'screenshots').mkdir()
    meta=store_meta_from(app);meta['icon']=pathlib.Path(meta['icon']).name;meta['screenshots']=['screenshots/'+pathlib.Path(p).name for p in app['store']['screenshots']]
    (stage/'meta.json').write_text(json.dumps(meta));shutil.copyfile(root/app['store']['icon'],stage/meta['icon'])
    for p in app['store']['screenshots']:shutil.copyfile(root/p,stage/'screenshots'/pathlib.Path(p).name)
    scripts=root/'.cache/packages/.github/scripts'
    for name,args in [('install_path_policy.py',[pkg,str(deb),str(pathlib.Path(tmp)/'paths.txt')]),('store_meta_policy.py',[pkg,str(stage),str(root/'.cache/packages/pool/main'),str(pathlib.Path(tmp)/'meta.txt'),str(pathlib.Path(tmp)/'warnings.txt')])]:
        subprocess.run([sys.executable,str(scripts/name),*args],check=True,env=dict(os.environ,DEB_MAINTAINER=metadata['maintainer']))
    for name in ('paths.txt','meta.txt'):
        p=pathlib.Path(tmp)/name
        assert not p.exists() or not p.read_text().strip(),p.read_text() if p.exists() else ''
    warnings=pathlib.Path(tmp)/'warnings.txt';warning_text=warnings.read_text() if warnings.exists() else ''
tar_bytes=subprocess.check_output(['dpkg-deb','--fsys-tarfile',str(deb)])
with tarfile.open(fileobj=io.BytesIO(tar_bytes)) as t:
    members=[m for m in t.getmembers() if m.isfile()];names=[m.name.removeprefix('./') for m in members]
    assert all(n.startswith('usr/share/doc/frotz-zero/') or n in ('usr/bin/frotz-zero','usr/share/APPLaunch/applications/frotz-zero.desktop','usr/share/APPLaunch/share/images/frotz-zero.png') for n in names)
    assert not any(pathlib.PurePosixPath(n).suffix.lower() in FORBIDDEN_SUFFIXES or re.fullmatch(r'\.z[1-8]',pathlib.PurePosixPath(n).suffix.lower()) for n in names)
with tarfile.open(root/'dist'/f"{pkg}_{app['version']}_source.tar.gz") as t:
    names=t.getnames();assert all(not set(pathlib.PurePosixPath(n).parts)&{'local','stories','dev-data','.cache','.git'} for n in names)
    assert not any(pathlib.PurePosixPath(n).suffix.lower() in FORBIDDEN_SUFFIXES or re.fullmatch(r'\.z[1-8]',pathlib.PurePosixPath(n).suffix.lower()) for n in names)
    assert not any(n.startswith('frotz-zero/third_party/frotz/src/test/') for n in names)
    assert all(pathlib.PurePosixPath(n).parent==pathlib.PurePosixPath('frotz-zero/tests/fixtures') for n in names if n.lower().endswith('.inf'))
    assert 'frotz-zero/third_party/frotz/src/common/process.c' in names and 'frotz-zero/third_party/frotz/COPYING' in names
    for name,expected in lock['qrcodegen']['files'].items():
        assert hashlib.sha256(t.extractfile('frotz-zero/third_party/qrcodegen/'+name).read()).hexdigest()==expected
    assert 'frotz-zero/packaging/nlohmann-json-LICENSE.MIT' in names
result={'package':metadata,'package_sha256':hashlib.sha256(deb.read_bytes()).hexdigest(),'passed':['official AppBuilder package metadata/desktop/root-service checks','official install-path policy','official store metadata and registry-snapshot uniqueness policy','strict binary/source public-content allowlists','complete pinned Frotz source and licence included'],'warnings':warning_text,'limitations':'Does not publish, reserve a name, prove physical operation, or verify that the proposed source URL is public.'}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
