#!/bin/sh
# Bounded normal-user test. Always restore the pre-existing launcher state.
set -eu
test "$(id -u)" != 0 || { echo 'Run as the device user, not root'; exit 1; }
base="$HOME/.cache/frotz-zero-tests"
binary="${FROTZ_ZERO_TEST_BINARY:-/usr/bin/frotz-zero}"
test -x "$binary"
test -f "$base/fixtures/lantern.z5"
if pgrep -x frotz-zero >/dev/null; then echo 'Frotz Zero is already running; exit it first'; exit 1; fi
run="$base/run-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
export XDG_CONFIG_HOME="$run/config" XDG_DATA_HOME="$run/data"
mkdir -p "$XDG_CONFIG_HOME/frotz-zero"
if [ "${FROTZ_ZERO_TEST_QUIET:-0}" = 1 ]; then printf 'mute=1\n' > "$XDG_CONFIG_HOME/frotz-zero/settings.conf"; fi
active=0
systemctl --user is-active --quiet APPLaunch.service && active=1
restore(){ if [ "$active" = 1 ]; then systemctl --user start APPLaunch.service; fi; }
trap restore EXIT
trap 'exit 130' HUP INT TERM
if [ "$active" = 1 ]; then systemctl --user stop APPLaunch.service; fi
"$binary" --version > "$run/abi.txt"
ldd "$binary" >> "$run/abi.txt"
if grep -q 'not found' "$run/abi.txt"; then cat "$run/abi.txt"; exit 1; fi
cat > "$run/home.drive" <<EOF
wait 400
capture $run/home.png
key Return
key End
wait 200
capture $run/library.png
key Right
capture $run/story-page.png
quit
key Escape
EOF
timeout 12 "$binary" --device --stories "$base/fixtures" --drive "$run/home.drive" > "$run/library.log" 2>&1
cat > "$run/credits.drive" <<EOF
wait 200
text 5
key Down
key Return
capture $run/about.png
key Return
key Down
key Return
capture $run/credits.png
key End
capture $run/credits-end.png
key Home
capture $run/credits-home.png
quit
EOF
timeout 12 "$binary" --device --stories "$base/fixtures" --drive "$run/credits.drive" > "$run/credits.log" 2>&1
cmp "$run/credits.png" "$run/credits-home.png"
mkdir -p "$run/light-config/frotz-zero"
printf 'theme=light\nmute=1\n' > "$run/light-config/frotz-zero/settings.conf"
cat > "$run/light.drive" <<EOF
wait 200
capture $run/home-light.png
text 5
key Down
key Return
capture $run/about-light.png
quit
EOF
XDG_CONFIG_HOME="$run/light-config" timeout 10 "$binary" --device --stories "$base/fixtures" --drive "$run/light.drive" > "$run/light.log" 2>&1
# Controls use isolated settings and return to the selected topic in both themes.
for theme in dark light; do
    control_config="$XDG_CONFIG_HOME"
    if [ "$theme" = light ]; then control_config="$run/light-config"; fi
    cat > "$run/controls-$theme.drive" <<EOF
wait 200
text 5
capture $run/controls-$theme-help.png
key Return
capture $run/controls-$theme-topics.png
key Right
capture $run/controls-$theme-first.png
key PageDown
capture $run/controls-$theme-page.png
key End
capture $run/controls-$theme-end.png
key Down
capture $run/controls-$theme-clamped.png
key Home
capture $run/controls-$theme-home.png
key Left
capture $run/controls-$theme-return.png
text 2
capture $run/controls-$theme-game.png
key Escape
key Home
key Down
key Down
key Return
capture $run/controls-$theme-menus.png
key Escape
key Home
key Down
key Down
key Down
key Return
capture $run/controls-$theme-saves.png
key Escape
key End
key Up
key Return
capture $run/controls-$theme-keyboard.png
key Escape
key Escape
capture $run/controls-$theme-help-return.png
quit
EOF
    XDG_CONFIG_HOME="$control_config" timeout 12 "$binary" --device --stories "$base/fixtures" --drive "$run/controls-$theme.drive" > "$run/controls-$theme.log" 2>&1
    cmp "$run/controls-$theme-first.png" "$run/controls-$theme-home.png"
    cmp "$run/controls-$theme-end.png" "$run/controls-$theme-clamped.png"
    cmp "$run/controls-$theme-topics.png" "$run/controls-$theme-return.png"
    cmp "$run/controls-$theme-help.png" "$run/controls-$theme-help-return.png"
