#pragma once

#include <string>
#include <cstdint>

class UObject
{
public:

    UObject();
    virtual ~UObject();

public:

    // Unique Object ID
    uint64_t GetID() const;

    // Object Name
    const std::string& GetName() const;
    void SetName(const std::string& InName);

    // Owner Object
    UObject* GetOuter() const;
    void SetOuter(UObject* InOuter);

    // Runtime type info
    virtual const char* GetObjClassName() const;

protected:

    uint64_t ObjectID = 0;

    std::string Name;

    UObject* Outer = nullptr;

private:

    static uint64_t GlobalObjectID;
};