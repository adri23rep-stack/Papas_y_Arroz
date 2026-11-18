#include "trace.hpp"
#include <cstdio>
#include <mutex>
namespace xs {
#ifdef __SWITCH__
static const char* path="sdmc:/switch/xuperswitch/diagnostico.log";
#else
static const char* path="diagnostico.log";
#endif
static unsigned sequence=0;
static std::mutex trace_mutex;
void trace_reset(){std::lock_guard<std::mutex> lock(trace_mutex);sequence=0;if(FILE* f=std::fopen(path,"w")){std::fputs("XuperSwitch 0.14.1 - etapas; sin datos de cuenta\n",f);std::fclose(f);}}
void trace(const char* stage){std::lock_guard<std::mutex> lock(trace_mutex);if(FILE* f=std::fopen(path,"a")){std::fprintf(f,"%u %s\n",++sequence,stage);std::fclose(f);}}
}