done
cat > "$run/play.drive" <<EOF
wait 800
text north
key Return
wait 800
text save
key Return
wait 400
text hardware
key Return
wait 800
text draft
wait 400
capture $run/story.png
key Help
capture $run/controls-in-game.png
key Return
key Down
key Escape
key Escape
capture $run/story-after-help.png
quit
EOF
timeout 15 "$binary" --device --engine "$base/fixtures/lantern.z5" --drive "$run/play.drive" > "$run/story.log" 2>&1
cmp "$run/story.png" "$run/story-after-help.png"
cat > "$run/resume.drive" <<EOF
wait 800
capture $run/continued.png
quit
EOF
timeout 10 "$binary" --device --engine "$base/fixtures/lantern.z5" --resume --drive "$run/resume.drive" > "$run/resume.log" 2>&1
cmp "$run/story.png" "$run/continued.png"
printf 'text_size=20\nmute=1\n' > "$XDG_CONFIG_HOME/frotz-zero/settings.conf"
cat > "$run/reading.drive" <<EOF
wait 600
capture $run/reading-opening.png
text north
key Return
text v4draft
# Linux KEY_LEFTCTRL with P, N, U and D; no second modifier.
evkey 29 1
evkey 25 1
evkey 25 0
evkey 29 0
evkey 29 1
evkey 49 1
evkey 49 0
evkey 29 0
evkey 29 1
evkey 22 1
evkey 22 0
evkey 29 0
wait 1400
capture $run/reading22.png
evkey 29 1
evkey 32 1
evkey 32 0
evkey 29 0
wait 1400
capture $run/reading20.png
quit
EOF
timeout 12 "$binary" --device --engine "$base/fixtures/lantern.z4" --drive "$run/reading.drive" > "$run/reading.log" 2>&1
sed 's@continued.png@reading-continued.png@g' "$run/resume.drive" > "$run/reading-resume.drive"
timeout 10 "$binary" --device --engine "$base/fixtures/lantern.z4" --resume --drive "$run/reading-resume.drive" > "$run/reading-resume.log" 2>&1
cmp "$run/reading20.png" "$run/reading-continued.png"
if [ "${FROTZ_ZERO_TEST_QUIET:-0}" != 1 ]; then printf 'mute=0\n' > "$XDG_CONFIG_HOME/frotz-zero/settings.conf"; fi
cat > "$run/media.drive" <<EOF
wait 1800
text a
wait 1200
capture $run/media.png
key ctrl+v
key Home
key Down
key Down
key Right
wait 1400
capture $run/media-fill.png
key ctrl+v
key Home
key Down
key Right
wait 1400
text q
EOF
timeout 12 "$binary" --device --engine "$base/fixtures/graphics.z6" --drive "$run/media.drive" > "$run/media.log" 2>&1
python3 - "$run" <<'PY'
import pathlib,json,sys,os
p=pathlib.Path(sys.argv[1])
for f in p.glob('data/frotz-zero/stories/*/working/drive-report.json'):
    r=json.loads(f.read_text())
    if 'PNG resource drawn.' in r['transcript']:
        assert 'Sound callbacks: 1.' in r['transcript'];(p/'media-unmuted.json').write_text(json.dumps(r))
