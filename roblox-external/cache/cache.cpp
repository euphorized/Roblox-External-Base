#include "cache.h"
#include <chrono>
#include <iostream>

bool _cache::_static()
{
	while (true)
	{
		std::lock_guard<std::mutex> lock_that_shit(_mutex);

		// < attach >
		bool attach = mem::get().attach(L"RobloxPlayerBeta.exe");
		if (!attach)
		{
			std::printf("couldn't attach to roblox");
			return 1;
		}
		
		// < clear >
		_parts.clear();

		// < cache >
		_base = mem::get().base;
		_fakedm = mem::get().read<uintptr_t>(_base + Offsets::FakeDataModel::Pointer);
		_datamodel = mem::get().read<uintptr_t>(_fakedm + Offsets::FakeDataModel::RealDataModel);
		_players = mem::get().find_child(_datamodel, "Players");
		auto _list = mem::get().get_children(_players);

		// < part >
		for (auto players : _list)
		{
			uintptr_t model_inst = mem::get().read<uintptr_t>(players + Offsets::Player::ModelInstance);
			auto parts = mem::get().get_children(model_inst);

			// k so we need to get the parts either by their name, or class name.
			for (auto __parts : parts)
			{
				// we do class name.
				uintptr_t name_zZz = mem::get().read<uintptr_t>(__parts + Offsets::Instance::Name);
				uintptr_t class_desc = mem::get().read<uintptr_t>(__parts + Offsets::Instance::ClassDescriptor);
				uintptr_t class_name = mem::get().read<uintptr_t>(class_desc + Offsets::Instance::ClassName);

				std::string _part_name = mem::get().read_string(name_zZz);
				std::string _class_name = mem::get().read_string(class_name);

				if (_class_name != "Part" && _class_name != "MeshPart")
					continue;

				// < debug >
				// std::printf("parts -> %s\n", _part_name.c_str());
				// std::printf("class name -> %s\n", _class_name.c_str());

				// < push >
				_parts.push_back({ __parts , _part_name });
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

// < parts are cached, now positions. >

void _cache::_transform()
{
	while (true)
	{
		std::lock_guard<std::mutex> lock_nigger(_mutex);

		auto parts = _parts.begin();
		auto end = _parts.end();

		// < clear >
		_position.clear();

		for (const auto& part : _parts)
		{
			// < debug >
			// std::printf("%s\n", part.c_str());
			
			// < chain >
			uintptr_t primitive = mem::get().read<uintptr_t>(part.first + Offsets::BasePart::Primitive);
			vector3_t NIGGA_POS = mem::get().read<vector3_t>(primitive + Offsets::Primitive::Position);
			
			_position.push_back({ NIGGA_POS });
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		system("cls");

		std::printf("base -> 0x%p\n", _base);
		std::printf("datamodel -> 0x%p\n", _datamodel);
		std::printf("players -> 0x%p\n", _players);
	}
}