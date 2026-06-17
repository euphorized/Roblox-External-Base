#include <shared_mutex>
#include "types.h"
#include "../memory/mem.h"
#include "../offsets.h"
#include <unordered_map>
#include <vector>

struct _cache
{
	// < globals >
	uint64_t _base;
	uintptr_t _fakedm;
	uintptr_t _datamodel;
	
	// < services > 
	uintptr_t _players;

	// < transform & names >
	std::unordered_map<uintptr_t, std::string> _names;
	std::vector<std::pair<uintptr_t, std::string>> _parts;
	std::vector<vector3_t> _position;

	bool _static();
	void _transform();

	// < safety >
	std::mutex _mutex;
};


/* < task > */
// cache all the players positions and their parts