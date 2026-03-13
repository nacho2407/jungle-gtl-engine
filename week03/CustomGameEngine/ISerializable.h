#pragma once

#include "json.h"

class ISerializable
{
public:
	virtual json::JSON Serialize() = 0;
	virtual void Deserialize(json::JSON) = 0;

	virtual ~ISerializable() = default;
};