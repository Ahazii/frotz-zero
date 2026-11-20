#!/usr/bin/env python3
"""Create a deterministic adapted Frotz tree. Upstream checkout remains pristine.
The exact pinned source is required; each structural replacement fails closed.
SPDX-License-Identifier: GPL-2.0-or-later
"""
import pathlib, shutil, subprocess, sys, json, hashlib
root=pathlib.Path(__file__).resolve().parents[1]
src=root/'third_party/frotz'
dest=pathlib.Path(sys.argv[1])
pin=json.loads((root/'dependencies.lock.json').read_text())['frotz']['commit']
if (src/'.git').exists():
    if subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip()!=pin:
        raise SystemExit('Frotz revision does not match dependencies.lock.json')
    if subprocess.run(['git','-C',str(src),'-c','core.autocrlf=true','-c','core.fileMode=false','diff','--quiet','HEAD','--','src']).returncode:
        raise SystemExit('Pinned Frotz sources have local changes; review them before building')
else:
    manifest=json.loads((src/'.frotz-zero-source.json').read_text())
    if manifest['commit']!=pin:raise SystemExit('Archived Frotz revision mismatch')
    for name,digest in manifest['files'].items():
        if pathlib.PurePosixPath(name).is_absolute() or '..' in pathlib.PurePosixPath(name).parts:raise SystemExit('Invalid source manifest path')
        if hashlib.sha256((src/name).read_bytes()).hexdigest()!=digest:raise SystemExit('Archived Frotz source changed: '+name)
shutil.copytree(src/'src',dest/'src',dirs_exist_ok=True,ignore=shutil.ignore_patterns('*.o','*.a','*.d'))
def edit(path,fn):
    p=dest/'src'/path
    p.write_text(fn(p.read_text()),encoding='utf-8',newline='\n')
def replace(s,a,b):
    if s.count(a)!=1: raise RuntimeError(f'Expected exactly one patch anchor: {a[:90]!r}; found {s.count(a)}')
    return s.replace(a,b)
def body(s,name,new):
    import re
    m=re.search(r'(?m)^(?:static )?(?:void|int|bool|zchar|zword|char \*|char\*|zlong)\s*'+name+r'\([^;]*?\)\s*\{',s)
    if not m: raise RuntimeError('Missing function '+name)
    a=m.end()-1;i=a+1;depth=1
    # Functions replaced below have balanced braces even inside their comments.
    while depth:
        if s[i]=='{': depth+=1
        elif s[i]=='}': depth-=1
        i+=1
    return s[:a]+'{\n'+new+'\n}'+s[i:]
def api(s):return '#include "engine_api.h"\n'+s
edit('common/main.c',lambda s:replace(s,'int cdecl main(int argc, char *argv[])','int cdecl fz_vm_main(int argc, char *argv[])'))
edit('common/main.c',lambda s:api(replace(s,'\tz_restart();\n\tinterpret();','\tz_restart();\n\tfz_engine_started();\n\tinterpret();')))
edit('common/process.c',lambda s:api(replace(s,'\tdo {\n\t\tzbyte opcode;','\tdo {\n\t\tzbyte opcode;\n\t\tfz_before_opcode();')))
# Omitted read timeouts must not inherit operand 3 from an earlier opcode.
# The pinned core defaults the routine but omits the matching timeout reset.
edit('common/input.c',lambda s:replace(s,'\t/* Supply default arguments */\n\tif (zargc < 4)',
    '\t/* Supply default arguments */\n\tif (zargc < 3)\n\t\tzargs[2] = 0;\n\tif (zargc < 4)'))
def transcript(s):
    # Capture at emission, while the logical font/window still belongs to this
    # character. A buffered word can contain several later font changes.
    s=replace(s,'void print_char(zchar c)\n{','void print_char(zchar c)\n{\n    if(ostream_screen && !ostream_memory && c>=32)fz_output_char(c);')
    s=replace(s,'void new_line(void)\n{','void new_line(void)\n{\n    if(ostream_screen && !ostream_memory)fz_output_char(10);')
    return 'extern void fz_output_char(unsigned short);\n'+s
