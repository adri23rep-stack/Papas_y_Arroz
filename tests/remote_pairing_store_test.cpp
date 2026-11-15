#define WIN32_LEAN_AND_MEAN
#include "remote_pairing_store.hpp"
#include <cassert>
#include <fstream>
#include <string>
int main(){
    using namespace xs;
    assert(remote_valid_secret(std::string(64,'a')));
    assert(!remote_valid_secret(std::string(64,'z')));
    assert(!remote_valid_secret("abc"));
    wchar_t temp[MAX_PATH]{};
    const DWORD length=GetTempPathW(MAX_PATH,temp);
    assert(length>0&&length<MAX_PATH);
    const auto folder=std::filesystem::path(temp)/
        (L"XuperTV-qr-test-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    assert(SetEnvironmentVariableW(L"XUPER_PAIRING_TEST_DIR",folder.c_str())!=0);
    const auto file=remote_pairing_path();
    std::error_code ec;std::filesystem::remove(file,ec);
    const std::string a(64,'a'),b(64,'b');
    assert(remote_save_secret_file(file,a));
    std::string out;
    assert(remote_read_secret_file(file,out)&&out==a);
    assert(remote_load_or_create_secret(out,b,out)&&out==a);
    // DPAPI ciphertext cannot be mistaken for a plaintext bearer token.
    std::ifstream input(file,std::ios::binary);
    const std::string ciphertext((std::istreambuf_iterator<char>(input)),
                                 std::istreambuf_iterator<char>());
    assert(ciphertext!=a&&ciphertext.find(a)==std::string::npos);
    std::filesystem::remove_all(folder,ec);
    SetEnvironmentVariableW(L"XUPER_PAIRING_TEST_DIR",nullptr);
    return 0;
}
