#include "AnimationTimelinePanel.h"
#include "AnimationTransportBar.h"

#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
#include "Animation/Sequence/AnimDataModel.h"
#include "Animation/Notify/AnimNotify.h"
#include "Animation/Notify/AnimNotifyState.h"
#include "Animation/AnimationManager.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Core/PropertyTypes.h"
#include "Editor/UI/Asset/MorphCurveEditObject.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

// 일부 헤더가 Windows.h 를 끌어오면 GetCurrentTime 매크로가 호출을 가로챈다.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace
{
	constexpr float HeaderW     = 190.0f; // 좌측 트랙 헤더 컬럼 폭
	constexpr float RulerH      = 22.0f;  // 프레임 눈금 행 높이
	constexpr float RowH        = 22.0f;  // 트랙 헤더 행 높이
	constexpr float NotifyLaneH = 28.0f;
	constexpr float MorphLaneH  = 28.0f; // Notifies 펼침 시 레인 높이
	constexpr float TransportH  = 36.0f; // 하단 트랜스포트 행 높이

	// Morph weight 편집 범위. 그래프 범위를 key 값으로 매 프레임 재계산하면
	// 마우스가 그래프 밖으로 나간 상태에서 값/스케일이 계속 확장되어 value가 폭주한다.
	constexpr float MorphValueMin = -1.0f;
	constexpr float MorphValueMax =  1.0f;

	constexpr ImU32 ColPanelBg   = IM_COL32(26, 26, 26, 255);
	constexpr ImU32 ColHeaderBg  = IM_COL32(38, 38, 38, 255);
	constexpr ImU32 ColRulerBg   = IM_COL32(33, 33, 33, 255);
	constexpr ImU32 ColSeparator = IM_COL32(18, 18, 18, 255);
	constexpr ImU32 ColTick      = IM_COL32(78, 78, 78, 255);
	constexpr ImU32 ColTickMinor = IM_COL32(52, 52, 52, 255);
	constexpr ImU32 ColLabel     = IM_COL32(150, 150, 150, 255);
	constexpr ImU32 ColRowText   = IM_COL32(205, 205, 205, 255);
	constexpr ImU32 ColPlayhead  = IM_COL32(255, 170, 40, 255);
	constexpr ImU32 ColNotify    = IM_COL32(74, 145, 226, 255);
	constexpr ImU32 ColNotifyDur = IM_COL32(74, 145, 226, 110);

	// 등록된 모든 UClass 중 Base 의 구체 서브클래스만 수집 (Base 자체는 제외).
	// ObjectFactory 가 UCLASS 등록 시 자동으로 클래스명 → 인스턴스 함수를 등록하므로
	// 새 Notify/NotifyState 클래스를 추가하면 별도 작업 없이 자동으로 콤보에 노출된다.
	TArray<UClass*> EnumerateConcreteSubclasses(UClass* Base)
	{
		TArray<UClass*> Out;
		if (!Base) return Out;
		for (UClass* C : UClass::GetAllClasses())
		{
			if (!C || C == Base) continue;
			if (!C->IsA(Base)) continue;
			Out.push_back(C);
		}
		return Out;
	}

	// Notify/NotifyState 인스턴스 생성 + FAnimNotifyEvent 채워 반환.
	// bAsState=true → NotifyState 슬롯, false → Notify 슬롯. ObjectFactory::Create 가 클래스
	// 이름으로 인스턴스 만들고 DataModel 을 Outer 로 매단다 (라이프타임 체인).
	FAnimNotifyEvent MakeNotifyFromClass(UAnimSequence* Seq, UClass* Cls,
	                                     const FString& Name, float Time,
	                                     float Duration, bool bAsState)
	{
		FAnimNotifyEvent Event;
		Event.NotifyName  = FName(Name);
		Event.TriggerTime = Time;
		Event.Duration    = bAsState ? std::max(Duration, 0.01f) : 0.0f;

		if (Cls && Seq)
		{
			UObject* Created = FObjectFactory::Get().Create(Cls->GetName(), Seq->GetDataModel());
			if (bAsState)
			{
				Event.NotifyState = Cast<UAnimNotifyState>(Created);
			}
			else
			{
				Event.Notify = Cast<UAnimNotify>(Created);
			}
		}
		return Event;
	}

	// 가용 폭에 안 맞으면 끝에 "..." 을 붙여 잘라낸다. CalcTextSize 가 픽셀 단위 폭을 알려주므로
	// 끝부터 한 글자씩 줄여가며 ellipsis 와 합한 폭이 들어맞을 때까지 반복. 일반 notify 이름은
	// 짧으므로 linear truncation 비용 무시 가능.
	std::string TruncateWithEllipsis(const std::string& In, float MaxW)
	{
		if (MaxW <= 0.0f || In.empty()) return "";
		const ImVec2 Full = ImGui::CalcTextSize(In.c_str());
		if (Full.x <= MaxW) return In;

		static const std::string Ellipsis = "...";
		const float EllipsisW = ImGui::CalcTextSize(Ellipsis.c_str()).x;
		if (MaxW <= EllipsisW) return Ellipsis;

		std::string Buf = In;
		while (Buf.size() > 1)
		{
			Buf.pop_back();
			if (ImGui::CalcTextSize((Buf + Ellipsis).c_str()).x <= MaxW)
			{
				return Buf + Ellipsis;
			}
		}
		return Ellipsis;
	}

	// 한 UObject 의 UPROPERTY(Edit) 필드를 인플레이스로 그려준다. Notify/NotifyState 의
	// payload 편집용 경량 인스펙터 — 풀 FEditorPropertyWidget 의존성 없이 timeline 패널 안에서
	// 자족. 지원 타입은 Notify payload 에 흔히 쓰일 단순형 (Bool/Int/Float/String/Vec3/Vec4/Color4).
	// 그 외 타입은 disabled placeholder.
	bool RenderObjectPropertiesInline(UObject* Object)
	{
		if (!Object)
		{
			ImGui::TextDisabled("(no object)");
			return false;
		}
		TArray<FPropertyValue> Props;
		Object->GetEditableProperties(Props);
		if (Props.empty())
		{
			ImGui::TextDisabled("(no editable properties)");
			return false;
		}

		bool bAnyChanged = false;

		if (ImGui::BeginTable("##notifyProps", 2,
		                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

			for (FPropertyValue& Prop : Props)
			{
				const bool bReadOnly = Prop.Property && (Prop.Property->Flags & PF_ReadOnly) != 0;
				const char* Disp = Prop.GetDisplayName();
				if (!Disp || !*Disp) Disp = Prop.GetName();

				ImGui::PushID(Prop.GetName() ? Prop.GetName() : "");
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(Disp ? Disp : "");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);

				if (bReadOnly) ImGui::BeginDisabled();

				bool bChanged = false;
				switch (Prop.GetType())
				{
				case EPropertyType::Bool:
				{
					bool* V = static_cast<bool*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::Checkbox("##v", V);
					break;
				}
				case EPropertyType::ByteBool:
				{
					uint8* V = static_cast<uint8*>(Prop.GetValuePtr());
					if (V)
					{
						bool b = (*V != 0);
						if (ImGui::Checkbox("##v", &b)) { *V = b ? 1 : 0; bChanged = true; }
					}
					break;
				}
				case EPropertyType::Int:
				{
					int32* V = static_cast<int32*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragInt("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Float:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Vec3:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat3("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Vec4:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::DragFloat4("##v", V, Prop.GetSpeed());
					break;
				}
				case EPropertyType::Color4:
				{
					float* V = static_cast<float*>(Prop.GetValuePtr());
					if (V) bChanged = ImGui::ColorEdit4("##v", V);
					break;
				}
				case EPropertyType::String:
				{
					FString* S = static_cast<FString*>(Prop.GetValuePtr());
					if (S)
					{
						char Buf[256];
						strncpy_s(Buf, sizeof(Buf), S->c_str(), _TRUNCATE);
						if (ImGui::InputText("##v", Buf, sizeof(Buf)))
						{
							*S = Buf;
							bChanged = true;
						}
					}
					break;
				}
				case EPropertyType::Name:
				{
					FName* N = static_cast<FName*>(Prop.GetValuePtr());
					if (N)
					{
						FString Cur = N->ToString();
						char Buf[256];
						strncpy_s(Buf, sizeof(Buf), Cur.c_str(), _TRUNCATE);
						if (ImGui::InputText("##v", Buf, sizeof(Buf)))
						{
							*N = FName(FString(Buf));
							bChanged = true;
						}
					}
					break;
				}
				case EPropertyType::Enum:
				{
					const FEnum* EnumType = Prop.GetEnumType();
					if (EnumType && EnumType->GetNames() && EnumType->GetCount() > 0 && Prop.GetValuePtr())
					{
						const char** EnumNames = EnumType->GetNames();
						const uint32 EnumCount = EnumType->GetCount();
						const uint32 EnumSize = EnumType->GetSize();
						int32 Val = 0;
						std::memcpy(&Val, Prop.GetValuePtr(), EnumSize);
						const char* Preview = ((uint32)Val < EnumCount) ? EnumNames[Val] : "Unknown";
						if (ImGui::BeginCombo("##v", Preview))
						{
							for (uint32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
							{
								const bool bSelected = Val == static_cast<int32>(EnumIndex);
								if (ImGui::Selectable(EnumNames[EnumIndex], bSelected))
								{
									int32 NewVal = static_cast<int32>(EnumIndex);
									std::memcpy(Prop.GetValuePtr(), &NewVal, EnumSize);
									bChanged = true;
								}
								if (bSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
					}
					break;
				}
				default:
					ImGui::TextDisabled("(unsupported type)");
					break;
				}

				if (bReadOnly) ImGui::EndDisabled();

				if (bChanged)
				{
					bAnyChanged = true;
					if (Prop.Property)
					{
						Object->PostEditProperty(Prop.Property->Name);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		return bAnyChanged;
	}

	static float ClampMorphCurveValue(float Value)
	{
		return std::clamp(Value, MorphValueMin, MorphValueMax);
	}

	static FMorphTargetCurve& FindOrAddMorphCurve(UAnimSequence* Seq, const FString& MorphTargetName)
	{
		TArray<FMorphTargetCurve>& Curves = Seq->GetMutableMorphTargetCurves();
		for (FMorphTargetCurve& Curve : Curves)
		{
			if (Curve.MorphTargetName == MorphTargetName)
			{
				return Curve;
			}
		}
		FMorphTargetCurve NewCurve;
		NewCurve.MorphTargetName = MorphTargetName;
		Curves.push_back(std::move(NewCurve));
		return Curves.back();
	}

	static void AddOrUpdateMorphCurveKey(FMorphTargetCurve& Curve, float TimeSeconds, float Value)
	{
		Value = ClampMorphCurveValue(Value);
		constexpr float TimeTolerance = 1.0e-4f;
		for (FRawFloatCurveKey& Key : Curve.Curve.Keys)
		{
			if (std::fabs(Key.TimeSeconds - TimeSeconds) <= TimeTolerance)
			{
				Key.Value = Value;
				return;
			}
		}
		FRawFloatCurveKey NewKey;
		NewKey.TimeSeconds   = TimeSeconds;
		NewKey.Value         = Value;
		NewKey.Interpolation = 2;
		Curve.Curve.Keys.push_back(NewKey);
		std::sort(
			Curve.Curve.Keys.begin(),
			Curve.Curve.Keys.end(),
			[](const FRawFloatCurveKey& A, const FRawFloatCurveKey& B)
			{
				return A.TimeSeconds < B.TimeSeconds;
			}
		);
	}

	static const char* InterpLabel(int32 Interp)
	{
		if ((Interp & 4) == 4) return "Bezier";
		if ((Interp & 1) == 1) return "Constant";
		return "Linear";
	}

	static const char* TangentModeLabel(int32 Mode)
	{
		switch (Mode)
		{
		case 1:
			return "Aligned";
		case 2:
			return "Free";
		case 0: default:
			return "Auto";
		}
	}

	static float EstimateTimelineCurveTangent(const FRawFloatCurve& Curve, int32 KeyIndex)
	{
		const int32 NumKeys = static_cast<int32>(Curve.Keys.size());
		if (NumKeys <= 1 || KeyIndex < 0 || KeyIndex >= NumKeys)
		{
			return 0.0f;
		}

		const int32 PrevIndex = std::max(0, KeyIndex - 1);
		const int32 NextIndex = std::min(NumKeys - 1, KeyIndex + 1);
		const FRawFloatCurveKey& Prev = Curve.Keys[PrevIndex];
		const FRawFloatCurveKey& Next = Curve.Keys[NextIndex];
		const float DeltaTime = Next.TimeSeconds - Prev.TimeSeconds;
		return std::fabs(DeltaTime) > 1.0e-6f ? (Next.Value - Prev.Value) / DeltaTime : 0.0f;
	}

	static float TimelineCubicBezier(float A, float B, float C, float D, float T)
	{
		const float U = 1.0f - T;
		return U * U * U * A + 3.0f * U * U * T * B + 3.0f * U * T * T * C + T * T * T * D;
	}

	static float TimelineCubicBezierDerivative(float A, float B, float C, float D, float T)
	{
		const float U = 1.0f - T;
		return 3.0f * U * U * (B - A) + 6.0f * U * T * (C - B) + 3.0f * T * T * (D - C);
	}

	static float SolveTimelineBezierTime(float TargetTime, float X0, float X1, float X2, float X3)
	{
		float T = 0.5f;
		const float Denom = X3 - X0;
		if (std::fabs(Denom) > 1.0e-6f)
		{
			T = std::clamp((TargetTime - X0) / Denom, 0.0f, 1.0f);
		}

		for (int32 Iter = 0; Iter < 8; ++Iter)
		{
			const float X = TimelineCubicBezier(X0, X1, X2, X3, T);
			const float DX = TimelineCubicBezierDerivative(X0, X1, X2, X3, T);
			if (std::fabs(DX) < 1.0e-6f)
			{
				break;
			}
			T = std::clamp(T - (X - TargetTime) / DX, 0.0f, 1.0f);
		}
		return T;
	}

	static float EvaluateTimelineMorphCurveSegment(const FRawFloatCurve& Curve, int32 KeyIndex, float TimeSeconds)
	{
		const FRawFloatCurveKey& A = Curve.Keys[KeyIndex];
		const FRawFloatCurveKey& B = Curve.Keys[KeyIndex + 1];
		const float DeltaTime = B.TimeSeconds - A.TimeSeconds;
		if (std::fabs(DeltaTime) < 1.0e-6f)
		{
			return B.Value;
		}

		if ((A.Interpolation & 1) == 1)
		{
			return A.Value;
		}

		if ((A.Interpolation & 4) != 4)
		{
			const float Alpha = std::clamp((TimeSeconds - A.TimeSeconds) / DeltaTime, 0.0f, 1.0f);
			return A.Value + (B.Value - A.Value) * Alpha;
		}

		float LeaveWeight = A.bLeaveTangentWeighted ? A.LeaveTangentWeight : DeltaTime / 3.0f;
		float ArriveWeight = B.bArriveTangentWeighted ? B.ArriveTangentWeight : DeltaTime / 3.0f;
		LeaveWeight = std::clamp(LeaveWeight, 1.0e-5f, DeltaTime);
		ArriveWeight = std::clamp(ArriveWeight, 1.0e-5f, DeltaTime);

		const float LeaveTangent = A.TangentMode == 0 ? EstimateTimelineCurveTangent(Curve, KeyIndex) : A.LeaveTangent;
		const float ArriveTangent = B.TangentMode == 0 ? EstimateTimelineCurveTangent(Curve, KeyIndex + 1) : B.ArriveTangent;

		const float X0 = A.TimeSeconds;
		const float Y0 = A.Value;
		const float X1 = A.TimeSeconds + LeaveWeight;
		const float Y1 = A.Value + LeaveTangent * LeaveWeight;
		const float X2 = B.TimeSeconds - ArriveWeight;
		const float Y2 = B.Value - ArriveTangent * ArriveWeight;
		const float X3 = B.TimeSeconds;
		const float Y3 = B.Value;

		const float BezierT = SolveTimelineBezierTime(TimeSeconds, X0, X1, X2, X3);
		return TimelineCubicBezier(Y0, Y1, Y2, Y3, BezierT);
	}

	static float EvaluateTimelineMorphCurve(const FRawFloatCurve& Curve, float TimeSeconds, float DefaultValue)
	{
		const int32 NumKeys = static_cast<int32>(Curve.Keys.size());
		if (NumKeys <= 0)
		{
			return DefaultValue;
		}
		if (NumKeys == 1 || TimeSeconds <= Curve.Keys.front().TimeSeconds)
		{
			return Curve.Keys.front().Value;
		}
		if (TimeSeconds >= Curve.Keys.back().TimeSeconds)
		{
			return Curve.Keys.back().Value;
		}
		for (int32 KeyIndex = 0; KeyIndex + 1 < NumKeys; ++KeyIndex)
		{
			if (TimeSeconds >= Curve.Keys[KeyIndex].TimeSeconds && TimeSeconds <= Curve.Keys[KeyIndex + 1].TimeSeconds)
			{
				return EvaluateTimelineMorphCurveSegment(Curve, KeyIndex, TimeSeconds);
			}
		}
		return Curve.Keys.back().Value;
	}

	static void GetMorphCurveValueRange(const FRawFloatCurve& Curve, float& OutMin, float& OutMax)
	{
		(void)Curve;
		// 고정 편집 범위. key 값 기반 자동 범위는 드래그 중 스케일이 바뀌어 value 폭주를 만든다.
		OutMin = MorphValueMin;
		OutMax = MorphValueMax;
	}

	static float MorphValueToY(float Value, float GraphTop, float GraphH, float MinValue, float MaxValue)
	{
		const float Alpha = std::clamp((Value - MinValue) / std::max(MaxValue - MinValue, 1.0e-6f), 0.0f, 1.0f);
		return GraphTop + (1.0f - Alpha) * GraphH;
	}

	static float MorphYToValue(float Y, float GraphTop, float GraphH, float MinValue, float MaxValue)
	{
		const float Alpha = 1.0f - std::clamp((Y - GraphTop) / std::max(GraphH, 1.0f), 0.0f, 1.0f);
		return MinValue + Alpha * (MaxValue - MinValue);
	}

	static float GetLeaveHandleTime(const FRawFloatCurveKey& Key, const FRawFloatCurveKey& Next)
	{
		const float Segment = std::max(Next.TimeSeconds - Key.TimeSeconds, 1.0e-5f);
		const float Weight = Key.bLeaveTangentWeighted ? Key.LeaveTangentWeight : Segment / 3.0f;
		return Key.TimeSeconds + std::clamp(Weight, 1.0e-5f, Segment);
	}

	static float GetArriveHandleTime(const FRawFloatCurveKey& Prev, const FRawFloatCurveKey& Key)
	{
		const float Segment = std::max(Key.TimeSeconds - Prev.TimeSeconds, 1.0e-5f);
		const float Weight = Key.bArriveTangentWeighted ? Key.ArriveTangentWeight : Segment / 3.0f;
		return Key.TimeSeconds - std::clamp(Weight, 1.0e-5f, Segment);
	}

	static void SetLeaveHandleFromPoint(FRawFloatCurveKey& Key, const FRawFloatCurveKey& Next, float HandleTime, float HandleValue)
	{
		HandleValue = ClampMorphCurveValue(HandleValue);
		if (Key.TangentMode == 0)
		{
			Key.TangentMode = 2;
		}
		const float Segment = std::max(Next.TimeSeconds - Key.TimeSeconds, 1.0e-5f);
		const float Weight = std::clamp(HandleTime - Key.TimeSeconds, 1.0e-5f, Segment);
		Key.LeaveTangentWeight = Weight;
		Key.bLeaveTangentWeighted = true;
		Key.LeaveTangent = (HandleValue - Key.Value) / Weight;
		if (Key.TangentMode == 1)
		{
			Key.ArriveTangent = Key.LeaveTangent;
			Key.ArriveTangentWeight = Key.LeaveTangentWeight;
			Key.bArriveTangentWeighted = Key.bLeaveTangentWeighted;
		}
	}

	static void SetArriveHandleFromPoint(const FRawFloatCurveKey& Prev, FRawFloatCurveKey& Key, float HandleTime, float HandleValue)
	{
		HandleValue = ClampMorphCurveValue(HandleValue);
		if (Key.TangentMode == 0)
		{
			Key.TangentMode = 2;
		}
		const float Segment = std::max(Key.TimeSeconds - Prev.TimeSeconds, 1.0e-5f);
		const float Weight = std::clamp(Key.TimeSeconds - HandleTime, 1.0e-5f, Segment);
		Key.ArriveTangentWeight = Weight;
		Key.bArriveTangentWeighted = true;
		Key.ArriveTangent = (Key.Value - HandleValue) / Weight;
		if (Key.TangentMode == 1)
		{
			Key.LeaveTangent = Key.ArriveTangent;
			Key.LeaveTangentWeight = Key.ArriveTangentWeight;
			Key.bLeaveTangentWeighted = Key.bArriveTangentWeighted;
		}
	}


	int NiceFrameStep(int Raw)
	{
		static const int Steps[] = { 1, 2, 5, 10, 15, 20, 30, 60, 120, 240, 600 };
		for (int S : Steps)
		{
			if (S >= Raw) return S;
		}
		return 1200;
	}

	// 좌측 트랙 헤더 행 한 줄을 그린다. bExpandable 이면 삼각형 토글을 그린다.
	// 클릭 판정은 호출부에서 InvisibleButton 으로 처리.
	void DrawTrackHeaderRow(ImDrawList* DL, const ImVec2& Pos, float Width, float Height,
	                        const char* Label, bool bExpandable, bool bExpanded)
	{
		DL->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), ColHeaderBg);
		DL->AddLine(ImVec2(Pos.x, Pos.y + Height - 1.0f),
		            ImVec2(Pos.x + Width, Pos.y + Height - 1.0f), ColSeparator);

		const float CY = Pos.y + Height * 0.5f;
		float TextX = Pos.x + 10.0f;

		if (bExpandable)
		{
			const float Cx = Pos.x + 12.0f;
			if (bExpanded)
			{
				DL->AddTriangleFilled(ImVec2(Cx - 4.0f, CY - 3.0f),
				                      ImVec2(Cx + 4.0f, CY - 3.0f),
				                      ImVec2(Cx, CY + 4.0f), ColRowText);
			}
			else
			{
				DL->AddTriangleFilled(ImVec2(Cx - 3.0f, CY - 4.0f),
				                      ImVec2(Cx - 3.0f, CY + 4.0f),
				                      ImVec2(Cx + 4.0f, CY), ColRowText);
			}
			TextX = Pos.x + 26.0f;
		}

		const ImVec2 TS = ImGui::CalcTextSize(Label);
		DL->AddText(ImVec2(TextX, CY - TS.y * 0.5f), ColRowText, Label);
	}
}

void FAnimationTimelinePanel::Render(UAnimSingleNodeInstance* NodeInst,
	USkeletalMeshComponent*                                   Comp,
	UAnimSequence*                                            Seq,
	float                                                     PanelHeight,
	int32&                                                    InOutSelectedNotifyIndex,
	int32&                                                    InOutSelectedMorphCurveIndex,
	int32&                                                    InOutSelectedMorphKeyIndex
	)
{
	// 변경 누적 플래그 — drag/resize 등 연속 이벤트는 매 프레임 commit 하지 않고
	// 마우스 release 시점에 일괄 save (디스크 thrash 방지). 인스턴트 이벤트는 즉시 save.
	// 프레임 간 보존 위해 static — 마우스 누르고 있는 동안 dirty 유지, release 에 일괄 flush.
	static bool sPendingSave = false;
	auto SaveSeqNow = [&]() {
		if (Seq) FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
	};

	// 선택 인덱스 stale clamp — 시퀀스 전환 등으로 out-of-range 면 -1.
	if (Seq)
	{
		const int32 NotifyCount = static_cast<int32>(Seq->GetNotifies().size());
		if (InOutSelectedNotifyIndex >= NotifyCount) InOutSelectedNotifyIndex = -1;
		const int32 MorphCurveCount = static_cast<int32>(Seq->GetMorphTargetCurves().size());
		if (InOutSelectedMorphCurveIndex >= MorphCurveCount)
		{
			InOutSelectedMorphCurveIndex = -1;
			InOutSelectedMorphKeyIndex   = -1;
		}
	}

	ImGui::BeginChild("##AnimTimelinePanel", ImVec2(0.0f, PanelHeight), false,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const float TrackViewportH = std::max(PanelHeight - TransportH, RulerH + RowH);
	ImGui::BeginChild("##AnimTimelineTrackScroll", ImVec2(0.0f, TrackViewportH), false,
	                  ImGuiWindowFlags_HorizontalScrollbar);

	ImDrawList*  DL     = ImGui::GetWindowDrawList();
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const float  FullW  = ImGui::GetContentRegionAvail().x;

	if (!Seq || Seq->GetPlayLength() <= 0.0f)
	{
		DL->AddRectFilled(Origin, ImVec2(Origin.x + FullW, Origin.y + PanelHeight), ColPanelBg);
		const char* Msg = "No animation selected.";
		const ImVec2 TS = ImGui::CalcTextSize(Msg);
		DL->AddText(ImVec2(Origin.x + (FullW - TS.x) * 0.5f, Origin.y + PanelHeight * 0.5f - TS.y),
		            ColLabel, Msg);
		ImGui::EndChild();
		ImGui::EndChild();
		return;
	}

	static bool bNotifiesExpanded    = true;
	static bool bMorphCurvesExpanded = true;

	const float PlayLength = Seq->GetPlayLength();
	const float FrameRate  = Seq->GetFrameRate() > 0.0f ? Seq->GetFrameRate() : 30.0f;
	const int   NumFrames  = std::max(Seq->GetNumberOfFrames(), 1);
	const int   EndFrame   = std::max(NumFrames - 1, 0);

	float TrackAreaH = std::max(TrackViewportH, RulerH + RowH);
	const float CanvasX    = Origin.x + HeaderW;
	const float CanvasW    = std::max(FullW - HeaderW, 1.0f);

	auto TimeToX = [&](float T) { return CanvasX + (T / PlayLength) * CanvasW; };

	const float CurrentTime  = NodeInst ? NodeInst->GetCurrentTime() : 0.0f;
	const int   CurrentFrame = static_cast<int>(std::lround((CurrentTime / PlayLength) * EndFrame));

	// ── 배경 ──
	DL->AddRectFilled(Origin, ImVec2(Origin.x + FullW, Origin.y + PanelHeight), ColPanelBg);
	DL->AddRectFilled(ImVec2(CanvasX, Origin.y),
	                  ImVec2(CanvasX + CanvasW, Origin.y + RulerH), ColRulerBg);

	// ── 스크럽 입력 (룰러 + 트랙 영역 전체) ──
	ImGui::SetCursorScreenPos(ImVec2(CanvasX, Origin.y));
	// 노티파이 핸들 등 위에 겹쳐 놓는 아이템이 입력을 먼저 가져갈 수 있도록 허용.
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton("##scrub", ImVec2(CanvasW, TrackAreaH));
	if ((ImGui::IsItemActive() || ImGui::IsItemHovered()) && ImGui::IsMouseDown(0) && NodeInst)
	{
		const float Frac = std::clamp((ImGui::GetIO().MousePos.x - CanvasX) / CanvasW, 0.0f, 1.0f);
		NodeInst->SetCurrentTime(Frac * PlayLength);
		if (Comp && ImGui::IsItemActive())
		{
			Comp->SetPlaying(false);
		}
	}
	// 빈 영역 클릭 → notify 선택 해제. 노티 배지는 더 늦게 그려져 입력을 먼저 가져가므로
	// scrub.IsItemActivated 가 트리거되었다는 것은 배지 hit 가 아니라는 뜻.
	if (ImGui::IsItemActivated())
	{
		InOutSelectedNotifyIndex     = -1;
		InOutSelectedMorphCurveIndex = -1;
		InOutSelectedMorphKeyIndex   = -1;
	}

	// ── 노티파이 레인 우클릭 → "Add Notify" 팝업 ──
	// 클릭 지점 시간에 노티파이(+LogMessage 로직)를 추가 → DataModel 에 기록되어
	// 직렬화되고, RefreshRuntimeNotifies 로 dispatch 캐시에 반영돼 프리뷰에서 실제 발사.
	static float sPendingNotifyTime = 0.0f;
	if (bNotifiesExpanded && ImGui::IsItemHovered() &&
	    ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		const float LaneTop = Origin.y + RulerH + RowH;
		const float LaneBot = LaneTop + NotifyLaneH;
		const float MouseY  = ImGui::GetIO().MousePos.y;
		if (MouseY >= LaneTop && MouseY <= LaneBot)
		{
			const float Frac = std::clamp(
				(ImGui::GetIO().MousePos.x - CanvasX) / CanvasW, 0.0f, 1.0f);
			sPendingNotifyTime = Frac * PlayLength;
			ImGui::OpenPopup("##addNotifyCtx");
		}
	}
	if (ImGui::BeginPopup("##addNotifyCtx"))
	{
		ImGui::TextDisabled("%.3f s", sPendingNotifyTime);
		ImGui::Separator();

		// 등록된 UAnimNotify 자손 (instant) 클래스 enum → 콤보 메뉴.
		if (ImGui::BeginMenu("Add Notify (instant)"))
		{
			const TArray<UClass*> NotifyClasses = EnumerateConcreteSubclasses(UAnimNotify::StaticClass());
			if (NotifyClasses.empty())
			{
				ImGui::TextDisabled("(no UAnimNotify subclass registered)");
			}
			for (UClass* Cls : NotifyClasses)
			{
				if (ImGui::MenuItem(Cls->GetName()))
				{
					static int sNotifyCounter = 0;
					const FString Name = FString(Cls->GetName()) + "_" + std::to_string(++sNotifyCounter);
					Seq->GetMutableModelNotifies().push_back(
						MakeNotifyFromClass(Seq, Cls, Name, sPendingNotifyTime, 0.0f, false));
					Seq->RefreshRuntimeNotifies();
					InOutSelectedNotifyIndex = static_cast<int32>(Seq->GetMutableModelNotifies().size()) - 1;
					SaveSeqNow();
				}
			}
			ImGui::EndMenu();
		}

		// 등록된 UAnimNotifyState 자손 (duration) 클래스 enum → 콤보 메뉴.
		if (ImGui::BeginMenu("Add NotifyState (duration)"))
		{
			const TArray<UClass*> StateClasses = EnumerateConcreteSubclasses(UAnimNotifyState::StaticClass());
			if (StateClasses.empty())
			{
				ImGui::TextDisabled("(no UAnimNotifyState subclass registered)");
			}
			for (UClass* Cls : StateClasses)
			{
				if (ImGui::MenuItem(Cls->GetName()))
				{
					static int sStateCounter = 0;
					const FString Name = FString(Cls->GetName()) + "_" + std::to_string(++sStateCounter);
					const float DefaultDur = std::min(0.3f, std::max(PlayLength - sPendingNotifyTime, 0.05f));
					Seq->GetMutableModelNotifies().push_back(
						MakeNotifyFromClass(Seq, Cls, Name, sPendingNotifyTime, DefaultDur, true));
					Seq->RefreshRuntimeNotifies();
					InOutSelectedNotifyIndex = static_cast<int32>(Seq->GetMutableModelNotifies().size()) - 1;
					SaveSeqNow();
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	// ── 룰러 눈금 / 프레임 번호 ──
	const int RawStep = static_cast<int>(std::lround(NumFrames * 55.0f / CanvasW));
	const int Step    = NiceFrameStep(std::max(RawStep, 1));
	for (int F = 0; F <= EndFrame; ++F)
	{
		const float X = TimeToX((static_cast<float>(F) / EndFrame) * PlayLength);
		const bool  bLabeled = (F % Step == 0) || (F == EndFrame);
		DL->AddLine(ImVec2(X, Origin.y + (bLabeled ? RulerH * 0.45f : RulerH * 0.7f)),
		            ImVec2(X, Origin.y + RulerH), bLabeled ? ColTick : ColTickMinor);
		if (F % Step == 0)
		{
			char Buf[16];
			snprintf(Buf, sizeof(Buf), "%d", F);
			DL->AddText(ImVec2(X + 3.0f, Origin.y + 3.0f), ColLabel, Buf);
		}
	}
	// 룰러 좌측 헤더(필터 라벨)
	DL->AddRectFilled(Origin, ImVec2(Origin.x + HeaderW, Origin.y + RulerH), ColHeaderBg);
	DL->AddText(ImVec2(Origin.x + 8.0f, Origin.y + 4.0f), ColLabel, "Filter");
	DL->AddRect(ImVec2(Origin.x + 44.0f, Origin.y + 3.0f),
	            ImVec2(Origin.x + HeaderW - 6.0f, Origin.y + RulerH - 3.0f), ColTick);

	// 좌측 헤더 우측 끝에 "+" 추가 어포던스를 그린다. 클릭 시 true 반환.
	// (실제 추가 로직은 미연결 — 호출부에서 TODO 처리)
	auto DrawAddButton = [&](const char* Id, float RowTop, float RowHeight) -> bool
	{
		const float BtnSize = 16.0f;
		const ImVec2 BtnPos(Origin.x + HeaderW - BtnSize - 6.0f,
		                    RowTop + (RowHeight - BtnSize) * 0.5f);
		ImGui::SetCursorScreenPos(BtnPos);
		ImGui::InvisibleButton(Id, ImVec2(BtnSize, BtnSize));
		const bool bHov = ImGui::IsItemHovered();
		const ImU32 Col = bHov ? IM_COL32(230, 230, 230, 255) : IM_COL32(150, 150, 150, 255);
		if (bHov)
		{
			DL->AddRectFilled(BtnPos, ImVec2(BtnPos.x + BtnSize, BtnPos.y + BtnSize),
			                  IM_COL32(255, 255, 255, 28), 2.0f);
			ImGui::SetTooltip("Add (not wired yet)");
		}
		const ImVec2 C(BtnPos.x + BtnSize * 0.5f, BtnPos.y + BtnSize * 0.5f);
		DL->AddLine(ImVec2(C.x - 4.0f, C.y), ImVec2(C.x + 4.0f, C.y), Col, 1.5f);
		DL->AddLine(ImVec2(C.x, C.y - 4.0f), ImVec2(C.x, C.y + 4.0f), Col, 1.5f);
		return ImGui::IsItemClicked();
	};

	// ── 트랙 행 ──
	float RowY = Origin.y + RulerH;

	// Notifies (펼침 가능 + 트랙 추가 어포던스)
	const ImVec2 NotifyHeaderPos(Origin.x, RowY);
	ImGui::SetCursorScreenPos(ImVec2(Origin.x, RowY));
	ImGui::InvisibleButton("##notifyToggle", ImVec2(HeaderW, RowH));
	if (ImGui::IsItemClicked())
	{
		bNotifiesExpanded = !bNotifiesExpanded;
	}
	DrawTrackHeaderRow(DL, NotifyHeaderPos, HeaderW, RowH, "Notifies", true, bNotifiesExpanded);
	if (DrawAddButton("##addNotifyTrack", RowY, RowH))
	{
		// TODO: 노티파이 트랙 추가 — 엔진에 노티파이 트랙(인덱스) 데이터 모델이
		// 생기면 여기서 새 트랙을 push 하도록 연결한다. (현재는 표시 전용)
	}
	DL->AddRectFilled(ImVec2(CanvasX, RowY), ImVec2(CanvasX + CanvasW, RowY + RowH),
	                  IM_COL32(30, 30, 30, 255));
	DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f),
	            ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
	RowY += RowH;

	if (bNotifiesExpanded)
	{
		const float LaneY = RowY;
		DL->AddRectFilled(ImVec2(Origin.x, LaneY),
		                  ImVec2(Origin.x + HeaderW, LaneY + NotifyLaneH), ColHeaderBg);
		DL->AddText(ImVec2(Origin.x + 26.0f, LaneY + NotifyLaneH * 0.5f - 7.0f),
		            ColLabel, "1");
		if (DrawAddButton("##addNotify", LaneY, NotifyLaneH))
		{
			// 같은 컨텍스트 popup 재사용 — playhead 시각으로 진입. 클래스 picker 제공.
			sPendingNotifyTime = CurrentTime;
			ImGui::OpenPopup("##addNotifyCtx");
		}
		DL->AddRectFilled(ImVec2(CanvasX, LaneY),
		                  ImVec2(CanvasX + CanvasW, LaneY + NotifyLaneH), IM_COL32(24, 24, 24, 255));

		// 드래그로 시간 이동 / 우클릭으로 삭제(루프 후 지연 적용).
		// 직렬화 소스(DataModel)를 직접 편집 → 아래에서 dispatch 캐시 동기화.
		TArray<FAnimNotifyEvent>& Notifies = Seq->GetMutableModelNotifies();
		int PendingDelete = -1;
		static char  sRenameBuf[64]   = {};
		static float sGrabOffsetTime  = 0.0f; // 잡은 지점과 앵커의 시간 차(점프 방지)
		const float BadgeTop  = LaneY + 5.0f;
		const float BadgeBot  = LaneY + NotifyLaneH - 5.0f;
		const float BadgeMidY = (BadgeTop + BadgeBot) * 0.5f;
		for (int i = 0; i < static_cast<int>(Notifies.size()); ++i)
		{
			FAnimNotifyEvent& N   = Notifies[i];
			const float       NX  = TimeToX(N.TriggerTime);
			const std::string Nm  = N.NotifyName.ToString();
			const ImVec2      TSz = ImGui::CalcTextSize(Nm.c_str());

			// State (Duration>0) 는 시각적 폭 = Duration 그대로 (이름이 길어도 늘어나지 않음, 잘림).
			// Instant 는 이름 폭 + 패딩 (시간이 0 이라 시각적 폭 의미 없음).
			float BadgeW;
			if (N.Duration > 0.0f)
			{
				BadgeW = std::max(TimeToX(N.TriggerTime + N.Duration) - NX, 6.0f);
			}
			else
			{
				BadgeW = TSz.x + 16.0f;
			}

			ImGui::PushID(i);

			// State notify (Duration > 0) 면 오른쪽 끝에 6px resize 핸들을 분리.
			// 본체 hit-rect 가 핸들 영역을 침범하지 않게 BodyW 를 줄여 click 분리.
			constexpr float HandleW = 6.0f;
			const bool      bHasDur  = (N.Duration > 0.0f);
			const float     FullW    = BadgeW + 12.0f;
			const float     BodyW    = bHasDur ? std::max(FullW - HandleW, 8.0f) : FullW;

			ImGui::SetCursorScreenPos(ImVec2(NX - 6.0f, BadgeTop));
			ImGui::InvisibleButton("##notify", ImVec2(BodyW, BadgeBot - BadgeTop));
			const bool bHovered = ImGui::IsItemHovered();
			const bool bActive  = ImGui::IsItemActive();

			auto MouseTime = [&]() {
				return std::clamp((ImGui::GetIO().MousePos.x - CanvasX) / CanvasW,
				                  0.0f, 1.0f) * PlayLength;
			};

			// 누른 순간 잡은 지점-앵커 시간차를 기록 + selection 설정.
			if (ImGui::IsItemActivated())
			{
				sGrabOffsetTime          = MouseTime() - N.TriggerTime;
				InOutSelectedNotifyIndex = i;
			}
			// 임계값(io.MouseDragThreshold) 이상 움직였을 때만 이동 → 더블클릭은 제외.
			// Duration > 0 이면 End 도 같이 이동하므로 N.Duration 은 그대로, TriggerTime 만 갱신
			// + 시퀀스 우측 경계 클램프 시 (TriggerTime + Duration) 가 PlayLength 넘지 않게.
			if (bActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
			{
				const float MaxStart = bHasDur ? std::max(PlayLength - N.Duration, 0.0f)
				                               : PlayLength;
				N.TriggerTime    = std::clamp(MouseTime() - sGrabOffsetTime, 0.0f, MaxStart);
				sPendingSave    = true;   // 마우스 release 시 일괄 save.
			}
			if (bHovered || bActive)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			}
			if (bHovered && !bActive)
			{
				if (bHasDur)
				{
					ImGui::SetTooltip("%s\n%.3f s + %.3f s (state)\n(click: select / drag right edge: resize)",
					                  Nm.c_str(), N.TriggerTime, N.Duration);
				}
				else
				{
					ImGui::SetTooltip("%s\n%.3f s\n(click: select)",
					                  Nm.c_str(), N.TriggerTime);
				}
			}

			// 우측 끝 resize 핸들 — state notify 만.
			if (bHasDur)
			{
				ImGui::SetCursorScreenPos(ImVec2(NX - 6.0f + BodyW, BadgeTop));
				ImGui::InvisibleButton("##notifyR", ImVec2(HandleW, BadgeBot - BadgeTop));
				if (ImGui::IsItemHovered() || ImGui::IsItemActive())
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
				}
				if (ImGui::IsItemActivated())
				{
					InOutSelectedNotifyIndex = i;
				}
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
				{
					const float NewEnd = std::clamp(MouseTime(),
					                                N.TriggerTime + 0.01f, PlayLength);
					N.Duration     = NewEnd - N.TriggerTime;
					sPendingSave   = true;
				}
			}

			// 우클릭 컨텍스트 메뉴 — Rename / Delete. Properties 편집은 좌상단 AssetDetails 패널.
			bool bOpenRename = false;
			if (ImGui::BeginPopupContextItem("##notifyCtx"))
			{
				InOutSelectedNotifyIndex = i;
				ImGui::TextDisabled("%s", Nm.c_str());
				ImGui::Separator();
				if (ImGui::MenuItem("Rename"))
				{
					bOpenRename = true;
				}
				if (ImGui::MenuItem("Delete"))
				{
					PendingDelete = i;
				}
				ImGui::EndPopup();
			}
			if (bOpenRename)
			{
				snprintf(sRenameBuf, sizeof(sRenameBuf), "%s", Nm.c_str());
				ImGui::OpenPopup("##notifyRename");
			}
			if (ImGui::BeginPopup("##notifyRename"))
			{
				ImGui::TextDisabled("Rename Notify");
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
				}
				ImGui::SetNextItemWidth(180.0f);
				const bool bCommit = ImGui::InputText("##rn", sRenameBuf, sizeof(sRenameBuf),
					ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
				ImGui::SameLine();
				if ((bCommit || ImGui::Button("OK")) && sRenameBuf[0] != '\0')
				{
					N.NotifyName = FName(FString(sRenameBuf));
					Seq->RefreshRuntimeNotifies();
					SaveSeqNow();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();

			const float MarkNX = TimeToX(N.TriggerTime);
			// State (Duration>0) 와 instant 컬러 분리 — state 파랑, instant 초록.
			const ImU32 Fill = bHasDur
				? ((bHovered || bActive) ? IM_COL32(110, 175, 240, 255) : IM_COL32(74, 145, 226, 255))
				: ((bHovered || bActive) ? IM_COL32(120, 205, 125, 255) : IM_COL32(76, 175, 80,  255));
			const ImU32 Border = bHasDur
				? IM_COL32(30, 70, 130, 255)
				: IM_COL32(40, 95, 45,  255);

			const ImVec2 BMin(MarkNX, BadgeTop);
			const ImVec2 BMax(MarkNX + BadgeW, BadgeBot);
			DL->AddRectFilled(BMin, BMax, Fill, 3.0f);
			// 선택된 entry 면 노란 outline 으로 강조 (좌상단 AssetDetails 패널과 동기 확인용).
			if (i == InOutSelectedNotifyIndex)
			{
				DL->AddRect(ImVec2(BMin.x - 1.0f, BMin.y - 1.0f),
				            ImVec2(BMax.x + 1.0f, BMax.y + 1.0f),
				            IM_COL32(255, 200, 60, 255), 3.0f, 0, 2.0f);
			}
			DL->AddRect(BMin, BMax, Border, 3.0f);

			const float  DiaR = 4.5f;
			const ImVec2 DC(MarkNX, BadgeMidY);
			DL->AddQuadFilled(ImVec2(DC.x - DiaR, DC.y), ImVec2(DC.x, DC.y - DiaR),
			                  ImVec2(DC.x + DiaR, DC.y), ImVec2(DC.x, DC.y + DiaR), Fill);
			DL->AddQuad(ImVec2(DC.x - DiaR, DC.y), ImVec2(DC.x, DC.y - DiaR),
			            ImVec2(DC.x + DiaR, DC.y), ImVec2(DC.x, DC.y + DiaR), Border);

			if (!Nm.empty())
			{
				// State notify 는 시각적 폭이 우선 — 이름이 길면 "..." 으로 잘라 표기.
				// (Instant 는 BadgeW 가 이름 폭에 맞춰 자동 확장되므로 truncation 무영향.)
				const float TextStartX = MarkNX + 8.0f;
				const float MaxTextW   = std::max(BMax.x - TextStartX - 4.0f, 0.0f);
				const std::string Disp = TruncateWithEllipsis(Nm, MaxTextW);
				if (!Disp.empty())
				{
					DL->PushClipRect(BMin, BMax, true);
					DL->AddText(ImVec2(TextStartX, BadgeMidY - TSz.y * 0.5f),
					            IM_COL32(20, 35, 22, 255), Disp.c_str());
					DL->PopClipRect();
				}
			}
		}
		if (PendingDelete >= 0 && PendingDelete < static_cast<int>(Notifies.size()))
		{
			Notifies.erase(Notifies.begin() + PendingDelete);

			// 선택 인덱스 stale 처리 — 삭제된 항목이 선택이면 해제, 뒤쪽이었으면 한 칸 당김.
			if (InOutSelectedNotifyIndex == PendingDelete)
			{
				InOutSelectedNotifyIndex = -1;
			}
			else if (InOutSelectedNotifyIndex > PendingDelete)
			{
				--InOutSelectedNotifyIndex;
			}
			SaveSeqNow();
		}
		// 추가/삭제/드래그(시간 변경)를 dispatch 캐시에 반영 → 프리뷰에서 실제 발사.
		Seq->RefreshRuntimeNotifies();
		DL->AddLine(ImVec2(CanvasX, LaneY + NotifyLaneH - 1.0f),
		            ImVec2(CanvasX + CanvasW, LaneY + NotifyLaneH - 1.0f), ColSeparator);
		RowY += NotifyLaneH;
	}

	auto DrawSimpleHeaderRow = [&](const char* Label, bool bExpandable, bool bExpanded)
	{
		DrawTrackHeaderRow(DL, ImVec2(Origin.x, RowY), HeaderW, RowH, Label, bExpandable, bExpanded);
		DL->AddRectFilled(ImVec2(CanvasX, RowY), ImVec2(CanvasX + CanvasW, RowY + RowH),
		                  IM_COL32(30, 30, 30, 255));
		DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f),
		            ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
	};

	// Morph Curves
	{
		TArray<FMorphTargetCurve>& Curves = Seq->GetMutableMorphTargetCurves();
		char                       HeaderLabel[64];
		std::snprintf(HeaderLabel, sizeof(HeaderLabel), "Morph Curves (%zu)", Curves.size());

		const ImVec2 MorphHeaderPos(Origin.x, RowY);
		ImGui::SetCursorScreenPos(ImVec2(Origin.x, RowY));
		ImGui::InvisibleButton("##morphCurveToggle", ImVec2(HeaderW, RowH));
		if (ImGui::IsItemClicked())
		{
			bMorphCurvesExpanded = !bMorphCurvesExpanded;
		}
		DrawTrackHeaderRow(DL, MorphHeaderPos, HeaderW, RowH, HeaderLabel, true, bMorphCurvesExpanded);
		if (DrawAddButton("##addMorphCurve", RowY, RowH))
		{
			USkeletalMesh* MeshForMorphTargets = Comp ? Comp->GetSkeletalMesh() : nullptr;
			FSkeletalMesh* MeshAsset = MeshForMorphTargets ? MeshForMorphTargets->GetSkeletalMeshAsset() : nullptr;
			if (MeshAsset && !MeshAsset->MorphTargets.empty())
			{
				for (const FMorphTarget& Target : MeshAsset->MorphTargets)
				{
					if (Seq->GetMorphTargetCurves().end() == std::find_if(
						Seq->GetMorphTargetCurves().begin(),
						Seq->GetMorphTargetCurves().end(),
						[&](const FMorphTargetCurve& C)
						{
							return C.MorphTargetName == Target.Name;
						}
					))
					{
						FindOrAddMorphCurve(Seq, Target.Name);
						InOutSelectedMorphCurveIndex = static_cast<int32>(Seq->GetMorphTargetCurves().size()) - 1;
						InOutSelectedMorphKeyIndex   = -1;
						InOutSelectedNotifyIndex     = -1;
						SaveSeqNow();
						break;
					}
				}
			}
		}
		DL->AddRectFilled(ImVec2(CanvasX, RowY), ImVec2(CanvasX + CanvasW, RowY + RowH), IM_COL32(30, 30, 30, 255));
		DL->AddLine(ImVec2(CanvasX, RowY + RowH - 1.0f), ImVec2(CanvasX + CanvasW, RowY + RowH - 1.0f), ColSeparator);
		RowY += RowH;

		if (bMorphCurvesExpanded)
		{
			for (int32 CurveIndex = 0; CurveIndex < static_cast<int32>(Curves.size()); ++CurveIndex)
			{
				FMorphTargetCurve& Curve = Curves[CurveIndex];
				const bool bCurveSelected = (CurveIndex == InOutSelectedMorphCurveIndex);
				const float LaneH = bCurveSelected ? 118.0f : MorphLaneH;
				const float LaneY = RowY;
				ImGui::PushID(CurveIndex);

				DL->AddRectFilled(ImVec2(Origin.x, LaneY), ImVec2(Origin.x + HeaderW, LaneY + LaneH), ColHeaderBg);
				const std::string Nm = Curve.MorphTargetName.empty() ? "(Unnamed)" : Curve.MorphTargetName;
				DL->AddText(
					ImVec2(Origin.x + 26.0f, LaneY + 7.0f),
					ColRowText,
					TruncateWithEllipsis(Nm, HeaderW - 60.0f).c_str()
				);
				if (bCurveSelected)
				{
					DL->AddText(ImVec2(Origin.x + 26.0f, LaneY + 27.0f), ColLabel, "Bezier graph");
				}

				if (DrawAddButton("##addMorphKey", LaneY, MorphLaneH))
				{
					float NewValue = 0.0f;
					if (Comp)
					{
						NewValue = Comp->GetMorphTargetWeight(Curve.MorphTargetName);
					}
					AddOrUpdateMorphCurveKey(Curve, CurrentTime, NewValue);
					InOutSelectedMorphCurveIndex = CurveIndex;
					InOutSelectedMorphKeyIndex   = -1;
					for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Curve.Keys.size()); ++KeyIndex)
					{
						if (std::fabs(Curve.Curve.Keys[KeyIndex].TimeSeconds - CurrentTime) <= 1.0e-4f)
						{
							InOutSelectedMorphKeyIndex = KeyIndex;
							break;
						}
					}
					InOutSelectedNotifyIndex = -1;
					SaveSeqNow();
				}

				DL->AddRectFilled(
					ImVec2(CanvasX, LaneY),
					ImVec2(CanvasX + CanvasW, LaneY + LaneH),
					IM_COL32(24, 24, 24, 255)
				);

				float MinValue = 0.0f;
				float MaxValue = 1.0f;
				GetMorphCurveValueRange(Curve.Curve, MinValue, MaxValue);

				const float GraphTop = bCurveSelected ? LaneY + 10.0f : LaneY;
				const float GraphH = bCurveSelected ? LaneH - 20.0f : LaneH;
				auto XToTime = [&](float X) -> float
				{
					return std::clamp((X - CanvasX) / CanvasW, 0.0f, 1.0f) * PlayLength;
				};
				auto TimeToScreen = [&](float Time) -> float
				{
					return TimeToX(std::clamp(Time, 0.0f, PlayLength));
				};
				auto ValueToScreen = [&](float Value) -> float
				{
					return MorphValueToY(Value, GraphTop, GraphH, MinValue, MaxValue);
				};
				auto ScreenToValue = [&](float Y) -> float
				{
					return MorphYToValue(Y, GraphTop, GraphH, MinValue, MaxValue);
				};

				if (bCurveSelected)
				{
					for (int Grid = 0; Grid <= 4; ++Grid)
					{
						const float Gy = GraphTop + GraphH * (static_cast<float>(Grid) / 4.0f);
						DL->AddLine(ImVec2(CanvasX, Gy), ImVec2(CanvasX + CanvasW, Gy), IM_COL32(45, 45, 45, 255));
					}

					if (Curve.Curve.Keys.size() >= 2)
					{
						ImVec2 PrevPoint;
						bool bHasPrevPoint = false;
						const int32 SampleCount = std::max(32, static_cast<int32>(CanvasW / 8.0f));
						for (int32 Sample = 0; Sample <= SampleCount; ++Sample)
						{
							const float T = (static_cast<float>(Sample) / static_cast<float>(SampleCount)) * PlayLength;
							const float V = EvaluateTimelineMorphCurve(Curve.Curve, T, 0.0f);
							const ImVec2 P(TimeToScreen(T), ValueToScreen(V));
							if (bHasPrevPoint)
							{
								DL->AddLine(PrevPoint, P, IM_COL32(125, 190, 255, 255), 2.0f);
							}
							PrevPoint = P;
							bHasPrevPoint = true;
						}
					}
				}

				bool bBreakKeyLoop = false;
				for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(Curve.Curve.Keys.size()); ++KeyIndex)
				{
					ImGui::PushID(KeyIndex);

					FRawFloatCurveKey& Key = Curve.Curve.Keys[KeyIndex];
					const float X = TimeToScreen(Key.TimeSeconds);
					const float Y = bCurveSelected ? ValueToScreen(Key.Value) : (LaneY + LaneH * 0.5f);
					bool bDeleteKey = false;

					ImGui::SetCursorScreenPos(ImVec2(X - 6.0f, Y - 6.0f));
					ImGui::InvisibleButton("##morphKey", ImVec2(12.0f, 12.0f));
					if (ImGui::IsItemActivated())
					{
						InOutSelectedMorphCurveIndex = CurveIndex;
						InOutSelectedMorphKeyIndex   = KeyIndex;
						InOutSelectedNotifyIndex     = -1;
					}
					if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
					{
						const float MinTime = KeyIndex > 0 ? Curve.Curve.Keys[KeyIndex - 1].TimeSeconds + 1.0e-4f : 0.0f;
						const float MaxTime = KeyIndex + 1 < static_cast<int32>(Curve.Curve.Keys.size()) ? Curve.Curve.Keys[KeyIndex + 1].TimeSeconds - 1.0e-4f : PlayLength;
						Key.TimeSeconds = std::clamp(XToTime(ImGui::GetIO().MousePos.x), MinTime, MaxTime);
						if (bCurveSelected)
						{
							Key.Value = ClampMorphCurveValue(ScreenToValue(ImGui::GetIO().MousePos.y));
						}
						sPendingSave = true;
					}
					if (ImGui::BeginPopupContextItem("##morphKeyCtx"))
					{
						InOutSelectedMorphCurveIndex = CurveIndex;
						InOutSelectedMorphKeyIndex   = KeyIndex;
						InOutSelectedNotifyIndex     = -1;
						if (ImGui::MenuItem("Delete Key"))
						{
							bDeleteKey = true;
						}
						ImGui::EndPopup();
					}

					if (bDeleteKey)
					{
						Curve.Curve.Keys.erase(Curve.Curve.Keys.begin() + KeyIndex);
						InOutSelectedMorphKeyIndex = -1;
						InOutSelectedNotifyIndex = -1;
						SaveSeqNow();
						ImGui::PopID();
						bBreakKeyLoop = true;
						break;
					}

					const bool bSelected = CurveIndex == InOutSelectedMorphCurveIndex && KeyIndex == InOutSelectedMorphKeyIndex;
					const ImU32 Fill = bSelected ? IM_COL32(255, 200, 60, 255) : IM_COL32(180, 110, 230, 255);
					DL->AddQuadFilled(
						ImVec2(X - 5.0f, Y),
						ImVec2(X, Y - 5.0f),
						ImVec2(X + 5.0f, Y),
						ImVec2(X, Y + 5.0f),
						Fill
					);

					ImGui::PopID();
				}

				if (!bBreakKeyLoop && bCurveSelected && InOutSelectedMorphKeyIndex >= 0 && InOutSelectedMorphKeyIndex < static_cast<int32>(Curve.Curve.Keys.size()))
				{
					const int32 SelectedKeyIndex = InOutSelectedMorphKeyIndex;
					FRawFloatCurveKey& SelectedKey = Curve.Curve.Keys[SelectedKeyIndex];
					const ImVec2 KeyPoint(TimeToScreen(SelectedKey.TimeSeconds), ValueToScreen(SelectedKey.Value));

					const bool bHasArriveBezierSegment = SelectedKeyIndex > 0
						&& (Curve.Curve.Keys[SelectedKeyIndex - 1].Interpolation & 4) == 4;
					const bool bHasLeaveBezierSegment = SelectedKeyIndex + 1 < static_cast<int32>(Curve.Curve.Keys.size())
						&& (SelectedKey.Interpolation & 4) == 4;

					ImGui::PushID("BezierHandles");
					if (bHasArriveBezierSegment)
					{
						const FRawFloatCurveKey& PrevKey = Curve.Curve.Keys[SelectedKeyIndex - 1];
						const float HandleTime = GetArriveHandleTime(PrevKey, SelectedKey);
						const float Tangent = SelectedKey.TangentMode == 0 ? EstimateTimelineCurveTangent(Curve.Curve, SelectedKeyIndex) : SelectedKey.ArriveTangent;
						const float HandleValue = SelectedKey.Value - Tangent * (SelectedKey.TimeSeconds - HandleTime);
						ImVec2 HandlePoint(TimeToScreen(HandleTime), ValueToScreen(HandleValue));
						DL->AddLine(KeyPoint, HandlePoint, IM_COL32(255, 170, 90, 180), 1.5f);
						DL->AddCircleFilled(HandlePoint, 4.0f, IM_COL32(255, 170, 90, 255));

						ImGui::SetCursorScreenPos(ImVec2(HandlePoint.x - 6.0f, HandlePoint.y - 6.0f));
						ImGui::InvisibleButton("##arriveHandle", ImVec2(12.0f, 12.0f));
						if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
						{
							SetArriveHandleFromPoint(PrevKey, SelectedKey, XToTime(ImGui::GetIO().MousePos.x), ScreenToValue(ImGui::GetIO().MousePos.y));
							sPendingSave = true;
						}
					}

					if (bHasLeaveBezierSegment)
					{
						const FRawFloatCurveKey& NextKey = Curve.Curve.Keys[SelectedKeyIndex + 1];
						const float HandleTime = GetLeaveHandleTime(SelectedKey, NextKey);
						const float Tangent = SelectedKey.TangentMode == 0 ? EstimateTimelineCurveTangent(Curve.Curve, SelectedKeyIndex) : SelectedKey.LeaveTangent;
						const float HandleValue = SelectedKey.Value + Tangent * (HandleTime - SelectedKey.TimeSeconds);
						ImVec2 HandlePoint(TimeToScreen(HandleTime), ValueToScreen(HandleValue));
						DL->AddLine(KeyPoint, HandlePoint, IM_COL32(255, 170, 90, 180), 1.5f);
						DL->AddCircleFilled(HandlePoint, 4.0f, IM_COL32(255, 170, 90, 255));

						ImGui::SetCursorScreenPos(ImVec2(HandlePoint.x - 6.0f, HandlePoint.y - 6.0f));
						ImGui::InvisibleButton("##leaveHandle", ImVec2(12.0f, 12.0f));
						if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, -1.0f))
						{
							SetLeaveHandleFromPoint(SelectedKey, NextKey, XToTime(ImGui::GetIO().MousePos.x), ScreenToValue(ImGui::GetIO().MousePos.y));
							sPendingSave = true;
						}
					}
					ImGui::PopID();
				}

				DL->AddLine(
					ImVec2(CanvasX, LaneY + LaneH - 1.0f),
					ImVec2(CanvasX + CanvasW, LaneY + LaneH - 1.0f),
					ColSeparator
				);
				ImGui::PopID();
				RowY += LaneH;
			}
		}
	}


	// Additive Layer Tracks: 언리얼에서는 에디터 내 비파괴 보정(저작) 기능이며,
	// 이 엔진에는 해당 저작 데이터 모델이 없으므로 빈 헤더만 표시 (읽기 전용).
	DrawSimpleHeaderRow("Additive Layer Tracks", false, false);
	RowY += RowH;

	// Attributes: 엔진에 커스텀 본 어트리뷰트 데이터가 없어 비어있음 (읽기 전용)
	DrawSimpleHeaderRow("Attributes", false, false);
	RowY += RowH;

	// 실제 트랙 행 높이가 viewport보다 커져도 transport는 고정하고,
	// 위쪽 track child만 스크롤되도록 content height만 확장한다.
	TrackAreaH = std::max(TrackAreaH, RowY - Origin.y);

	// 남은 캔버스 빈 영역
	if (RowY < Origin.y + TrackAreaH)
	{
		DL->AddRectFilled(ImVec2(CanvasX, RowY),
		                  ImVec2(CanvasX + CanvasW, Origin.y + TrackAreaH), IM_COL32(30, 30, 30, 255));
	}

	// ── 플레이헤드 ──
	const float PX = TimeToX(CurrentTime);
	DL->AddLine(ImVec2(PX, Origin.y + RulerH), ImVec2(PX, Origin.y + TrackAreaH),
	            ColPlayhead, 1.5f);
	DL->AddTriangleFilled(ImVec2(PX - 6.0f, Origin.y),
	                      ImVec2(PX + 6.0f, Origin.y),
	                      ImVec2(PX, Origin.y + 9.0f), ColPlayhead);

	// 헤더/캔버스 구분선 + 외곽
	DL->AddLine(ImVec2(CanvasX, Origin.y), ImVec2(CanvasX, Origin.y + TrackAreaH), ColSeparator);
	DL->AddRect(Origin, ImVec2(Origin.x + FullW, Origin.y + TrackAreaH), ColSeparator);

	// 레이아웃 영역 확정: track child 안에서는 실제 트랙 content height만 잡는다.
	ImGui::SetCursorScreenPos(Origin);
	ImGui::Dummy(ImVec2(FullW, TrackAreaH));

	ImGui::EndChild();

	// ── 하단 트랜스포트 행 ──
	// transport는 outer child에 고정하고, 위쪽 timeline track만 별도 child에서 스크롤한다.
	DL = ImGui::GetWindowDrawList();
	const ImVec2 TransportOrigin = ImGui::GetCursorScreenPos();
	const float  TransportW      = ImGui::GetContentRegionAvail().x;
	DL->AddRectFilled(TransportOrigin,
	                  ImVec2(TransportOrigin.x + TransportW, TransportOrigin.y + TransportH),
	                  ColPanelBg);

	ImGui::SetCursorScreenPos(ImVec2(TransportOrigin.x + 6.0f, TransportOrigin.y + 6.0f));
	ImGui::AlignTextToFramePadding();
	ImGui::Text("%d", 0);                 // 범위 시작 프레임
	ImGui::SameLine(0.0f, 14.0f);

	FAnimationTransportBar::Render(NodeInst, Comp, PlayLength, NumFrames);

	// 우측: 현재 프레임 입력 / 끝 프레임
	ImGui::SameLine(0.0f, 16.0f);
	int FrameInput = CurrentFrame;
	ImGui::SetNextItemWidth(64.0f);
	if (ImGui::InputInt("##curFrame", &FrameInput, 0, 0) && NodeInst)
	{
		FrameInput = std::clamp(FrameInput, 0, EndFrame);
		NodeInst->SetCurrentTime((static_cast<float>(FrameInput) / EndFrame) * PlayLength);
		if (Comp) Comp->SetPlaying(false);
	}
	ImGui::SameLine(0.0f, 6.0f);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("/ %d", EndFrame);

	ImGui::SetCursorScreenPos(TransportOrigin);
	ImGui::Dummy(ImVec2(TransportW, TransportH));

	// Drag/Resize 의 누적 dirty 를 마우스 release 에 일괄 save (frame 별 디스크 thrash 방지).
	if (sPendingSave && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		SaveSeqNow();
		sPendingSave = false;
	}

	ImGui::EndChild();
}

bool FAnimationTimelinePanel::RenderNotifyDetails(UAnimSequence* Seq, int32 SelectedNotifyIndex)
{
	if (!Seq) return false;
	const TArray<FAnimNotifyEvent>& Notifies = Seq->GetNotifies();
	if (SelectedNotifyIndex < 0 || SelectedNotifyIndex >= static_cast<int32>(Notifies.size()))
	{
		return false;
	}

	// const_cast — GetMutableModelNotifies 가 DataModel 측 mutable 컨테이너 보장.
	TArray<FAnimNotifyEvent>& Mutable = Seq->GetMutableModelNotifies();
	if (SelectedNotifyIndex >= static_cast<int32>(Mutable.size())) return false;
	FAnimNotifyEvent& N = Mutable[SelectedNotifyIndex];

	const FString ClsName = N.Notify      ? FString(N.Notify->GetClass()->GetName())
	                      : N.NotifyState ? FString(N.NotifyState->GetClass()->GetName())
	                                      : FString("None");
	const bool bIsState = (N.NotifyState != nullptr) && (N.Duration > 0.0f);

	ImGui::TextUnformatted("Notify Details");
	ImGui::Separator();
	ImGui::TextDisabled("Class:  %s", ClsName.c_str());
	ImGui::TextDisabled("Type:   %s", bIsState ? "State (duration)" : "Instant");
	ImGui::Dummy(ImVec2(0, 4));

	bool bChanged = false;

	// Name 편집 — 인플레이스.
	{
		FString Cur = N.NotifyName.ToString();
		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Cur.c_str(), _TRUNCATE);
		ImGui::TextUnformatted("Name");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##notifyName", Buf, sizeof(Buf)))
		{
			N.NotifyName = FName(FString(Buf));
			bChanged     = true;
		}
	}

	// TriggerTime 편집 + (state) Duration 편집.
	{
		ImGui::TextUnformatted("Trigger Time (sec)");
		ImGui::SetNextItemWidth(-FLT_MIN);
		const float MaxStart = bIsState
			? std::max(Seq->GetPlayLength() - N.Duration, 0.0f)
			: Seq->GetPlayLength();
		if (ImGui::DragFloat("##trig", &N.TriggerTime, 0.01f, 0.0f, MaxStart, "%.3f"))
		{
			bChanged = true;
		}
	}
	if (bIsState)
	{
		ImGui::TextUnformatted("Duration (sec)");
		ImGui::SetNextItemWidth(-FLT_MIN);
		const float MaxDur = std::max(Seq->GetPlayLength() - N.TriggerTime, 0.01f);
		if (ImGui::DragFloat("##dur", &N.Duration, 0.01f, 0.01f, MaxDur, "%.3f"))
		{
			bChanged = true;
		}
	}

	ImGui::Dummy(ImVec2(0, 6));
	ImGui::Separator();
	ImGui::TextUnformatted("Properties");
	ImGui::Separator();

	if (N.Notify)
	{
		if (RenderObjectPropertiesInline(N.Notify))      bChanged = true;
	}
	if (N.NotifyState)
	{
		if (RenderObjectPropertiesInline(N.NotifyState)) bChanged = true;
	}
	if (!N.Notify && !N.NotifyState)
	{
		ImGui::TextDisabled("(no notify object bound)");
	}

	if (bChanged)
	{
		Seq->RefreshRuntimeNotifies();
		FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
	}
	return bChanged;
}

bool FAnimationTimelinePanel::RenderMorphDetails(
	UAnimSequence* Seq,
	USkeletalMesh* SkeletalMesh,
	int32&         InOutSelectedMorphCurveIndex,
	int32&         InOutSelectedMorphKeyIndex
	)
{
	if (!Seq) return false;
	TArray<FMorphTargetCurve>& Curves = Seq->GetMutableMorphTargetCurves();
	if (InOutSelectedMorphCurveIndex < 0 || InOutSelectedMorphCurveIndex >= static_cast<int32>(Curves.size()))
	{
		return false;
	}

	FMorphTargetCurve& Curve                    = Curves[InOutSelectedMorphCurveIndex];
	bool               bChanged                 = false;
	bool               bImmediateSave           = false;
	static bool        sMorphDetailsPendingSave = false;

	ImGui::TextUnformatted("Morph Curve Details");
	ImGui::Separator();

	FSkeletalMesh* MeshAsset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	ImGui::TextUnformatted("Target");
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (MeshAsset && !MeshAsset->MorphTargets.empty())
	{
		const int32 CurrentMorphIndex = MeshAsset->FindMorphTargetIndex(Curve.MorphTargetName);
		const char* PreviewName = CurrentMorphIndex >= 0
			? MeshAsset->MorphTargets[CurrentMorphIndex].Name.c_str()
			: "<Invalid Morph Target>";

		if (ImGui::BeginCombo("##morphTargetName", PreviewName))
		{
			for (int32 MorphIndex = 0; MorphIndex < static_cast<int32>(MeshAsset->MorphTargets.size()); ++MorphIndex)
			{
				const FMorphTarget& Target = MeshAsset->MorphTargets[MorphIndex];
				const bool bSelected = (MorphIndex == CurrentMorphIndex);
				const char* TargetName = Target.Name.empty() ? "Unnamed" : Target.Name.c_str();

				if (ImGui::Selectable(TargetName, bSelected))
				{
					Curve.MorphTargetName = Target.Name;
					bChanged = true;
					bImmediateSave = true;
				}

				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (CurrentMorphIndex < 0 && !Curve.MorphTargetName.empty())
		{
			ImGui::TextDisabled("Current target does not exist on this skeletal mesh.");
		}
	}
	else
	{
		ImGui::TextDisabled("No morph targets on current mesh.");
	}

	FRawFloatCurveKey* SelectedKey = nullptr;
	if (InOutSelectedMorphKeyIndex >= 0 && InOutSelectedMorphKeyIndex < static_cast<int32>(Curve.Curve.Keys.size()))
	{
		SelectedKey = &Curve.Curve.Keys[InOutSelectedMorphKeyIndex];
	}

	static UMorphCurveEditObject* sMorphEditObject = nullptr;
	if (!IsValid(sMorphEditObject))
	{
		sMorphEditObject = UObjectManager::Get().CreateObject<UMorphCurveEditObject>();
	}

	if (sMorphEditObject)
	{
		sMorphEditObject->LoadFrom(Curve, SelectedKey, Seq->GetPlayLength());

		ImGui::Dummy(ImVec2(0, 6));
		ImGui::Separator();
		ImGui::TextUnformatted(SelectedKey ? "Reflected Morph Curve / Key Properties" : "Reflected Morph Curve Properties");
		ImGui::Separator();

		if (RenderObjectPropertiesInline(sMorphEditObject))
		{
			sMorphEditObject->ApplyTo(Curve, SelectedKey);
			if (SelectedKey)
			{
				SelectedKey->Value = ClampMorphCurveValue(SelectedKey->Value);
			}
			bChanged = true;
			sMorphDetailsPendingSave = true;
		}
	}

	if (SelectedKey && ((SelectedKey->Interpolation & 4) == 4))
	{
		ImGui::Dummy(ImVec2(0, 4));
		if (ImGui::SmallButton("Flatten"))
		{
			SelectedKey->ArriveTangent = 0.0f;
			SelectedKey->LeaveTangent  = 0.0f;
			SelectedKey->TangentMode   = 1;
			bChanged                  = true;
			bImmediateSave            = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Auto Handles"))
		{
			SelectedKey->TangentMode             = 0;
			SelectedKey->bArriveTangentWeighted = false;
			SelectedKey->bLeaveTangentWeighted  = false;
			bChanged                            = true;
			bImmediateSave                      = true;
		}
		ImGui::TextDisabled("Bezier handles are edited in the timeline graph. The properties above are reflected UPROPERTY fields.");
	}

	if (bChanged)
	{
		std::sort(
			Curve.Curve.Keys.begin(),
			Curve.Curve.Keys.end(),
			[](const FRawFloatCurveKey& A, const FRawFloatCurveKey& B)
			{
				return A.TimeSeconds < B.TimeSeconds;
			}
		);
	}

	if (bImmediateSave)
	{
		FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
		sMorphDetailsPendingSave = false;
	}
	else if (sMorphDetailsPendingSave && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
	{
		FAnimationManager::Get().SaveAnimationPreservingMetadata(Seq);
		sMorphDetailsPendingSave = false;
	}

	return bChanged;
}
