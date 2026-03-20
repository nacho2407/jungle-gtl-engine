#pragma once
#include "CoreTypes.h"
#include "Object/Object.h"

namespace common {
	namespace structs {
		struct FViewOutput {
			int InternalIndex = 0;
			std::string ObjectPicked = "";
			UObject* Object = nullptr;
		};
	}

	namespace constants {
		namespace ImGui {
			constexpr float NotificationTimer = 1.5f;
		}
	}
}
