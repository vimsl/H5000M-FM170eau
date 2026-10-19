#ifndef AIQOS_COMPAT_MKDIR_H
#define AIQOS_COMPAT_MKDIR_H
#ifdef _WIN32
#include <direct.h>
#ifndef mkdir
#define mkdir(path, mode) _mkdir(path)
#endif
#endif
#endif