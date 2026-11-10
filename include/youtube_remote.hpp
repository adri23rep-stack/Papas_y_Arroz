#pragma once
#ifdef _WIN32
#include <string>
namespace xs {
// The Windows system browser is the player; no scraping, stream extraction,
// background playback, or unapproved third-party player is involved.
std::string youtube_results_url(const std::string& query);
bool youtube_launch();
bool youtube_search(const std::string& query);
bool youtube_foreground();
bool youtube_input(const std::string& command,int dx=0,int dy=0);
}
#endif
