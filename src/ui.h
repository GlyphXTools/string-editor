#ifndef UI_H
#define UI_H

#include <map>
#include <string>
#include "types.h"

void EditList_GetPairs(HWND hWnd, std::map<LANGID,std::wstring>& pairs, std::map<LANGID,bool>& enabled);
void EditList_AddPair(HWND hWnd, LANGID language, const std::wstring& postfix);
bool EditList_Initialize(HINSTANCE hInstance);

#endif