edit('common/buffer.c',transcript)
def video(s):
    s=api(s)
    s=body(s,'myGrefresh','fz_present_pixels(sbuffer, ewidth, eheight);')
    s=body(s,'sf_flushdisplay','if (!dirty) return false; myGrefresh(); dirty=0; return true;')
    s=body(s,'sf_initvideo','''(void)full;
    fz_video_init(W,H);
    sdl_active=TRUE;
    sbuffer=calloc((size_t)W*H,sizeof(zlong));
    if(!sbuffer) os_fatal("Cannot allocate story framebuffer");
    xmin=ymin=0; xmax=ewidth=W; ymax=eheight=H; sbpitch=W; dirty=1;''')
    s=body(s,'sf_read_key','(void)allowed; (void)text; return fz_read_char(timeout,cursor);')
    s=body(s,'os_read_key','return fz_read_char(timeout,cursor);')
    s=body(s,'os_read_line','return fz_read_line(max,buf,timeout,width,continued);')
    s=body(s,'os_read_mouse','return fz_mouse_state(&mouse_x,&mouse_y);')
    s=body(s,'os_tick','fz_tick();')
    # Preserve a coherent line while horizontally scrolling only the input field.
    s=replace(s,'\t/* Display the input */\n\tts->cx = ptx;','''    int clipx,clipy,clipw,cliph; sf_getclip(&clipx,&clipy,&clipw,&cliph);
    int advance=0,oh=0; for(int n=0;n<pos;n++) advance+=sf_charwidth(buffer[n],&oh);
    int shift=advance+os_char_width('0')>width?advance+os_char_width('0')-width:0;
    sf_setclip(ptx,pty,width,height);
    /* Display the input */
    ts->cx = ptx-shift;''')
    s=replace(s,'drawthecursor(ptx + wid, pty, 1);','drawthecursor(ptx + wid-shift, pty, 1);')
    s=replace(s,'\t/* Update the window */\n\tsf_flushdisplay();','\tsf_setclip(clipx,clipy,clipw,cliph);\n\t/* Update the window */\n\tsf_flushdisplay();')
    s+='''
void fz_draw_input(uint16_t*b,int p,int x,int y,int w,int cursor){sf_DrawInput(b,p,x,y,w,cursor);}
void fz_get_text_position(int*x,int*y){SF_textsetting*t=sf_curtextsetting();*x=t->cx;*y=t->cy;}
void fz_set_mouse(int x,int y){mouse_x=x;mouse_y=y;}
void fz_flush(void){flush_buffer();sf_flushtext();sf_flushdisplay();}
int fz_is_terminator(uint16_t c){return is_terminator(c);}
int fz_char_valid(uint16_t c){return sf_IsValidChar(c);}
int fz_text_width(const uint16_t*b){return os_string_width(b);}
int fz_video_state(FILE*f,int loading){
  SF_textsetting*t=sf_curtextsetting();
  int fields[]={t->style,t->zfontnum,t->cx,t->cy,t->oh,t->fore,t->back,t->foreDefault,t->backDefault,t->backTransparent,xmin,xmax,ymin,ymax};
  if(loading){
    if(fread(fields,sizeof(fields),1,f)!=1)return 0;
    if(fields[1]!=1&&fields[1]!=3&&fields[1]!=4)return 0;
    os_set_font(fields[1]);os_set_text_style(fields[0]);
    t->cx=fields[2];t->cy=fields[3];t->oh=fields[4];t->fore=fields[5];t->back=fields[6];t->foreDefault=fields[7];t->backDefault=fields[8];t->backTransparent=fields[9];
    xmin=fields[10];xmax=fields[11];ymin=fields[12];ymax=fields[13];
    if(xmin<0||xmax>ewidth||ymin<0||ymax>eheight)return 0;
    if(fread(sbuffer,sizeof(zlong),(size_t)ewidth*eheight,f)!=(size_t)ewidth*eheight)return 0;
    dirty=1;
  }else{if(fwrite(fields,sizeof(fields),1,f)!=1)return 0;if(fwrite(sbuffer,sizeof(zlong),(size_t)ewidth*eheight,f)!=(size_t)ewidth*eheight)return 0;}
  return 1;
}
'''
    return s
