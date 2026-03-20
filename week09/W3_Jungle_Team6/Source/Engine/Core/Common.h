#pragma once
#include "CoreTypes.h"
#include "Object/Object.h"

namespace common {
	namespace structs {
		struct FViewOutput {
			int InternalIndex = 0;
			std::string ObjectPicked = "";
			UObject* Object;
		};
	}

	namespace constants {
		namespace ImGui {
			const float NotificationTimer = 1.5f;
		}
	}
}
