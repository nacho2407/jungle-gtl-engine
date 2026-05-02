#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Core/ResourceManager.h"
#include "Engine/Runtime/Engine.h"
#include "Object/FName.h"
#include "UI/RuntimeUIFacade.h"

#include <cmath>

namespace
{
    FRuntimeUIFacade MakeRuntimeUIFacade()
    {
        return FRuntimeUIFacade(GEngine->GetRuntimeUI());
    }

    ERuntimeUIImageDrawMode ParseImageDrawMode(const FString& DrawMode)
    {
        if (DrawMode == "NineSlice" || DrawMode == "nine_slice" || DrawMode == "nine")
        {
            return ERuntimeUIImageDrawMode::NineSlice;
        }
        return ERuntimeUIImageDrawMode::Simple;
    }

    ERuntimeUIProgressFillDirection ParseProgressFillDirection(const FString& Direction)
    {
        if (Direction == "RightToLeft" || Direction == "right_to_left")
        {
            return ERuntimeUIProgressFillDirection::RightToLeft;
        }
        if (Direction == "BottomToTop" || Direction == "bottom_to_top")
        {
            return ERuntimeUIProgressFillDirection::BottomToTop;
        }
        if (Direction == "TopToBottom" || Direction == "top_to_bottom")
        {
            return ERuntimeUIProgressFillDirection::TopToBottom;
        }
        return ERuntimeUIProgressFillDirection::LeftToRight;
    }

    ERuntimeUIAnimationEasing ParseAnimationEasing(const FString& Easing)
    {
        if (Easing == "EaseIn" || Easing == "ease_in" || Easing == "easein")
        {
            return ERuntimeUIAnimationEasing::EaseIn;
        }
        if (Easing == "EaseOut" || Easing == "ease_out" || Easing == "easeout")
        {
            return ERuntimeUIAnimationEasing::EaseOut;
        }
        if (Easing == "EaseInOut" || Easing == "ease_in_out" || Easing == "easeinout")
        {
            return ERuntimeUIAnimationEasing::EaseInOut;
        }
        if (Easing == "SmoothStep" || Easing == "smooth_step" || Easing == "smoothstep")
        {
            return ERuntimeUIAnimationEasing::SmoothStep;
        }
        return ERuntimeUIAnimationEasing::Linear;
    }

    FRuntimeUIColor MakeUIColor(float R, float G, float B, float A)
    {
        return FRuntimeUIColor(R, G, B, A);
    }

    const FParticleResource* FindExactParticleResource(const FString& ParticleNameOrPath)
    {
        const TArray<FString> ParticleNames = FResourceManager::Get().GetParticleNames();
        for (const FString& ParticleName : ParticleNames)
        {
            if (ParticleName == ParticleNameOrPath)
            {
                return FResourceManager::Get().FindParticle(FName(ParticleNameOrPath.c_str()));
            }
        }
        return nullptr;
    }

    float MakeDurationFromSpeed(float X0, float Y0, float X1, float Y1, float Speed)
    {
        if (Speed <= 0.0f)
        {
            return 0.0f;
        }

        const float DX = X1 - X0;
        const float DY = Y1 - Y0;
        return std::sqrt(DX * DX + DY * DY) / Speed;
    }
}

namespace FLuaEngineAPI
{
    void BindUI(sol::state& Lua, sol::table& API)
    {
        sol::table UI = Lua.create_table();

        UI["ShowScreen"] = sol::overload(
            [](const FString& ScreenId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().ShowScreen(ScreenId) : false;
            },
            [](const FString& ScreenId, const FString& CanvasId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().ShowScreen(ScreenId, CanvasId) : false;
            });

        UI["CreatePanel"] = sol::overload(
            [](const FString& ScreenId, const FString& WidgetId, float X, float Y, float W, float H) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreatePanel(ScreenId, WidgetId, { X, Y }, { W, H }) : false;
            },
            [](const FString& ScreenId, const FString& WidgetId, float X, float Y, float W, float H, const FString& ParentId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreatePanel(ScreenId, WidgetId, { X, Y }, { W, H }, ParentId) : false;
            });

