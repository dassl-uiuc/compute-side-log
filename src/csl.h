#pragma once

#ifndef O_CSL
# define O_CSL 040000000
#endif

#define __IS_COMP_SIDE_LOG(flags) (((flags) & O_CSL) != 0)
