#include "../fx.cpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
using namespace opxy;
void require(bool value, const char* text) { if (!value) { std::fprintf(stderr, "%s\n", text); std::exit(1); } }
int main() {
    graindelay::State state; graindelay::initialize(state);
    std::vector<i32> memory(graindelay::buffer_words, 0x55555555);
    Buffer buffer{memory.data(), static_cast<u32>(memory.size())};
    i32 l[64], r[64], p[8] = {0,0,0x7fffffff,0,0x3fffffff,0,0,0x7fffffff};
    for (u32 block = 0; block < 1200; ++block) {
        for (u32 i=0;i<64;++i) l[i]=r[i]=0;
        graindelay::process(state,buffer,l,r,p);
        for (u32 i=0;i<64;++i) require(!l[i]&&!r[i],"unwritten/reset history must be silent through ring wrap");
    }
    graindelay::initialize(state);
    u32 first=0; i32 peak=0;
    for(u32 block=0;block<40;++block) {
        for(u32 i=0;i<64;++i) { l[i]=block==0&&i==0?1<<28:0; r[i]=0; }
        graindelay::process(state,buffer,l,r,p);
        for(u32 i=0;i<64;++i) {
            require(r[i]==0,"left impulse must not leak to right");
            if(l[i]&&!first) first=block*64+i;
            if(l[i]>peak) peak=l[i];
        }
    }
    require(first==884 && peak>1<<25,"unity pitch grain stream must produce correct delayed impulse");
    graindelay::initialize(state);
    for(u32 block=0;block<1300;++block) {
        for(u32 i=0;i<64;++i) { l[i]=1<<27; r[i]=1<<26; }
        graindelay::process(state,buffer,l,r,p);
        if(block>1100) for(u32 i=0;i<64;++i) {
            require(l[i]>60000000&&l[i]<70000000,"overlap windows must settle without periodic holes after wrap");
            require(std::abs(l[i]-2*r[i])<32,"stereo balance must survive window mixing");
        }
    }
    p[3]=0x7fffffff;
    for(u32 block=0;block<2000;++block) {
        for(u32 i=0;i<64;++i) l[i]=r[i]=0;
        graindelay::process(state,buffer,l,r,p);
        if(block>1900) for(u32 i=0;i<64;++i) require(std::abs(l[i])<128&&std::abs(r[i])<128,"maximum feedback must decay");
    }
    for(u32 setting=0;setting<5;++setting) {
        graindelay::initialize(state);
        p[4]=static_cast<i32>((static_cast<i64>(2*setting+1)*0x7fffffff)/10);
        p[0]=p[1]=p[5]=p[6]=0x7fffffff;
        for(u32 block=0;block<1800;++block) {
            for(u32 i=0;i<64;++i) { l[i]=(i&1)?0x7fffffff:(-0x7fffffff-1); r[i]=-(l[i]/2); }
            graindelay::process(state,buffer,l,r,p);
        }
        require(state.written==graindelay::history_frames,"history count bounded at all pitch extremes");
        graindelay::initialize(state);
        for(u32 i=0;i<64;++i) l[i]=r[i]=0;
        graindelay::process(state,buffer,l,r,p);
        for(u32 i=0;i<64;++i) require(!l[i]&&!r[i],"reset must reject previously full loud memory");
    }
    for(u32 i=0;i<64;++i) l[i]=r[i]=1<<30;
    graindelay::process(state,Buffer{nullptr,0},l,r,p);
    for(u32 i=0;i<64;++i) require(!l[i]&&!r[i],"missing scratch produces silence");
    return 0;
}