edit('sdl/sf_video.c',video)
def util(s):
    s=api(s)
    s=body(s,'os_reset_screen','sf_flushdisplay(); sf_cleanup_all();')
    s=body(s,'os_quit','fz_engine_exit(status);')
    s=body(s,'os_read_file_name','return fz_file_dialog(default_name,flag);')
    # Fix upstream overreads in default filename construction, including -L.
    a=s.index('\t/* Create nice default file names */');b=s.index('\t/* Save the executable file name */',a)
    s=s[:a]+'''    f_setup.script_name=malloc(strlen(f_setup.story_name)+8);sprintf(f_setup.script_name,"%s%s",f_setup.story_name,EXT_SCRIPT);
    f_setup.command_name=malloc(strlen(f_setup.story_name)+8);sprintf(f_setup.command_name,"%s%s",f_setup.story_name,EXT_COMMAND);
    f_setup.save_name=strdup(f_setup.restore_mode?f_setup.tmp_save_name:"save.qzl");
    f_setup.aux_name=malloc(strlen(f_setup.story_name)+8);sprintf(f_setup.aux_name,"%s%s",f_setup.story_name,EXT_AUX);
'''+s[b:]
    return s
edit('sdl/sf_util.c',util)
def resources(s):
    s=api(s)
    s=body(s,'os_scrollback_char','fz_transcript_char(c);')
    s=body(s,'os_scrollback_erase','fz_transcript_erase(erase);')
    s=replace(s,'\tif (z_header.version == V6) {\n\t\tif (blorb_map) {\n\t\t\tz_header.config |= CONFIG_PICTURES;\n\t\t\tz_header.config |= CONFIG_SOUND;\n\t\t}\n\t}', '''    if(z_header.version==V6){
        z_header.config &= ~(CONFIG_PICTURES|CONFIG_SOUND);
        if(fz_has_pictures())z_header.config|=CONFIG_PICTURES;
        if(fz_has_sound() && sf_initsound())z_header.config|=CONFIG_SOUND;
    }
    if(!fz_has_pictures())z_header.flags &= ~GRAPHICS_FLAG;''')
    s=replace(s,'''\tif (z_header.version == V6) {
\t\tz_header.default_foreground =
\t\t    sf_GetColourIndex(sf_GetDefaultColour(true));
\t\tz_header.default_background =
\t\t    sf_GetColourIndex(sf_GetDefaultColour(false));
\t} else {
\t\tz_header.default_foreground = 1;
\t\tz_header.default_background = 1;
\t}''','''    /* Header defaults are actual colour numbers, never the "default" sentinel. */
    z_header.default_foreground=sf_GetColourIndex(sf_GetDefaultColour(true));
    z_header.default_background=sf_GetColourIndex(sf_GetDefaultColour(false));
    if(!fz_has_sound())z_header.flags &= ~(z_header.version==V3?OLD_SOUND_FLAG:SOUND_FLAG);''')
    s=body(s,'os_fatal','''va_list args; char message[1024]; va_start(args,s);vsnprintf(message,sizeof(message),s,args);va_end(args);fz_engine_error(message);fz_engine_exit(EXIT_FAILURE);''')
    return s
edit('sdl/sf_resource.c',resources)
def memory(s):
    s=api(s)
    s=replace(s,'\t\tsuccess = save_quetzal(gfp, story_fp);','\t\tsuccess = save_quetzal(gfp, story_fp);') # checked anchor, completion is patched after fclose below
    # The successful write is committed only after the complete portable save closes.
    start=s.index('void z_save(void)')
    tail=s[start:]
    tail=replace(tail,'\t\tfclose(gfp);','\t\tif(fflush(gfp)||fsync(fileno(gfp))) success=0;\n\t\tif(fclose(gfp)) success=0;')
    tail=replace(tail,'''\t\t/* Close game file and check for errors */
\t\tif (fclose(gfp) == EOF || ferror(story_fp)) {
\t\t\tprint_string("Error writing save file\\n");
\t\t\tgoto finished;
\t\t}
\t\t/* Success */
\t\tsuccess = 1;''','''        /* Commit only a successful, completely flushed portable save. */
        if(fflush(gfp)||fsync(fileno(gfp)))success=0;
        if(fclose(gfp)==EOF||ferror(story_fp))success=0;
        success=fz_save_complete(success);''')
    s=s[:start]+tail
    s+='''
int fz_vm_save(const char*p){FILE*f=fopen(p,"wb");if(!f)return 0;int ok=save_quetzal(f,story_fp);if(fflush(f))ok=0;if(fclose(f))ok=0;return ok;}
int fz_vm_restore(const char*p){FILE*f=fopen(p,"rb");if(!f)return 0;int ok=restore_quetzal(f,story_fp);fclose(f);if(ok>0)restart_header();return ok>0;}
'''
    return '#include <unistd.h>\n'+s
