#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

// Mouse/remote-friendly rail geometry, independent of the streaming decoder.
// A short click switches to a channel; hold reveals an information preview;
// dragging and the mouse wheel scroll without accidentally switching streams.
namespace xs {
class ChannelRail {
public:
    static constexpr int x=24,y=561,width=205,height=119,gap=14,viewport=1231;
    static constexpr uint32_t hold_ms=550;
    explicit ChannelRail(size_t count=0,size_t current=0):count_(count),focus_(std::min(current,count?count-1:0)){
        center_on(focus_);
    }
    bool available()const{return count_>1;}
    int offset()const{return offset_;}
    size_t focus()const{return focus_;}
    bool pressed()const{return pressed_index_>=0;}
    bool dragging()const{return dragged_;}
    void center_on(size_t index){
        if(!count_)return;
        focus_=std::min(index,count_-1);
        offset_=std::clamp(static_cast<int>(focus_)*(width+gap)-viewport/2+width/2,0,max_offset());
    }
    void move_focus(int delta){
        if(!count_)return;
        const int index=std::clamp(static_cast<int>(focus_)+delta,0,static_cast<int>(count_)-1);
        focus_=static_cast<size_t>(index);
        const int left=index*(width+gap),right=left+width;
        if(left<offset_)offset_=left;
        if(right>offset_+viewport)offset_=right-viewport;
        offset_=std::clamp(offset_,0,max_offset());
    }
    void wheel(int delta){offset_=std::clamp(offset_-delta*(width+gap),0,max_offset());}
    bool contains(int px,int py)const{return px>=x&&px<x+viewport&&py>=y&&py<y+height;}
    int hit(int px,int py)const{
        if(!contains(px,py))return -1;
        const int local=px-x+offset_,index=local/(width+gap);
        if(local%(width+gap)>=width||index<0||static_cast<size_t>(index)>=count_)return -1;
        return index;
    }
    void press(int px,int py,uint32_t now){
        pressed_index_=hit(px,py);down_x_=px;last_x_=px;down_time_=now;dragged_=false;
    }
    void motion(int px){
        if(pressed_index_<0)return;
        if(std::abs(px-down_x_)>9)dragged_=true;
        if(dragged_)offset_=std::clamp(offset_+(last_x_-px),0,max_offset());
        last_x_=px;
    }
    int preview(uint32_t now)const{
        return pressed_index_>=0&&!dragged_&&now-down_time_>=hold_ms?pressed_index_:-1;
    }
    int release(int px,int py,uint32_t now){
        const int target=hit(px,py);
        const int result=!dragged_&&pressed_index_>=0&&target==pressed_index_&&now-down_time_<hold_ms?target:-1;
        if(result>=0)focus_=static_cast<size_t>(result);
        pressed_index_=-1;dragged_=false;return result;
    }
private:
    size_t count_=0,focus_=0;
    int offset_=0,pressed_index_=-1,down_x_=0,last_x_=0;
    uint32_t down_time_=0;
    bool dragged_=false;
    int max_offset()const{
        const int total=static_cast<int>(count_)*(width+gap)-gap;
        return std::max(0,total-viewport);
    }
};
}
