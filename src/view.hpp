// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL.h>
#include <algorithm>
#include <utility>
namespace fz {
enum class ViewMode { Reader, Fit, Fill, Width, Pixels, Zoom, Framed, Automatic };
inline const char* view_name(ViewMode m){switch(m){case ViewMode::Reader:return "Reader";case ViewMode::Fit:return "Fit screen";case ViewMode::Fill:return "Fill screen";case ViewMode::Width:return "Fit width";case ViewMode::Pixels:return "Actual pixels";case ViewMode::Framed:return "Frame + text zoom";case ViewMode::Automatic:return "Automatic";default:return "Reading zoom";}}
struct Viewport {
    SDL_Rect source, target;
    std::pair<int,int> story_point(int x,int y) const {
        x=std::clamp(x-target.x,0,target.w-1);
        y=std::clamp(y-target.y,0,target.h-1);
        return {source.x+x*source.w/target.w,source.y+y*source.h/target.h};
    }
    std::pair<int,int> screen_point(int x,int y) const {
        return {target.x+(x-source.x)*target.w/source.w,target.y+(y-source.y)*target.h/source.h};
    }
};
inline Viewport make_viewport(int width,int height,double scale,int pan_x,int pan_y){
    scale=std::clamp(scale,0.15,4.0);
    int visible_w=std::min(width,int(320/scale)),visible_h=std::min(height,int(170/scale));
    int shown_w=int(visible_w*scale),shown_h=int(visible_h*scale);
    return {{std::clamp(pan_x,0,width-visible_w),std::clamp(pan_y,0,height-visible_h),visible_w,visible_h},
            {(320-shown_w)/2,(170-shown_h)/2,shown_w,shown_h}};
}
inline Viewport make_viewport(int width,int height,ViewMode mode,double zoom,int pan_x,int pan_y){
    if(mode==ViewMode::Fill||mode==ViewMode::Framed)return {{0,0,width,height},{0,0,320,170}};
    if(mode==ViewMode::Fit||mode==ViewMode::Reader||mode==ViewMode::Automatic){
        double fit=std::min(320.0/width,170.0/height);
        int w=std::max(1,int(width*fit)),h=std::max(1,int(height*fit));
        return {{0,0,width,height},{(320-w)/2,(170-h)/2,w,h}};
    }
    return make_viewport(width,height,mode==ViewMode::Width?320.0/width:mode==ViewMode::Pixels?1.0:zoom,pan_x,pan_y);
}
// Keep the outer layout fixed. Only the game's main text window is magnified.
inline Viewport framed_viewport(int width,int height,SDL_Rect window,double zoom,int pan_x,int pan_y){
    int x=(window.x*320+width-1)/width,y=(window.y*170+height-1)/height;
    int right=(window.x+window.w)*320/width,bottom=(window.y+window.h)*170/height;
    SDL_Rect target{x,y,std::max(1,right-x),std::max(1,bottom-y)};
    int w=std::clamp(int(target.w/std::clamp(zoom,0.15,4.0)),1,window.w);
    int h=std::clamp(int(target.h/std::clamp(zoom,0.15,4.0)),1,window.h);
    return {{std::clamp(pan_x,window.x,window.x+window.w-w),std::clamp(pan_y,window.y,window.y+window.h-h),w,h},target};
}
}