edit('common/fastmem.c',memory)
edit('common/random.c',lambda s:s+'''\nint fz_random_state(FILE*f,int load){if(load)return fread(&A,sizeof(A),1,f)==1&&fread(&interval,sizeof(interval),1,f)==1&&fread(&counter,sizeof(counter),1,f)==1;return fwrite(&A,sizeof(A),1,f)==1&&fwrite(&interval,sizeof(interval),1,f)==1&&fwrite(&counter,sizeof(counter),1,f)==1;}\n''')
def screen(s):
    s=api(s)
    s=replace(s,'void erase_window(zword win)\n{','''void erase_window(zword win)
{
    if(wp[win].x_size>=160 && wp[win].y_size>=80)fz_layout_changed();''')
    s=replace(s,'static void erase_screen(zword win)\n{','''static void erase_screen(zword win)
{
    fz_layout_changed();''')
    s=replace(s,'\tos_draw_picture(pic, y, x);','''    {int height,width;if(os_picture_data(pic,&height,&width)&&width>=160&&height>=80)fz_layout_changed();}
    os_draw_picture(pic, y, x);''')
    s+='''
#include <string.h>
extern int fz_video_state(FILE*,int);
extern int fz_random_state(FILE*,int);
void fz_output_char(zchar c){
  /* Capture readable V6 panels in any window, excluding shallow status strips.
     Artwork and window coordinates remain untouched in the original canvas. */
  int row=cwp->y_cursor/((cwp->font_size>>8)?(cwp->font_size>>8):1);
  int font=z_header.version==V6?cwp->font:wp[0].font;
  int panel=z_header.version==V6 &&
    cwp->x_size>=80 && cwp->y_size>=3*(cwp->font_size>>8);
  if(font!=GRAPHICS_FONT && (cwin==0 || panel || (z_header.version!=V6 && cwin==1 && row>=2)))
    fz_transcript_window(c,cwin,row);
}
int fz_main_window(int*x,int*y,int*w,int*h){
  *x=wp[0].x_pos-1;*y=wp[0].y_pos-1;*w=wp[0].x_size;*h=wp[0].y_size;
  return z_header.version==V6 && cwin==0 && *x>=0 && *y>=0 && *w>=160 && *h>=80;
}
int fz_screen_save(const char*p){
  FILE*f=fopen(p,"wb");if(!f)return 0;
  int fields[]={sizeof(long),sizeof(Zwindow),cwin,mwin,font_height,font_width,input_redraw,more_prompts,discarding,cursor,input_window,enable_wrapping,enable_scripting,enable_scrolling,enable_buffering};
  int ok=fwrite("FZSTATE2",8,1,f)==1&&fwrite(fields,sizeof(fields),1,f)==1&&fwrite(wp,sizeof(wp),1,f)==1&&fz_random_state(f,0)&&fz_video_state(f,0);
  if(fflush(f))ok=0;if(fclose(f))ok=0;return ok;
}
int fz_screen_restore(const char*p){
  FILE*f=fopen(p,"rb");if(!f)return 0;char magic[8];int fields[15];Zwindow wins[8];
  int ok=fread(magic,8,1,f)==1&&!memcmp(magic,"FZSTATE2",8)&&fread(fields,sizeof(fields),1,f)==1&&fields[0]==sizeof(long)&&fields[1]==sizeof(Zwindow)&&fields[2]>=0&&fields[2]<8&&fread(wins,sizeof(wins),1,f)==1;
  if(ok){memcpy(wp,wins,sizeof(wp));cwin=fields[2];mwin=fields[3];font_height=fields[4];font_width=fields[5];input_redraw=fields[6];more_prompts=fields[7];discarding=fields[8];cursor=fields[9];input_window=fields[10];enable_wrapping=fields[11];enable_scripting=fields[12];enable_scrolling=fields[13];enable_buffering=fields[14];cwp=wp+cwin;ok=fz_random_state(f,1)&&fz_video_state(f,1);}
  fclose(f);return ok;
}
void fz_before_opcode(void){
  if(*pcp==0xe4||*pcp==0xf6){
    flush_buffer();
    int safe=((z_header.version>=3&&z_header.version<=5)||z_header.version==7||z_header.version==8)&&*pcp==0xe4&&!ostream_memory&&!ostream_script&&!ostream_record&&!istream_replay&&!fz_audio_active();
    /* Never serialize the C continuation of an interrupt routine. */
    for(zword*p=fp;p<stack+STACK_SIZE-4;p=stack+p[1]+1){if((p[0]&0xf000)==0x2000){safe=0;break;}if(p[1]+1>=STACK_SIZE)break;}
    fz_capture_boundary(safe);
  }
}
'''
    return s
