#pragma once
#include <string>

#ifdef __ANDROID__
bool AndroidPrepareDataRoot(std::string& outRoot);
#endif
