#pragma once
#include <common.h>

namespace chromatracker {

// all functions handle UTF-8
// why are these not in std? :(

void initLocale();
string toUpper(string s);
string toLower(string s);
string leftPad(string s, int length, char c='0');
string hex(int value);

} // namespace