        UI["CreateText"] = sol::overload(
            [](const FString& ScreenId, const FString& WidgetId, const FString& Text, float X, float Y, float W, float H) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateText(ScreenId, WidgetId, Text, { X, Y }, { W, H }) : false;
            },
            [](const FString& ScreenId, const FString& WidgetId, const FString& Text, float X, float Y, float W, float H, const FString& ParentId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateText(ScreenId, WidgetId, Text, { X, Y }, { W, H }, ParentId) : false;
            });

        UI["CreateButton"] = sol::overload(
            [](const FString& ScreenId, const FString& WidgetId, const FString& Text, const FString& EventName, float X, float Y, float W, float H) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateButton(ScreenId, WidgetId, Text, EventName, { X, Y }, { W, H }) : false;
            },
            [](const FString& ScreenId, const FString& WidgetId, const FString& Text, const FString& EventName, float X, float Y, float W, float H, const FString& ParentId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateButton(ScreenId, WidgetId, Text, EventName, { X, Y }, { W, H }, ParentId) : false;
            });

        UI["CreateImage"] = sol::overload(
            [](const FString& ScreenId, const FString& WidgetId, const FString& ImagePath, float X, float Y, float W, float H) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateImage(ScreenId, WidgetId, ImagePath, { X, Y }, { W, H }) : false;
            },
            [](const FString& ScreenId, const FString& WidgetId, const FString& ImagePath, float X, float Y, float W, float H, const FString& ParentId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateImage(ScreenId, WidgetId, ImagePath, { X, Y }, { W, H }, ParentId) : false;
            });

        UI["CreateProgressBar"] = sol::overload(
            [](const FString& ScreenId, const FString& WidgetId, float Value, float X, float Y, float W, float H) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateProgressBar(ScreenId, WidgetId, Value, { X, Y }, { W, H }) : false;
            },
            [](const FString& ScreenId, const FString& WidgetId, float Value, float X, float Y, float W, float H, const FString& ParentId) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().CreateProgressBar(ScreenId, WidgetId, Value, { X, Y }, { W, H }, ParentId) : false;
            });

        UI["RemoveWidget"] = [](const FString& WidgetId) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().RemoveWidget(WidgetId) : false;
        };

        UI["SetText"] = [](const FString& WidgetId, const FString& Text) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetText(WidgetId, Text) : false;
        };

        UI["SetImage"] = [](const FString& WidgetId, const FString& ImagePath) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetImage(WidgetId, ImagePath) : false;
        };

        UI["SetProgress"] = [](const FString& WidgetId, float Value) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetProgress(WidgetId, Value) : false;
        };

        UI["SetVisible"] = [](const FString& WidgetId, bool bVisible) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetVisible(WidgetId, bVisible) : false;
        };

        UI["SetEnabled"] = [](const FString& WidgetId, bool bEnabled) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetEnabled(WidgetId, bEnabled) : false;
        };

        UI["SetActionEvent"] = [](const FString& WidgetId, const FString& EventName) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetActionEvent(WidgetId, EventName) : false;
        };

        UI["SetZOrder"] = [](const FString& WidgetId, int32 ZOrder) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetZOrder(WidgetId, ZOrder) : false;
        };

        UI["SetTint"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetTint(WidgetId, MakeUIColor(R, G, B, A)) : false;
        };

        UI["SetBackgroundColor"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetBackgroundColor(WidgetId, MakeUIColor(R, G, B, A)) : false;
        };

        UI["SetTextColor"] = [](const FString& WidgetId, float R, float G, float B, float A) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetTextColor(WidgetId, MakeUIColor(R, G, B, A)) : false;
        };

        UI["SetAlpha"] = [](const FString& WidgetId, float Alpha) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetAlpha(WidgetId, Alpha) : false;
        };

        UI["SetRounding"] = [](const FString& WidgetId, float Rounding) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetRounding(WidgetId, Rounding) : false;
        };

        UI["SetFontScale"] = [](const FString& WidgetId, float FontScale) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetFontScale(WidgetId, FontScale) : false;
        };

        UI["SetWidgetTransform"] = [](const FString& WidgetId, float X, float Y, float W, float H) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetWidgetTransform(WidgetId, { X, Y }, { W, H }) : false;
        };

        UI["SetWidgetAnchors"] = [](const FString& WidgetId, float MinX, float MinY, float MaxX, float MaxY, float PivotX, float PivotY) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetWidgetAnchors(WidgetId, { MinX, MinY }, { MaxX, MaxY }, { PivotX, PivotY }) : false;
        };

        UI["SetImageOptions"] = sol::overload(
            [](const FString& WidgetId, float UMinX, float UMinY, float UMaxX, float UMaxY, const FString& DrawMode) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().SetImageOptions(
                        WidgetId,
                        { { UMinX, UMinY }, { UMaxX, UMaxY } },
                        ParseImageDrawMode(DrawMode))
                    : false;
            },
            [](const FString& WidgetId, float UMinX, float UMinY, float UMaxX, float UMaxY, const FString& DrawMode, float Left, float Top, float Right, float Bottom) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().SetImageOptions(
                        WidgetId,
                        { { UMinX, UMinY }, { UMaxX, UMaxY } },
                        ParseImageDrawMode(DrawMode),
                        { Left, Top, Right, Bottom })
                    : false;
            });

        UI["SetButtonImages"] = [](const FString& WidgetId, const FString& Normal, const FString& Hover, const FString& Pressed, const FString& Disabled) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetButtonImages(WidgetId, Normal, Hover, Pressed, Disabled) : false;
        };

        UI["SetSpriteFrame"] = [](const FString& WidgetId, int32 Columns, int32 Rows, int32 FrameIndex) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().SetSpriteFrame(WidgetId, Columns, Rows, FrameIndex) : false;
        };

        UI["SetSpriteFrameByMeta"] = [](const FString& WidgetId, const FString& ParticleNameOrPath, int32 FrameIndex) -> bool
        {
            if (!GEngine)
            {
                return false;
            }

            const FParticleResource* Particle = FindExactParticleResource(ParticleNameOrPath);
            if (!Particle)
            {
                return false;
            }

            FRuntimeUIFacade Facade = MakeRuntimeUIFacade();
            return Facade.SetImage(WidgetId, Particle->Path)
                && Facade.SetSpriteFrame(
                    WidgetId,
                    static_cast<int32>(Particle->Columns),
                    static_cast<int32>(Particle->Rows),
                    FrameIndex);
        };

        UI["SetProgressImages"] = sol::overload(
            [](const FString& WidgetId, const FString& Background, const FString& Fill, const FString& Frame) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().SetProgressImages(WidgetId, Background, Fill, Frame) : false;
            },
            [](const FString& WidgetId, const FString& Background, const FString& Fill, const FString& Frame, const FString& Direction) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().SetProgressImages(WidgetId, Background, Fill, Frame, ParseProgressFillDirection(Direction)) : false;
            });

        UI["AnimateTransform"] = sol::overload(
            [](const FString& WidgetId, float X, float Y, float W, float H, float Duration) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetTransform(WidgetId, { X, Y }, { W, H }, Duration) : false;
            },
            [](const FString& WidgetId, float X, float Y, float W, float H, float Duration, const FString& Easing) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetTransform(WidgetId, { X, Y }, { W, H }, Duration, false, false, ParseAnimationEasing(Easing))
                    : false;
            },
            [](const FString& WidgetId, float X, float Y, float W, float H, float Duration, bool bLoop, bool bPingPong) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetTransform(WidgetId, { X, Y }, { W, H }, Duration, bLoop, bPingPong) : false;
            },
            [](const FString& WidgetId, float X, float Y, float W, float H, float Duration, bool bLoop, bool bPingPong, const FString& Easing) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetTransform(WidgetId, { X, Y }, { W, H }, Duration, bLoop, bPingPong, ParseAnimationEasing(Easing))
                    : false;
            });

        UI["AnimatePatrol"] = sol::overload(
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Duration) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration) : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Duration, const FString& Easing) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, true, true, ParseAnimationEasing(Easing))
                    : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Duration, bool bLoop, bool bPingPong) -> bool
            {
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, bLoop, bPingPong) : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Duration, bool bLoop, bool bPingPong, const FString& Easing) -> bool
            {
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, bLoop, bPingPong, ParseAnimationEasing(Easing))
                    : false;
            });

        UI["AnimatePatrolBySpeed"] = sol::overload(
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Speed) -> bool
            {
                const float Duration = MakeDurationFromSpeed(X0, Y0, X1, Y1, Speed);
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration) : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Speed, const FString& Easing) -> bool
            {
                const float Duration = MakeDurationFromSpeed(X0, Y0, X1, Y1, Speed);
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, true, true, ParseAnimationEasing(Easing))
                    : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Speed, bool bLoop, bool bPingPong) -> bool
            {
                const float Duration = MakeDurationFromSpeed(X0, Y0, X1, Y1, Speed);
                return GEngine ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, bLoop, bPingPong) : false;
            },
            [](const FString& WidgetId, float X0, float Y0, float X1, float Y1, float Speed, bool bLoop, bool bPingPong, const FString& Easing) -> bool
            {
                const float Duration = MakeDurationFromSpeed(X0, Y0, X1, Y1, Speed);
                return GEngine
                    ? MakeRuntimeUIFacade().AnimateWidgetPatrol(WidgetId, { X0, Y0 }, { X1, Y1 }, Duration, bLoop, bPingPong, ParseAnimationEasing(Easing))
                    : false;
            });

        UI["StopAnimation"] = [](const FString& WidgetId) -> bool
        {
            return GEngine ? MakeRuntimeUIFacade().StopWidgetAnimation(WidgetId) : false;
        };

        UI["PollActionEvents"] = [](sol::this_state State)
        {
            sol::state_view Lua(State);
            sol::table Events = Lua.create_table();
            if (!GEngine)
            {
                return Events;
            }

            const TArray<FString> PendingEvents = MakeRuntimeUIFacade().PollActionEvents();
            int32 Index = 1;
            for (const FString& EventName : PendingEvents)
            {
                Events[Index++] = EventName;
            }
            return Events;
        };

        API["UI"] = UI;
    }
}
