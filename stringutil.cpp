#include "stringutil.h"
#include <algorithm>
#include <exception>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utf8.h>

namespace chromatracker {

void initLocale()
{
    try {
        std::locale::global(std::locale("C.UTF-8")); // alt: en_US.UTF8
    } catch (std::exception e) {
        cout << "Failed to set locale\n";
        std::locale::global(std::locale(""));
    }
}

string toUpper(string s)
{
    string result;
    auto it = s.begin();
    try {
        while (it < s.end()) {
            uint32_t c = utf8::next(it, s.end());
            utf8::append((char32_t)std::toupper(c, std::locale()), result);
        }
    } catch (utf8::exception e) {} // TODO
    return result;
}

string toLower(string s)
{
    string result;
    auto it = s.begin();
    try {
        while (it < s.end()) {
            uint32_t c = utf8::next(it, s.end());
            utf8::append((char32_t)std::tolower(c, std::locale()), result);
        }
    } catch (utf8::exception e) {} // TODO
    return result;
}

string leftPad(string s, int length, char c)
{
    int sDist = utf8::distance(s.begin(), s.end());
    if (length > sDist)
        s.insert(s.begin(), length - sDist, c);
    return s;
}

string hex(int value)
{
    std::stringstream stream;
    stream << std::hex << value;
    return stream.str();
}

} // namespace