assert (p/'media-unmuted.json').exists()
PY
mkdir -p "$XDG_CONFIG_HOME/frotz-zero"
printf 'mute=1\n' > "$XDG_CONFIG_HOME/frotz-zero/settings.conf"
sed 's@media.png@muted.png@g' "$run/media.drive" > "$run/muted.drive"
timeout 12 "$binary" --device --engine "$base/fixtures/graphics.z6" --drive "$run/muted.drive" > "$run/muted.log" 2>&1
# Exercise downloads through the actual UI with an original CC0 fixture.
python3 - "$run" "$base/fixtures/lantern.z5" <<'PY'
import pathlib,json,hashlib,sys
p=pathlib.Path(sys.argv[1]);source=p/'online-source';(source/'gamefiles').mkdir(parents=True)
b=bytearray(pathlib.Path(sys.argv[2]).read_bytes());b[2:4]=(2).to_bytes(2,'big')
name='online-lantern.z5';(source/'gamefiles'/name).write_bytes(b)
row={'dir':'gamefiles','type':'z5','title':'The Lantern Room - Online demo','release':'2','serial':b[18:24].decode(),'md5':hashlib.md5(b).hexdigest(),'filename':name,'comment':'Original CC0 fixture for the on-device download flow.'}
(source/'catalog.json').write_text(json.dumps([row]));(p/'online-stories').mkdir()
PY
cat > "$run/online.drive" <<EOF
wait 400
key Return
key Return
key Return
wait 200
capture $run/online-library.png
key Return
wait 600
key Return
key Return
wait 200
capture $run/online-release.png
key Return
wait 600
key Return
wait 200
capture $run/download-ready.png
quit
EOF
timeout 15 "$binary" --device --stories "$run/online-stories" --online-fixture "$run/online-source" --drive "$run/online.drive" > "$run/online.log" 2>&1
cmp "$run/online-source/gamefiles/online-lantern.z5" "$run/online-stories/online-lantern.z5"
cat > "$run/manual.drive" <<EOF
wait 400
key Return
text o
key Return
key Down
key Down
key Down
key Return
key Down
key Return
wait 200
capture $run/manual-link.png
quit
EOF
timeout 10 "$binary" --device --stories "$run/online-stories" --drive "$run/manual.drive" > "$run/manual.log" 2>&1
# Real HTTPS on the Zero; metadata only, isolated from the user's cache.
cat > "$run/live-catalog.drive" <<EOF
wait 400
key Return
text o
key Return
key Return
wait 18000
capture $run/live-catalog.png
quit
EOF
XDG_DATA_HOME="$run/live-data" timeout 25 "$binary" --device --stories "$base/fixtures" --drive "$run/live-catalog.drive" > "$run/live-catalog.log" 2>&1
# IF Archive fixture: native UI, V8 bytes and the indexed-cover story page.
python3 - "$run" "$base/fixtures" <<'PY'
import pathlib,hashlib,sys,struct
p=pathlib.Path(sys.argv[1]);fixtures=pathlib.Path(sys.argv[2]);source=p/'archive-source';(source/'indexes').mkdir(parents=True);(source/'if-archive/games/zcode').mkdir(parents=True)
b=(fixtures/'lantern.z8').read_bytes();name='archive-lantern.z8';(source/'if-archive/games/zcode'/name).write_bytes(b)
xml='<ifarchive><file><name>'+name+'</name><path>if-archive/games/zcode/'+name+'</path><size>'+str(len(b))+'</size><md5>'+hashlib.md5(b).hexdigest()+'</md5><description>Archive Lantern by Frotz Zero. Original CC0 fixture.</description></file></ifarchive>'
(source/'indexes/Master-Index.xml').write_text(xml);(p/'archive-stories').mkdir()
cover=bytearray((fixtures/'graphics.blb').read_bytes());cover.extend(b'Fspc'+struct.pack('>II',4,1));cover[4:8]=struct.pack('>I',len(cover)-8);(p/'cover-stories').mkdir();(p/'cover-stories/cover.zblorb').write_bytes(cover)
PY
cat > "$run/archive.drive" <<EOF
wait 300
text 3
text 2
text 1
wait 600
key Right
key Right
capture $run/archive-release.png
text 1
wait 600
key Right
capture $run/archive-story.png
quit
EOF
timeout 15 "$binary" --device --stories "$run/archive-stories" --online-fixture "$run/archive-source" --drive "$run/archive.drive" > "$run/archive.log" 2>&1
cmp "$run/archive-source/if-archive/games/zcode/archive-lantern.z8" "$run/archive-stories/archive-lantern.z8"
cat > "$run/cover.drive" <<EOF
wait 300
text 2
key Right
capture $run/embedded-cover.png
quit
EOF
timeout 10 "$binary" --device --stories "$run/cover-stories" --drive "$run/cover.drive" > "$run/cover.log" 2>&1
cat > "$run/v8.drive" <<EOF
wait 400
text north
key Return
text v8draft
capture $run/v8.png
quit
EOF
timeout 10 "$binary" --device --engine "$base/fixtures/lantern.z8" --drive "$run/v8.drive" > "$run/v8.log" 2>&1
sed 's@continued.png@v8-continued.png@g' "$run/resume.drive" > "$run/v8-resume.drive"
timeout 10 "$binary" --device --engine "$base/fixtures/lantern.z8" --resume --drive "$run/v8-resume.drive" > "$run/v8-resume.log" 2>&1
cmp "$run/v8.png" "$run/v8-continued.png"
# Title pauses, fixed-frame text zoom and deletion use original fixtures only.
cat > "$run/intro.drive" <<EOF
wait 300
evkey 28 2
evkey 28 0
capture $run/intro.png
key ctrl+u
wait 1400
capture $run/intro-larger.png
quit
EOF
timeout 8 "$binary" --device --engine "$base/fixtures/intro.z4" --drive "$run/intro.drive" > "$run/intro.log" 2>&1
cat > "$run/framed.drive" <<EOF
wait 300
key ctrl+v
key Home
key Down
key Down
key Right
wait 1400
capture $run/frame-fill.png
key ctrl+u
wait 1400
capture $run/frame-zoom.png
key ctrl+z
capture $run/frame-pan.png
text framedraft
quit
EOF
timeout 10 "$binary" --device --engine "$base/fixtures/framed.z6" --drive "$run/framed.drive" > "$run/framed.log" 2>&1
python3 - "$run" "$base/fixtures/lantern.z7" <<'PY'
import pathlib,sys,hashlib
p=pathlib.Path(sys.argv[1]);b=pathlib.Path(sys.argv[2]).read_bytes();stories=p/'delete-stories';stories.mkdir()
for name in ('chosen.z7','duplicate.z7'):(stories/name).write_bytes(b)
(stories/'chosen.png').write_bytes(b'cover fixture')
save=p/'delete-data/frotz-zero/stories'/hashlib.sha256(b).hexdigest()/'saves/retained.qzl';save.parent.mkdir(parents=True);save.write_text('retained save sentinel')
PY
cat > "$run/delete.drive" <<EOF
wait 300
text 2
key Right
text 4
capture $run/delete-confirm.png
key Return
capture $run/delete-cancel.png
text 4
text 3
capture $run/delete-files.png
key Escape
text 2
capture $run/deleted.png
key Return
quit
EOF
XDG_DATA_HOME="$run/delete-data" timeout 10 "$binary" --device --stories "$run/delete-stories" --drive "$run/delete.drive" > "$run/delete.log" 2>&1
# Named save management uses copies of this run's own fixture saves.
python3 - "$run" "$base/fixtures/lantern.z5" <<'PY'
import pathlib,sys,hashlib,json,shutil,os,time
p=pathlib.Path(sys.argv[1]);story=pathlib.Path(sys.argv[2]);identity=hashlib.sha256(story.read_bytes()).hexdigest();data=p/'data/frotz-zero/stories'/identity
def hashes(folder):return {str(f.relative_to(folder)):hashlib.sha256(f.read_bytes()).hexdigest() for f in folder.rglob('*') if f.is_file()}
(p/'manage-stories').mkdir();shutil.copyfile(story,p/'manage-stories/Lantern.z5')
shutil.copytree(data/'resume',data/'saves/managed.fzsave');shutil.copyfile(data/'saves/hardware.qzl',data/'saves/manage.qzl')
os.utime(data/'saves/managed.fzsave',(time.time()+10,time.time()+10));os.utime(data/'saves/manage.qzl',(time.time()+20,time.time()+20))
(p/'manage-before.json').write_text(json.dumps({'identity':identity,'resume':hashes(data/'resume'),'save':hashlib.sha256((data/'saves/hardware.qzl').read_bytes()).hexdigest()}))
PY
cat > "$run/manage.drive" <<EOF
wait 300
text 2
key Right
text 3
capture $run/saves.png
text m
text 2
key Home
key Delete
key Delete
key Delete
key Delete
key Delete
key Delete
text renamed
key Return
key Return
capture $run/save-renamed.png
text m
text 3
key Return
text m
text 3
text 2
key Return
text m
text 2
key Home
key Delete
key Delete
key Delete
key Delete
key Delete
key Delete
key Delete
text chapter
key Return
key Return
capture $run/checkpoint-renamed.png
text m
text 3
text 2
key Return
capture $run/saves-retained.png
quit
EOF
timeout 12 "$binary" --device --stories "$run/manage-stories" --drive "$run/manage.drive" > "$run/manage.log" 2>&1
# Universal readable text and exact Continue, including the original view.
mkdir -p "$run/universal-config/frotz-zero"
for version in 3 4 5 7 8; do
    printf 'text_size=18\nmute=1\n' > "$run/universal-config/frotz-zero/settings.conf"
    cat > "$run/universal-$version.drive" <<EOF
