#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace xs {
// The pairing token belongs to the Windows user, NOT the portable EXE folder.
// Existing portable installs automatically migrate their old secret once.
inline bool remote_valid_secret(const std::string& token){
    return token.size()==64&&std::all_of(token.begin(),token.end(),[](unsigned char c){
        return std::isxdigit(c)!=0;
    });
}
inline std::filesystem::path remote_pairing_root(){
    // CI tests opt into an isolated temporary directory; normal users ALWAYS
    // get their account's known LocalAppData folder.
    wchar_t test_dir[32768]{};
    const DWORD n=GetEnvironmentVariableW(L"XUPER_PAIRING_TEST_DIR",test_dir,32768);
    if(n>0&&n<32768)return std::filesystem::path(test_dir);
    PWSTR directory=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DEFAULT,nullptr,&directory))
       ||!directory)return {};
    std::filesystem::path path(directory);
    CoTaskMemFree(directory);
    return path/L"XuperTV";
}
inline std::filesystem::path remote_pairing_path(){
    const auto directory=remote_pairing_root();
    return directory.empty()?std::filesystem::path{}:directory/L"pairing.dat";
}
inline bool remote_read_secret_file(const std::filesystem::path& filename,
                                    std::string& out){
    std::error_code ec;
    if(!std::filesystem::is_regular_file(filename,ec)||
       std::filesystem::file_size(filename,ec)>4096)return false;
    std::ifstream stream(filename,std::ios::binary);
    if(!stream)return false;
    std::vector<BYTE> bytes((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
    if(bytes.empty()||bytes.size()>4096)return false;
    DATA_BLOB input{static_cast<DWORD>(bytes.size()),bytes.data()},output{};
    if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN,&output))return false;
    out.assign(reinterpret_cast<const char*>(output.pbData),output.cbData);
    if(output.pbData)LocalFree(output.pbData);
    return remote_valid_secret(out);
}
inline bool remote_save_secret_file(const std::filesystem::path& filename,
                                    const std::string& token){
    if(filename.empty()||!remote_valid_secret(token))return false;
    std::error_code ec;
    std::filesystem::create_directories(filename.parent_path(),ec);
    if(ec)return false;
    DATA_BLOB input{static_cast<DWORD>(token.size()),
                    reinterpret_cast<BYTE*>(const_cast<char*>(token.data()))},output{};
    if(!CryptProtectData(&input,L"XuperTV local pairing secret",nullptr,nullptr,
                         nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))return false;
    const auto tmp=std::filesystem::path(filename.wstring()+L".tmp");
    bool ok=false;
    {
        std::ofstream stream(tmp,std::ios::binary|std::ios::trunc);
        if(stream){
            stream.write(reinterpret_cast<const char*>(output.pbData),output.cbData);
            stream.flush();ok=stream.good();
        }
    }
    if(output.pbData)LocalFree(output.pbData);
    if(ok){
        ok=MoveFileExW(tmp.c_str(),filename.c_str(),
                        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    }
    if(!ok)std::filesystem::remove(tmp,ec);
    return ok;
}
inline bool remote_load_or_create_secret(std::string& out,
                                        const std::string& proposed,
                                        std::string& error){
    const auto path=remote_pairing_path();
    if(path.empty()){error="No se pudo encontrar la carpeta de usuario de Windows.";return false;}
    std::error_code ec;
    if(std::filesystem::exists(path,ec)){
        if(remote_read_secret_file(path,out))return true;
        error="No se pudo leer la vinculacion protegida. No se reemplazara automaticamente.";
        return false;
    }
    // Import the former per-installation secret before generating another,
    // preventing previously linked phones from losing access on upgrade.
    std::ifstream old("remote_secret.txt",std::ios::binary);
    std::string legacy;
    std::getline(old,legacy);
    old.close(); // Windows must release the legacy file before removing it.
    // Legacy builds used text-mode streams. Accept both LF and Windows CRLF,
    // but only after removing final line terminators (never arbitrary data).
    while(!legacy.empty()&&(legacy.back()=='\r'||legacy.back()=='\n'))
        legacy.pop_back();
    const bool migrating=remote_valid_secret(legacy);
    const std::string token=migrating?legacy:proposed;
    if(!remote_save_secret_file(path,token)){
        error="No se pudo guardar la clave permanente del control.";
        return false;
    }
    out=token;
    if(migrating){
        // Old plaintext key is no longer needed after encrypted migration.
        // Remove only a validated legacy file in the current install folder.
        std::filesystem::remove("remote_secret.txt",ec);
    }
    return true;
}
}
#endif
