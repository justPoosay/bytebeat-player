#define _CRT_SECURE_NO_WARNINGS
#include "GlobalState.h"
#include <cstring>

AppState state;

AppState::AppState() {
    const char* defaultCode = "";
    strncpy(inputBuf, defaultCode, sizeof(inputBuf) - 1);
    inputBuf[sizeof(inputBuf) - 1] = '\0';
}