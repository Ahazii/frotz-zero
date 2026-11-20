// SPDX-License-Identifier: GPL-2.0-or-later
#include "app.hpp"
#include "library_ui.hpp"
#include "online_ui.hpp"
#include "online.hpp"
#include "credits.hpp"
#include "artwork.hpp"
#include <iostream>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
using namespace fz;
static int launch(const Story&s,const Paths&p,RunOptions opt){
    fs::path folder=p.data/"stories"/s.id;for(;;){std::vector<std::string>a={"/proc/self/exe","--engine",s.path.string(),"--scale",std::to_string(opt.scale)};if(opt.device)a.push_back("--device");if(opt.resume)a.push_back("--resume");if(!opt.load.empty()){a.push_back("--load");a.push_back(opt.load.string());}std::vector<char*>av;for(auto&v:a)av.push_back(v.data());av.push_back(nullptr);
        pid_t pid=fork();if(pid<0)throw std::runtime_error("Cannot start interpreter");if(pid==0){execv("/proc/self/exe",av.data());_exit(127);}int status=0;while(waitpid(pid,&status,0)<0){if(errno!=EINTR)throw std::runtime_error("Could not wait for interpreter");}int result=WIFEXITED(status)?WEXITSTATUS(status):1;if(result!=77)return result;
        opt.load=read_text(folder/"launch-request",4096);fs::remove(folder/"launch-request");opt.resume=false;
    }
}
int main(int argc,char**argv){try{
    auto p=paths();auto settings=read_settings(p);RunOptions opt;fs::path engine_path,inv,online_fixture;bool scan_only=false;
    for(int i=1;i<argc;i++){std::string a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing value for "+a);return std::string(argv[i]);};
        if(a=="--device")opt.device=true;else if(a=="--scale")opt.scale=std::stoi(value());else if(a=="--stories")settings.stories=fs::absolute(value());else if(a=="--engine")engine_path=fs::absolute(value());else if(a=="--resume")opt.resume=true;else if(a=="--load")opt.load=fs::absolute(value());else if(a=="--drive")opt.drive=fs::absolute(value());else if(a=="--capture")opt.capture=fs::absolute(value());else if(a=="--online-fixture")online_fixture=fs::absolute(value());else if(a=="--inventory"){inv=fs::absolute(value());scan_only=true;}else if(a=="--version"){std::cout<<"Frotz Zero "<<FROTZ_ZERO_VERSION<<"; Frotz 042d7bcadc1e2a8090cf737c841d8b7fc14b6eff\n";return 0;}else if(a=="--help"){std::cout<<"Frotz Zero [--stories DIRECTORY] [--device] [--scale 1..5]\n  --inventory FILE  Write local header/identity inventory and exit\n  --engine STORY [--resume | --load SAVE]  Direct shared-engine session\n  --drive SCRIPT --capture PNG  Deterministic integration-test driver\n";return 0;}else throw std::runtime_error("Unknown option: "+a);}
    if(!online_fixture.empty()&&opt.drive.empty())throw std::runtime_error("--online-fixture requires --drive");
    auto library_scan=[&](){clear_story_art();auto stories=scan(settings.stories);annotate_online(stories,p);return stories;};
    if(scan_only){auto all=library_scan();inventory(all,inv);std::cout<<all.size()<<" stories inventoried in "<<inv<<'\n';return 0;}
    if(!engine_path.empty()){auto story=inspect_story(engine_path);std::vector<Story> selected_story={story};annotate_online(selected_story,p);story=selected_story.front();if(!story.valid)throw std::runtime_error(story.issue);return run_engine(story,settings,p,opt);}
    std::unique_ptr<Display>d;auto open_display=[&](){d=std::make_unique<Display>(opt.device,opt.scale);d->theme(settings.theme=="dark");if(!opt.drive.empty()){d->load_drive(opt.drive);opt.drive.clear();}if(!opt.capture.empty()){d->capture_once=opt.capture;opt.capture.clear();}};open_display();
    auto all=library_scan();
    int selected=1,library_index=0;bool in_library=false;std::string chosen;
    while(true){
        int index=-1;
        for(size_t i=0;i<all.size();i++)if(all[i].id==settings.recent){index=int(i);break;}
        const Story* current=index>=0?&all[index]:nullptr;
        Generation resume;
        bool can_resume=false;
        if(current&&current->valid){
            auto folder=p.data/"stories"/current->id;
            resume=find_generation(folder/"resume",current->id);
            can_resume=bool(resume)&&!fs::exists(folder/"resume-unavailable");
        }
        auto action=in_library?HomeAction::Library:home_screen(*d,current,all.size(),can_resume,selected);
        if(!in_library)selected=static_cast<int>(action);
        StoryAction play_action=StoryAction::Continue;
        if(action==HomeAction::Exit)return 0;
        if(action==HomeAction::Settings){settings_menu(*d,settings,p);all=library_scan();continue;}
        if(action==HomeAction::Help){help_menu(*d);continue;}
        if(action==HomeAction::Downloads){chosen=online_library(*d,settings,p,online_fixture);all=library_scan();if(chosen.empty())continue;in_library=true;}
        if(action==HomeAction::Library||action==HomeAction::Downloads){
            in_library=true;
            int pick=-1;
            if(!chosen.empty())for(size_t i=0;i<all.size();i++)if(all[i].id==chosen){pick=int(i);break;}
            if(pick<0){
                chosen.clear();
                for(;;){
                    pick=story_picker(*d,all,library_index);
                    if(pick==-3){all=library_scan();continue;}
                    if(pick!=-2)break;
                    chosen=online_library(*d,settings,p,online_fixture);all=library_scan();
                    if(!chosen.empty()){for(size_t i=0;i<all.size();i++)if(all[i].id==chosen){pick=int(i);break;}if(pick>=0)break;}
                }
            }
            if(pick<0){in_library=false;chosen.clear();continue;}
            library_index=pick;current=&all[pick];chosen=current->id;
            auto folder=p.data/"stories"/current->id;
            resume=find_generation(folder/"resume",current->id);can_resume=bool(resume)&&!fs::exists(folder/"resume-unavailable");
            play_action=story_screen(*d,*current,can_resume);
            if(play_action==StoryAction::Back){chosen.clear();continue;}
        }
        if(!current){message(*d,"Choose an adventure","Open Library and choose a story. Home Continue resumes the last story you actually played.");continue;}
        auto story=*current;
        if(play_action==StoryAction::Delete){
            try{
                auto plan=plan_story_deletion(settings.stories,story);
                for(;;){
                    int choice=menu(*d,"Delete "+story.title+"?",{"Cancel","Delete "+std::to_string(plan.files.size())+" files permanently","Review file list"},"Saves and Continue kept; shared media kept");
                    if(choice<1)break;
                    if(choice==2){std::string files="Only these files will be removed:\n";for(const auto&[file,hash]:plan.files)files+="\n"+file.lexically_relative(plan.root).string();files+="\n\nSaved games and checkpoints stay on the Zero. Download or copy back the same release to use them again.";message(*d,"Files to delete",files);continue;}
                    auto count=delete_story_files(plan,p);
                    if(settings.recent==story.id){settings.recent.clear();write_settings(p,settings);}
                    chosen.clear();message(*d,"Title deleted",std::to_string(count)+" files removed. Saved games and checkpoints were kept. Add the same story release again to use them.");break;
                }
            }catch(const std::exception&e){message(*d,"Deletion stopped",e.what());chosen.clear();}
            all=library_scan();continue;
        }
        if(!story.valid){message(*d,"Story cannot be opened",story.issue);continue;}
        auto folder=p.data/"stories"/story.id;
        RunOptions run=opt;run.resume=false;run.load.clear();
        if(play_action==StoryAction::Continue){
            if(!can_resume){message(*d,"Continue unavailable",fs::exists(folder/"resume-unavailable")?read_text(folder/"resume-unavailable"):"No valid automatic resume. Choose New Game or Load Save. Existing files have been preserved.");continue;}
            run.resume=true;
            if(!resume.problem.empty())message(*d,"Checkpoint recovery",resume.problem);
        }
        if(play_action==StoryAction::NewGame){
            if(resume&&menu(*d,"Replace automatic resume?",{"Cancel","Start new game (keep named saves)"})!=1)continue;
            if(story.version==6&&!story.pictures){
                int media=menu(*d,"Graphics resources absent",{"Cancel","Try story without graphics","Resource help"});
                if(media==2)message(*d,"Matching media required","Place a matching Blorb beside this story. Embedded release identities are matched automatically even when filenames differ. Without an identity, use the same base filename only after checking media provenance. Check release and serial in Load Story's details. Original Infocom .mg1/.eg1 files need conversion with a separate tool; this frontend does not decode them. See docs/RESOURCES.md.");
                if(media!=1)continue;
            }
        }
        if(play_action==StoryAction::LoadSave){run.load=choose_save(*d,folder/"saves",story);if(run.load.empty())continue;}
        settings.recent=story.id;write_settings(p,settings);
        d.reset();
        int result=launch(story,p,run);
        if(result==78)return 0;
        settings=read_settings(p);all=library_scan();open_display();chosen.clear();
        selected=0;
        if(result){auto msg=fs::exists(folder/"last-error")?read_text(folder/"last-error"):"The story process stopped unexpectedly. Named saves and previous checkpoints are preserved.";message(*d,"Story stopped",msg);}
    }
}catch(const std::exception&e){std::cerr<<"Frotz Zero: "<<e.what()<<'\n';return 1;}}
