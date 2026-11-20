// SPDX-License-Identifier: GPL-3.0-or-later
#include "artwork.hpp"
#include <algorithm>
#include <png.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <deque>
#include <csetjmp>
#include <cstdio>
#include <jpeglib.h>
#include "artwork.inc"
namespace fz {
namespace {
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
Surface decode(const unsigned char* bytes,size_t length) {
    png_image image{};image.version=PNG_IMAGE_VERSION;
    if(!png_image_begin_read_from_memory(&image,bytes,length))throw std::runtime_error("Cannot read bundled artwork");
    if(!image.width||!image.height||image.width>4096||image.height>4096||uint64_t(image.width)*image.height>8*1024*1024){png_image_free(&image);throw std::runtime_error("Artwork dimensions exceed limit");}
    image.format=PNG_FORMAT_RGBA;
    Surface surface(SDL_CreateRGBSurfaceWithFormat(0,image.width,image.height,32,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);
    if(!surface){png_image_free(&image);throw std::runtime_error(SDL_GetError());}
    bool ok=png_image_finish_read(&image,nullptr,surface->pixels,surface->pitch,nullptr);
    png_image_free(&image);
    if(!ok)throw std::runtime_error("Cannot decode bundled artwork");
    SDL_SetSurfaceBlendMode(surface.get(),SDL_BLENDMODE_BLEND);
    return surface;
}
struct JpegState {jpeg_decompress_struct codec{};jpeg_error_mgr error{};std::jmp_buf jump;SDL_Surface* surface=nullptr;unsigned char* row=nullptr;};
void jpeg_error(j_common_ptr info){auto*s=static_cast<JpegState*>(info->client_data);std::longjmp(s->jump,1);}
Surface decode_jpeg(const std::vector<uint8_t>&bytes){
    auto s=std::make_unique<JpegState>();s->codec.err=jpeg_std_error(&s->error);s->error.error_exit=jpeg_error;s->codec.client_data=s.get();
    if(setjmp(s->jump)){if(s->surface)SDL_FreeSurface(s->surface);free(s->row);jpeg_destroy_decompress(&s->codec);return Surface(nullptr,SDL_FreeSurface);}
    jpeg_create_decompress(&s->codec);jpeg_mem_src(&s->codec,bytes.data(),bytes.size());jpeg_read_header(&s->codec,TRUE);
    if(!s->codec.image_width||!s->codec.image_height||s->codec.image_width>4096||s->codec.image_height>4096||uint64_t(s->codec.image_width)*s->codec.image_height>8*1024*1024){jpeg_destroy_decompress(&s->codec);return Surface(nullptr,SDL_FreeSurface);}
    s->codec.out_color_space=JCS_RGB;jpeg_start_decompress(&s->codec);
    s->surface=SDL_CreateRGBSurfaceWithFormat(0,s->codec.output_width,s->codec.output_height,32,SDL_PIXELFORMAT_RGBA32);
    s->row=static_cast<unsigned char*>(malloc(size_t(s->codec.output_width)*3));
    if(!s->surface||!s->row){jpeg_error(reinterpret_cast<j_common_ptr>(&s->codec));}
    while(s->codec.output_scanline<s->codec.output_height){auto y=s->codec.output_scanline;JSAMPROW rows[]={s->row};jpeg_read_scanlines(&s->codec,rows,1);auto dst=static_cast<uint8_t*>(s->surface->pixels)+y*s->surface->pitch;for(unsigned x=0;x<s->codec.output_width;x++){dst[x*4]=s->row[x*3];dst[x*4+1]=s->row[x*3+1];dst[x*4+2]=s->row[x*3+2];dst[x*4+3]=255;}}
    jpeg_finish_decompress(&s->codec);jpeg_destroy_decompress(&s->codec);free(s->row);
    return Surface(s->surface,SDL_FreeSurface);
}
struct Cover {std::string key;Surface image{nullptr,SDL_FreeSurface};};
std::deque<Cover> covers;
// Small, consistent pixel strokes; no font glyph or extra drawing dependency.
void line(Display&d,int x,int y,int ex,int ey,uint32_t c) {
    int dx=std::abs(ex-x),sx=x<ex?1:-1,dy=-std::abs(ey-y),sy=y<ey?1:-1,err=dx+dy;
    for(;;){d.rect(x,y,1,1,c);if(x==ex&&y==ey)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
}
void clear_story_art(){covers.clear();}
void story_art(Display&d,const Story&s,int x,int y,int size){
    auto key=s.path.string()+"|"+s.resource.string();auto found=std::find_if(covers.begin(),covers.end(),[&](const auto&c){return c.key==key;});
    if(found==covers.end()){
        Cover cover;cover.key=key;
        try{auto data=read_story_cover(s);if(data.size()>8){auto image=data[0]==0xff&&data[1]==0xd8?decode_jpeg(data):decode(data.data(),data.size());if(image){
            // Cache only a small thumbnail, never a collection of full-size covers.
            cover.image=Surface(SDL_CreateRGBSurfaceWithFormat(0,96,96,32,SDL_PIXELFORMAT_RGBA32),SDL_FreeSurface);
            if(cover.image){SDL_FillRect(cover.image.get(),nullptr,SDL_MapRGBA(cover.image->format,0,0,0,0));double fit=std::min(96.0/image->w,96.0/image->h);int w=std::max(1,int(image->w*fit)),h=std::max(1,int(image->h*fit));SDL_Rect to{(96-w)/2,(96-h)/2,w,h};SDL_SetSurfaceBlendMode(image.get(),SDL_BLENDMODE_NONE);SDL_BlitScaled(image.get(),nullptr,cover.image.get(),&to);SDL_SetSurfaceBlendMode(cover.image.get(),SDL_BLENDMODE_BLEND);}
        }}}catch(...){}
        if(covers.size()>=8)covers.pop_front();covers.push_back(std::move(cover));found=std::prev(covers.end());
    }
    d.rounded_rect(x,y,size,size,7,d.panel);
    if(found->image){SDL_Rect to{x,y,size,size};SDL_BlitScaled(found->image.get(),nullptr,d.canvas,&to);return;}
    // Original CC0 miniature landscapes, varied deterministically for each title.
    uint32_t seed=2166136261u;for(unsigned char c:s.title){seed^=c;seed*=16777619u;}
    const uint32_t shades[]={0xff264c60,0xff3d405f,0xff305852,0xff644c42,0xff314c70,0xff58435c};
    uint32_t sky=shades[seed%6];auto box=[&](int a,int b,int w,int h,uint32_t c){d.rect(x+a*size/96,y+b*size/96,std::max(1,w*size/96),std::max(1,h*size/96),c);};
    d.rounded_rect(x,y,size,size,7,sky);
    for(int i=0;i<9;i++){seed=seed*1664525u+1013904223u;box(8+seed%78,8+(seed>>8)%42,1,1,0xffe7dcb8);}
    d.rounded_rect(x+size*65/96,y+size*14/96,size*16/96,size*16/96,size*8/96,0xfff2cf89);
    for(int col=3;col<93;col++){int ridge=55+std::abs(48-col)/3;box(col,ridge,1,93-ridge,0xff1b343e);}
    int motif=(seed>>16)%4;
    if(motif==0){box(29,37,13,46,0xffd6be8e);box(49,46,17,37,0xffc4a878);box(27,34,17,5,0xffe5c88f);box(53,57,5,7,0xff18303e);box(33,48,4,8,0xff18303e);box(33,70,5,13,0xff18303e);}
    else if(motif==1){for(int row=0;row<25;row++)box(47-row/2,34+row,3+row,1,0xff72b6a0);box(46,59,5,24,0xffd7bc85);box(39,82,18,2,0xff72b6a0);}
    else if(motif==2){box(29,48,35,35,0xff558c88);box(34,53,25,30,0xff142c3c);box(43,61,8,22,0xffebc97b);box(24,83,45,3,0xffd6be8e);}
    else{box(44,35,4,44,0xffd6be8e);for(int row=0;row<28;row++)box(49,38+row,27-row,1,0xffe7d6b3);for(int row=0;row<8;row++)box(27+row,77+row,43-row*2,1,0xff78bbae);}
    box(12,89,72,1,0xffe2c087);
}
void brand(Display&d,int x,int y,int size) {
    static const std::array<Surface,3> images={decode(brand_24,sizeof brand_24),decode(brand_40,sizeof brand_40),decode(brand_96,sizeof brand_96)};
    int index=size==24?0:size==40?1:2;
    auto surface=images[index].get();SDL_Rect target{x,y,surface->w,surface->h};
    SDL_BlitSurface(surface,nullptr,d.canvas,&target);
}
void glyph(Display&d,Glyph g,int x,int y,uint32_t c) {
    // Original menu pictogram geometry: CC0-1.0.
    auto stroke=[&](int a,int b,int e,int f){line(d,x+a,y+b,x+e,y+f,c);};
    auto box=[&](int a,int b,int w,int h){stroke(a,b,a+w,b);stroke(a,b,a,b+h);stroke(a+w,b,a+w,b+h);stroke(a,b+h,a+w,b+h);};
    switch(g){
    case Glyph::Continue:
        for(int i=0;i<9;i++)d.rect(x+4+i,y+2+i/2,1,12-i,c);break;
    case Glyph::Library:
        box(1,3,6,10);box(8,3,6,10);stroke(7,4,7,15);stroke(3,6,5,6);stroke(10,6,12,6);break;
    case Glyph::NewGame:
        stroke(8,0,8,15);stroke(0,8,15,8);stroke(3,3,12,12);stroke(3,12,12,3);d.rect(x+6,y+6,5,5,c);break;
    case Glyph::LoadSave:
        stroke(1,5,1,14);stroke(1,14,14,14);stroke(14,14,14,5);stroke(1,5,5,5);stroke(5,5,6,7);stroke(6,7,10,7);
        stroke(10,0,10,10);stroke(7,7,10,10);stroke(13,7,10,10);break;
    case Glyph::Settings:
        for(int i=0;i<3;i++){int a=3+i*5,b=i==1?10:4;stroke(a,1,a,14);d.rect(x+a-2,y+b,5,3,c);}break;
    case Glyph::Refresh:
        stroke(3,3,11,3);stroke(11,3,14,6);stroke(14,6,14,9);stroke(14,9,12,12);stroke(12,12,4,12);stroke(4,12,1,9);stroke(1,9,1,6);
        stroke(1,1,1,5);stroke(1,5,5,5);stroke(14,14,14,10);stroke(14,10,10,10);break;
    case Glyph::Help:
        stroke(5,2,10,2);stroke(10,2,12,4);stroke(12,4,12,6);stroke(12,6,8,9);stroke(8,9,8,11);d.rect(x+7,y+14,2,2,c);stroke(3,4,5,2);break;
    case Glyph::Exit:
        stroke(7,1,1,1);stroke(1,1,1,14);stroke(1,14,7,14);stroke(6,8,15,8);stroke(12,5,15,8);stroke(12,11,15,8);break;
    }
}
void page_header(Display&d,const std::string&title,const std::string&trailing) {
    brand(d,7,3,24);
    int space=trailing.empty()?273:223;
    d.text(37,5,title,d.fg,17,space);
    if(!trailing.empty())d.text(310-d.text_width(trailing,10),10,trailing,d.muted,10,45);
    d.rect(9,31,302,1,d.panel);d.rect(9,31,24,1,d.gold);
}
}