edit('common/screen.c',screen)
def sound(s):
    s=api(s)
    s=replace(s,'\t/* initialize sdl mixer, open up the audio device */','\tSDL_InitSubSystem(SDL_INIT_AUDIO);\n\t/* initialize sdl mixer, open up the audio device */')
    s=replace(s,'''\t\tSFaudiorunning = 0;
\t\treturn 0;
\t}

\tMix_QuerySpec''','''        /* Preserve sample duration and completion callbacks without a sink. */
        fprintf(stderr,"Frotz Zero audio output: %s\\n",SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        fz_audio_unavailable();
        if(SDL_InitSubSystem(SDL_INIT_AUDIO)<0 || Mix_OpenAudio(m_frequency,MIX_DEFAULT_FORMAT,2,audio_buffers)<0){
            SFaudiorunning=0;return 0;
        }
    }

    fprintf(stderr,"Frotz Zero audio backend: %s\\n",SDL_GetCurrentAudioDriver());
    Mix_QuerySpec''')
    s=replace(s,'\tif (!e_sfx->ended)\n\t\treturn;','')
    # Stop-before-free is required when a new sample replaces a playing one.
    s=replace(s,'\tMix_PlayChannel(0, e_sfx->sam, e_sfx->repeats);','\te_sfx->active=1;\n\tif(Mix_PlayChannel(0,e_sfx->sam,e_sfx->repeats)<0)e_sfx->ended=1;')
    s=replace(s,'\tMix_PlayMusic(e_mod->mod, e_mod->repeats);','\te_mod->active=1;\n\tif(Mix_PlayMusic(e_mod->mod,e_mod->repeats)<0)e_mod->ended=1;')
    s=s.replace('e->volume = 32 * volume;','e->volume = 16 * volume;')
    s=s.replace('Mix_Volume(0, e_sfx->volume);','Mix_Volume(0, fz_audio_volume(e_sfx->volume));')
    s=s.replace('Mix_VolumeMusic(e_mod->volume);','Mix_VolumeMusic(fz_audio_volume(e_mod->volume));')
    s+='''\nvoid fz_check_audio(void){sf_checksound();}\nint fz_audio_active(void){return (e_sfx&&(e_sfx->active||e_sfx->ended))||(e_mod&&(e_mod->active||e_mod->ended));}\nvoid fz_suspend_audio(int pause){if(pause){Mix_Pause(-1);Mix_PauseMusic();}else{Mix_Resume(-1);Mix_ResumeMusic();}if(e_sfx)Mix_Volume(0,fz_audio_volume(e_sfx->volume));if(e_mod)Mix_VolumeMusic(fz_audio_volume(e_mod->volume));}\n'''
    return s
edit('sdl/sf_sound.c',sound)
(dest/'src/common/defs.h').write_text('''#pragma once
#define UNIX
#define USE_UTF8
#define VERSION "2.56pre"
#define RELEASE_NOTES "Frotz Zero pinned frontend adaptation"
#define MAX_UNDO_SLOTS 500
#define MAX_FILE_NAME 4096
#define TEXT_BUFFER_SIZE 512
#define INPUT_BUFFER_SIZE 200
#define STACK_SIZE 32768
''')
(dest/'src/common/hash.h').write_text(f'#pragma once\n#define GIT_HASH "{pin}"\n#define GIT_HASH_SHORT "{pin[:8]}"\n#define GIT_DATE "pinned; see dependencies.lock.json"\n')
(dest/'src/sdl/sf_defines.h').write_text('#pragma once\n')
print('Prepared pinned Frotz frontend and checkpoint hooks:',pin)