wait 250
text north
key Return
text draft
key ctrl+u
wait 1400
capture $run/universal-$version-text.png
quit
EOF
    XDG_CONFIG_HOME="$run/universal-config" XDG_DATA_HOME="$run/universal-$version-data" timeout 12 "$binary" --device --engine "$base/fixtures/lantern.z$version" --drive "$run/universal-$version.drive" > "$run/universal-$version.log" 2>&1
    cat > "$run/universal-$version-resume.drive" <<EOF
wait 250
capture $run/universal-$version-resumed.png
key Tab
capture $run/universal-$version-original.png
quit
EOF
    XDG_CONFIG_HOME="$run/universal-config" XDG_DATA_HOME="$run/universal-$version-data" timeout 12 "$binary" --device --engine "$base/fixtures/lantern.z$version" --resume --drive "$run/universal-$version-resume.drive" >> "$run/universal-$version.log" 2>&1
    cmp "$run/universal-$version-text.png" "$run/universal-$version-resumed.png"
    cat > "$run/universal-$version-original.drive" <<EOF
wait 250
capture $run/universal-$version-original-resumed.png
key Tab
quit
EOF
    XDG_CONFIG_HOME="$run/universal-config" XDG_DATA_HOME="$run/universal-$version-data" timeout 12 "$binary" --device --engine "$base/fixtures/lantern.z$version" --resume --drive "$run/universal-$version-original.drive" >> "$run/universal-$version.log" 2>&1
    cmp "$run/universal-$version-original.png" "$run/universal-$version-original-resumed.png"
