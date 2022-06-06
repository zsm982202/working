#pragma once

#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>

namespace util {
	int64_t NowMs(); //计算从1900年开始到现在的毫秒数
}
