#pragma once

namespace PickId
{
    static constexpr uint32 None = 0u;
    static constexpr uint32 GizmoMask = 0x80000000u;

    enum class EGizmoPart : uint32
    {
        None = 0,

        TranslateX = 1,
        TranslateY = 2,
        TranslateZ = 3,

        RotateX = 4,
        RotateY = 5,
        RotateZ = 6,

        ScaleX = 7,
        ScaleY = 8,
        ScaleZ = 9,

        TranslateCenter = 10,
        RotateCenter = 11,
        ScaleCenter = 12,
    };

    inline uint32 MakeGizmoPartId(EGizmoType InGizmoType, EAxis InAxis)
    {
        uint32 Part = 0;

        switch (InGizmoType)
        {
        case EGizmoType::Translation:
            Part = (InAxis == EAxis::X)   ? static_cast<uint32>(EGizmoPart::TranslateX)
                   : (InAxis == EAxis::Y) ? static_cast<uint32>(EGizmoPart::TranslateY)
                                          : static_cast<uint32>(EGizmoPart::TranslateZ);
            break;

        case EGizmoType::Rotation:
            Part = (InAxis == EAxis::X)   ? static_cast<uint32>(EGizmoPart::RotateX)
                   : (InAxis == EAxis::Y) ? static_cast<uint32>(EGizmoPart::RotateY)
                                          : static_cast<uint32>(EGizmoPart::RotateZ);
            break;

        case EGizmoType::Scaling:
            Part = (InAxis == EAxis::X)   ? static_cast<uint32>(EGizmoPart::ScaleX)
                   : (InAxis == EAxis::Y) ? static_cast<uint32>(EGizmoPart::ScaleY)
                                          : static_cast<uint32>(EGizmoPart::ScaleZ);
            break;

        default:
            Part = 0;
            break;
        }

        return (Part == 0) ? None : (GizmoMask | Part);
    }

    inline uint32 MakeGizmoCenterId(EGizmoType InGizmoType)
    {
        uint32 Part = 0;

        switch (InGizmoType)
        {
        case EGizmoType::Translation:
            Part = static_cast<uint32>(EGizmoPart::TranslateCenter);
            break;

        case EGizmoType::Rotation:
            Part = static_cast<uint32>(EGizmoPart::RotateCenter);
            break;

        case EGizmoType::Scaling:
            Part = static_cast<uint32>(EGizmoPart::ScaleCenter);
            break;

        default:
            Part = 0;
            break;
        }

        return (Part == 0) ? None : (GizmoMask | Part);
    }

    inline bool IsGizmoId(uint32 InPickId) { return (InPickId & GizmoMask) != 0; }

    inline bool DecodeGizmoPart(uint32 InPickId, EGizmoType& OutGizmoType, EAxis& OutAxis)
    {
        if (!IsGizmoId(InPickId))
        {
            OutGizmoType = EGizmoType::None;
            OutAxis = EAxis::X;
            return false;
        }

        const uint32 Part = (InPickId & ~GizmoMask);

        switch (Part)
        {
        case static_cast<uint32>(EGizmoPart::TranslateX):
            OutGizmoType = EGizmoType::Translation;
            OutAxis = EAxis::X;
            return true;

        case static_cast<uint32>(EGizmoPart::TranslateY):
            OutGizmoType = EGizmoType::Translation;
            OutAxis = EAxis::Y;
            return true;

        case static_cast<uint32>(EGizmoPart::TranslateZ):
            OutGizmoType = EGizmoType::Translation;
            OutAxis = EAxis::Z;
            return true;

        case static_cast<uint32>(EGizmoPart::RotateX):
            OutGizmoType = EGizmoType::Rotation;
            OutAxis = EAxis::X;
            return true;

        case static_cast<uint32>(EGizmoPart::RotateY):
            OutGizmoType = EGizmoType::Rotation;
            OutAxis = EAxis::Y;
            return true;

        case static_cast<uint32>(EGizmoPart::RotateZ):
            OutGizmoType = EGizmoType::Rotation;
            OutAxis = EAxis::Z;
            return true;

        case static_cast<uint32>(EGizmoPart::ScaleX):
            OutGizmoType = EGizmoType::Scaling;
            OutAxis = EAxis::X;
            return true;

        case static_cast<uint32>(EGizmoPart::ScaleY):
            OutGizmoType = EGizmoType::Scaling;
            OutAxis = EAxis::Y;
            return true;

        case static_cast<uint32>(EGizmoPart::ScaleZ):
            OutGizmoType = EGizmoType::Scaling;
            OutAxis = EAxis::Z;
            return true;

        case static_cast<uint32>(EGizmoPart::TranslateCenter):
            OutGizmoType = EGizmoType::Translation;
            OutAxis = EAxis::X;
            return true;

        case static_cast<uint32>(EGizmoPart::RotateCenter):
            OutGizmoType = EGizmoType::Rotation;
            OutAxis = EAxis::X;
            return true;

        case static_cast<uint32>(EGizmoPart::ScaleCenter):
            OutGizmoType = EGizmoType::Scaling;
            OutAxis = EAxis::X;
            return true;

        default:
            OutGizmoType = EGizmoType::None;
            OutAxis = EAxis::X;
            return false;
        }
    }
} // namespace PickId