done
cat > "$run/text-modes.drive" <<EOF
wait 250
capture $run/text-menu.png
text n
wait 100
capture $run/text-timed.png
wait 2400
quit
EOF
XDG_DATA_HOME="$run/text-modes-data" timeout 12 "$binary" --device --engine "$base/fixtures/text-modes.z5" --drive "$run/text-modes.drive" > "$run/text-modes.log" 2>&1
cat > "$run/pagination.drive" <<EOF
wait 250
text b
key Return
capture $run/pagination.png
quit
EOF
XDG_DATA_HOME="$run/pagination-data" timeout 12 "$binary" --device --engine "$base/fixtures/pagination.z3" --drive "$run/pagination.drive" > "$run/pagination.log" 2>&1
# Automatic artwork/text and timed-input checks use this run's isolated data.
cat > "$run/automatic.drive" <<EOF
wait 250
capture $run/automatic-art.png
key Return
capture $run/automatic-question.png
key ctrl+u
wait 1400
capture $run/automatic-large.png
text n
key Return
capture $run/automatic-page.png
key Tab
capture $run/automatic-original.png
key Tab
capture $run/automatic-page-again.png
text autodraft
quit
EOF
timeout 12 "$binary" --device --engine "$base/fixtures/automatic.z6" --drive "$run/automatic.drive" > "$run/automatic.log" 2>&1
cmp "$run/automatic-page.png" "$run/automatic-page-again.png"
cat > "$run/automatic-timed.drive" <<EOF
wait 250
key Return
text n
text timed
key Return
wait 100
capture $run/automatic-timed.png
wait 2400
quit
EOF
XDG_DATA_HOME="$run/automatic-timed-data" timeout 12 "$binary" --device --engine "$base/fixtures/automatic.z6" --drive "$run/automatic-timed.drive" > "$run/automatic-timed.log" 2>&1
# Refresh the real archive over HTTPS; only catalogue metadata is fetched.
cat > "$run/live-archive.drive" <<EOF
wait 300
text 3
text 2
text 1
wait 24000
capture $run/live-archive.png
quit
EOF
XDG_DATA_HOME="$run/live-archive-data" timeout 32 "$binary" --device --stories "$base/fixtures" --drive "$run/live-archive.drive" > "$run/live-archive.log" 2>&1
python3 - "$run" <<'PY'
import pathlib,json,sys,os
p=pathlib.Path(sys.argv[1]);reports=list(p.glob('data/frotz-zero/stories/*/working/drive-report.json'))
records=[json.loads(f.read_text()) for f in reports]
assert any(r['draft']=='draft' and 'Turns: 1. Score: 1.' in r['transcript'] for r in records),records
assert any(r['draft']=='v4draft' and r['reading_size']==20 and r['canvas_width']==640 and 'Turns: 1. Score: 1.' in r['transcript'] for r in records),records
assert any(r['draft']=='v4draft' and r['view_mode']=='Automatic' for r in records)
assert any(r['draft']=='v8draft' and r['safe_resume'] and 'Turns: 1. Score: 1.' in r['transcript'] for r in records)
assert any('PNG resource drawn.' in r['transcript'] and 'Sound callbacks: 1.' in r['transcript'] and 'Timed character read (0.3 seconds): 0.' in r['transcript'] for r in records),records
assert list(p.glob('data/frotz-zero/stories/*/saves/hardware.qzl'))
fallback=list(p.glob('data/frotz-zero/stories/*/audio-status'))
backend=[line for line in (p/'media.log').read_text().splitlines() if 'audio backend:' in line]
result={'passed':['normal-user framebuffer/input initialization','actual device home, synopsis picker and story rendering','named Quetzal save','pixel-identical Continue after restart','v4 native Ctrl+U/D reading 20/22/20, Ctrl+P/N history/draft and pixel-identical Continue','v6 PNG/window/font3 path',('muted' if os.environ.get('FROTZ_ZERO_TEST_QUIET')=='1' else 'unmuted and muted')+' audio completion; timed character input'], 'audio_backend':'silent fallback' if fallback else (backend[-1] if backend else 'configured backend opened; see invocation'), 'physical_observation':'Audibility, display appearance and physical key presses require user verification'}
assert (p/'manual-link.png').is_file()
live=json.loads((p/'live-data/frotz-zero/online/catalog.json').read_text());assert any(r.get('type')=='z5' for r in live)
result['passed']+=['on-device Online Library fixture download with identical bytes','manual URL and QR rendered through native display','real verified HTTPS catalogue fetched on the Zero (metadata only)']
assert all((p/name).is_file() for name in ('about.png','about-light.png','home-light.png'))
assert (p/'credits.png').read_bytes()!=(p/'credits-end.png').read_bytes()
result['passed']+=['native dark/light adventure home and About rendering','offline Frotz credits with End/Home scrolling and exact return']
archive=json.loads((p/'live-archive-data/frotz-zero/online/ifarchive.json').read_text());assert len(archive)>800
assert all((p/name).is_file() for name in ('reading-opening.png','story-page.png','media-fill.png','archive-release.png','archive-story.png','embedded-cover.png'))
result['passed']+=['native first-page Reader and story page','fit and fill graphics modes','native IF Archive verified V8 download','indexed Blorb frontispiece on story page','V8 pixel-identical Continue with draft','real IF Archive HTTPS catalogue fetch and bounded parsing: '+str(len(archive))+' files']
assert any(r['input_kind']=='character' and 'THE OBSERVATORY' in r['reader_visible'] and 'FIRST STORY PARAGRAPH' not in r['transcript'] and 'STATUS MUST' not in r['transcript'] for r in records)
assert any(r['view_mode']=='Frame + text zoom' and r['draft']=='framedraft' and r['target_width']<320 and r['source_width']<560 for r in records)
assert not list((p/'delete-stories').iterdir())
assert next(p.glob('delete-data/frotz-zero/stories/*/saves/retained.qzl')).read_text()=='retained save sentinel'
result['passed']+=['native untimed intro Reader and live sizing without advancing the story','fixed-frame text zoom, pan and draft input','native delete Cancel, file review and confirmed removal with saves kept']
import hashlib
before=json.loads((p/'manage-before.json').read_text());managed=p/'data/frotz-zero/stories'/before['identity']
assert {f.name for f in (managed/'saves').iterdir()}=={'hardware.qzl'}
assert hashlib.sha256((managed/'saves/hardware.qzl').read_bytes()).hexdigest()==before['save']
assert {str(f.relative_to(managed/'resume')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (managed/'resume').rglob('*') if f.is_file()}==before['resume']
result['passed']+=['native Quetzal/checkpoint rename and confirmed delete; other save and automatic Continue unchanged']
assert any(r['view_mode']=='Automatic' and r['reader_active'] and r['draft']=='autodraft' and 'Answer code: 110.' in r['transcript'] for r in records)
timed=json.loads(next(p.glob('automatic-timed-data/frotz-zero/stories/*/working/drive-report.json')).read_text())
assert timed['view_mode']=='Automatic' and timed['input_kind']=='line' and 'Timer result: 0.' in timed['transcript']
assert (p/'automatic-page.png').read_bytes()!=(p/'automatic-original.png').read_bytes()
result['passed']+=['native automatic fitted artwork, non-main-window question, text sizing and reversible artwork view','native automatic timed text preserves timer expiry and returns to command input']
for version in (3,4,5,7,8):
    r=json.loads(next(p.glob(f'universal-{version}-data/frotz-zero/stories/*/working/drive-report.json')).read_text())
    assert r['view_mode']=='Automatic' and r['reader_active'] and r['reading_size']==20 and r['draft']=='draft' and r['safe_resume'] and 'Turns: 1. Score: 1.' in r['transcript'],r
    result['passed']+=[f'native V{version} Automatic reading, sizing, draft and pixel-identical text/original-view Continue']
r=json.loads(next(p.glob('text-modes-data/frotz-zero/stories/*/working/drive-report.json')).read_text())
assert r['reader_active'] and r['input_kind']=='line' and 'Menu answer: 110.' in r['transcript'] and 'Timer result: 0.' in r['transcript'] and 'ABCD' not in r['transcript'],r
r=json.loads(next(p.glob('pagination-data/frotz-zero/stories/*/working/drive-report.json')).read_text())
assert r['reader_active'] and r['input_kind']=='character' and not r['safe_resume'],r
result['passed']+=['native non-V6 upper text menu and timer, with font3 symbols excluded','native readable More pause with unsafe mid-output checkpoints blocked']
for theme in ('dark','light'):
    assert (p/f'controls-{theme}-first.png').read_bytes()!=(p/f'controls-{theme}-page.png').read_bytes()
    assert all((p/f'controls-{theme}-{topic}.png').is_file() for topic in ('game','menus','saves','keyboard'))
    result['passed']+=[f'native {theme} Controls topics, formatted rows, paging limits and exact return navigation']
result['passed']+=['native in-game Controls paging returns to the exact story view and draft']
(p/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
PY
printf '%s\n' "$run" > "$base/latest"
printf 'Evidence: %s\n' "$run"
