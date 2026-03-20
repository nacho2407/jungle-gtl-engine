#pragma once

#include "ThirdParty/json.h"

class ISerializable
{
public:
	virtual json::JSON Serialize() = 0;
	virtual void Deserialize(json::JSON) = 0;

	virtual ~ISerializable() = default;
};