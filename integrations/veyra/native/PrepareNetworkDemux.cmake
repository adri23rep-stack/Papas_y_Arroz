# Generate a build-only copy of the pinned source. Keep the user's checkout intact.
file(READ "${INPUT}" demux)
set(anchor "        networkMediaOptions(&options);")
string(FIND "${demux}" "${anchor}" pos)
if(pos LESS 0)
  message(FATAL_ERROR "Unrecognized Veyra network demux source")
endif()
set(verified [=[
        if (name.find('?') != std::string::npos) {
            if (_wcsnicmp(path_.c_str(), L"https://", 8) != 0 ||
                av_dict_set(&options, "tls_verify", "1", 0) < 0 ||
                av_dict_set(&options, "protocol_whitelist", "https,tls,tcp,crypto", 0) < 0) {
                av_dict_free(&options);
                avformat_free_context(context);
                return false;
            }
        }
]=])
string(REPLACE "${anchor}" "${anchor}\n${verified}" demux "${demux}")
file(WRITE "${OUTPUT}" "${demux}")
