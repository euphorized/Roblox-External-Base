#include "cache/cache.h"

// < global >
_cache cache;

auto main() -> int
{
	// < thread >
	std::thread __static(&_cache::_static, &cache);

	// < detach >
	__static.detach();	
	cache._transform();
}