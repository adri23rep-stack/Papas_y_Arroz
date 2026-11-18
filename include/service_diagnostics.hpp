#pragma once
#include <string>
namespace xs {
inline std::string service_return_code(const std::string& code){
    // A server-controlled field is not safe merely because it has no newlines.
    // Only known symbolic codes and fixed-format portal codes may reach the UI.
    if(code=="permission-denied"||code=="denied")return code;
    if(code.size()==12&&code.compare(0,6,"portal")==0&&
       code.find_first_not_of("0123456789",6)==std::string::npos)return code;
    return "no reconocido";
}
}
