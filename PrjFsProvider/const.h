#pragma once
#define __DIRECTORY_WORKAROUND__

#ifdef __DIRECTORY_WORKAROUND__
#define SHADOW_FILE_SUFFIX L"_MOVE"
#endif

#define PATH_BUFF_LEN (256)
#define DIRECTORY_SEP_PATH L"\\"
#define ENTER_DIRECTORY_PATH L"\\*"

#define SrcName LR"(B:\src)"
#define DstName LR"(A:\dst27)"