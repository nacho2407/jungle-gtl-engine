#include "Materials/Graph/MaterialHlslGenerator.h"
#include "Render/Types/RenderConstants.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>

namespace
{
	struct FEmittedNode
	{
		FString               Expr;              // "n_5" 또는 텍스처면 "Tex_Diffuse"
		EMaterialGraphPinType Type = EMaterialGraphPinType::Float;
		bool                  bIsTexture = false;
	};

	struct FParamDecl
	{
		FString               Name;
		FString               HlslName;
		EMaterialGraphPinType Type = EMaterialGraphPinType::Float;
		FVector4              Value = FVector4(0, 0, 0, 0);
	};

	struct FTextureDecl
	{
		FString              Name;
		FString              HlslName;
		FString              Path;
		EMaterialTextureSlot Slot = EMaterialTextureSlot::Diffuse;
	};

	int32 ComponentCount(EMaterialGraphPinType Type)
	{
		switch (Type)
		{
		case EMaterialGraphPinType::Float:  return 1;
		case EMaterialGraphPinType::Float2:
		case EMaterialGraphPinType::UV:     return 2;
		case EMaterialGraphPinType::Float3:
		case EMaterialGraphPinType::Color:  return 3;
		case EMaterialGraphPinType::Float4: return 4;
		default: return 0;
		}
	}

	// 두 vector 타입 중 컴포넌트 수가 큰 쪽을 반환. 동수면 A 우선 (Color/Float3 alias 보존).
	EMaterialGraphPinType MaxType(EMaterialGraphPinType A, EMaterialGraphPinType B)
	{
		const int32 CA = ComponentCount(A);
		const int32 CB = ComponentCount(B);
		if (CA == 0 && CB == 0) return EMaterialGraphPinType::Float;
		if (CA == 0) return B;
		if (CB == 0) return A;
		return CA >= CB ? A : B;
	}

	FString SanitizeIdentifier(FString In)
	{
		if (In.empty()) return "Value";
		for (char& Ch : In)
		{
			const unsigned char U = static_cast<unsigned char>(Ch);
			if (!std::isalnum(U) && Ch != '_') Ch = '_';
		}
		if (std::isdigit(static_cast<unsigned char>(In[0]))) In = "_" + In;
		return In;
	}

	FString HlslType(EMaterialGraphPinType Type)
	{
		switch (Type)
		{
		case EMaterialGraphPinType::Float:    return "float";
		case EMaterialGraphPinType::Float2:
		case EMaterialGraphPinType::UV:       return "float2";
		case EMaterialGraphPinType::Float3:
		case EMaterialGraphPinType::Color:    return "float3";
		case EMaterialGraphPinType::Float4:   return "float4";
		case EMaterialGraphPinType::Texture2D:return "Texture2D";
		case EMaterialGraphPinType::Bool:     return "bool";
		case EMaterialGraphPinType::Sampler:  return "SamplerState";
		}
		return "float";
	}

	FString Literal(EMaterialGraphPinType Type, const FVector4& V)
	{
		char Buffer[160];
		switch (Type)
		{
		case EMaterialGraphPinType::Float:
			std::snprintf(Buffer, sizeof(Buffer), "%.6ff", V.X);
			return Buffer;
		case EMaterialGraphPinType::Float2:
		case EMaterialGraphPinType::UV:
			std::snprintf(Buffer, sizeof(Buffer), "float2(%.6ff, %.6ff)", V.X, V.Y);
			return Buffer;
		case EMaterialGraphPinType::Float3:
		case EMaterialGraphPinType::Color:
			std::snprintf(Buffer, sizeof(Buffer), "float3(%.6ff, %.6ff, %.6ff)", V.X, V.Y, V.Z);
			return Buffer;
		case EMaterialGraphPinType::Float4:
			std::snprintf(Buffer, sizeof(Buffer), "float4(%.6ff, %.6ff, %.6ff, %.6ff)", V.X, V.Y, V.Z, V.W);
			return Buffer;
		default:
			return "0";
		}
	}

	EMaterialGraphPinType MaskType(const FString& Mask)
	{
		const int32 Count = static_cast<int32>(Mask.size());
		if (Count <= 1) return EMaterialGraphPinType::Float;
		if (Count == 2) return EMaterialGraphPinType::Float2;
		if (Count == 3) return EMaterialGraphPinType::Float3;
		return EMaterialGraphPinType::Float4;
	}

	FString NormalizeMask(FString Mask)
	{
		if (Mask.empty()) Mask = "RGBA";
		FString Out;
		for (char Ch : Mask)
		{
			switch (std::toupper(static_cast<unsigned char>(Ch)))
			{
			case 'R': Out += 'r'; break;
			case 'G': Out += 'g'; break;
			case 'B': Out += 'b'; break;
			case 'A': Out += 'a'; break;
			default: break;
			}
		}
		return Out.empty() ? FString("rgba") : Out;
	}

	// HLSL 타입 변환 — broadcast / swizzle down / zero-pad up를 모두 처리.
	// 처리 못 하는 케이스는 에러를 등록하고 Expr을 그대로 반환.
	FString ConvertExpr(const FString& Expr, EMaterialGraphPinType From, EMaterialGraphPinType To, TArray<FString>* OutErrors = nullptr)
	{
		if (From == To) return Expr;

		// Color ↔ Float3, UV ↔ Float2 는 alias.
		if ((From == EMaterialGraphPinType::Color && To == EMaterialGraphPinType::Float3) ||
			(From == EMaterialGraphPinType::Float3 && To == EMaterialGraphPinType::Color))
			return Expr;
		if ((From == EMaterialGraphPinType::UV && To == EMaterialGraphPinType::Float2) ||
			(From == EMaterialGraphPinType::Float2 && To == EMaterialGraphPinType::UV))
			return Expr;

		const int32 FromN = ComponentCount(From);
		const int32 ToN = ComponentCount(To);

		if (FromN == 0 || ToN == 0)
		{
			if (OutErrors) OutErrors->push_back("Cannot convert non-scalar/vector type in material graph.");
			return Expr;
		}

		// 스칼라 → 벡터 : broadcast
		if (FromN == 1)
		{
			switch (ToN)
			{
			case 2: return "float2(" + Expr + ", " + Expr + ")";
			case 3: return "float3(" + Expr + ", " + Expr + ", " + Expr + ")";
			case 4: return "float4(" + Expr + ", " + Expr + ", " + Expr + ", " + Expr + ")";
			default: return Expr;
			}
		}

		// 다운캐스트 : swizzle
		if (FromN > ToN)
		{
			static const char* Swizzles[] = { "", ".x", ".xy", ".xyz", ".xyzw" };
			return "(" + Expr + ")" + Swizzles[ToN];
		}

		// 업캐스트 : 0으로 패딩
		const char* ToHlsl = (ToN == 2) ? "float2" : (ToN == 3) ? "float3" : "float4";
		FString Padded = FString(ToHlsl) + "(" + Expr;
		for (int32 i = 0; i < ToN - FromN; ++i) Padded += ", 0.0f";
		Padded += ")";
		return Padded;
	}

	// 다중 출력 노드(ParticleColor/VertexColor/TextureSample 등)에서 어떤 출력 pin이 링크 소스인지에 따라
	// 노드의 풀 expression에 swizzle을 적용해 채널/서브벡터를 꺼냄.
	// 알 수 없는 pin 이름이면 그대로 두고 ConvertExpr가 처리하도록 위임.
	bool ApplyOutputPinSwizzle(const FString& PinName, FString& InOutExpr, EMaterialGraphPinType& InOutType)
	{
		auto Scalar = [&](const char* Sw)
		{
			InOutExpr = "(" + InOutExpr + ")" + Sw;
			InOutType = EMaterialGraphPinType::Float;
			return true;
		};
		if (PinName == "R" || PinName == "Param1") return Scalar(".r");
		if (PinName == "G" || PinName == "Param2") return Scalar(".g");
		if (PinName == "B" || PinName == "Param3") return Scalar(".b");
		if (PinName == "A" || PinName == "Param4") return Scalar(".a");
		if (PinName == "RGB") { InOutExpr = "(" + InOutExpr + ").rgb"; InOutType = EMaterialGraphPinType::Float3; return true; }
		return false;
	}

	const FMaterialGraphPin* FindPinByName(const FMaterialGraphNode& Node, const char* Name, EMaterialGraphPinKind Kind)
	{
		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			if (Pin.Kind == Kind && Pin.DisplayName.ToString() == Name) return &Pin;
		}
		return nullptr;
	}

	const FMaterialGraphPin* FindFirstOutputPin(const FMaterialGraphNode& Node)
	{
		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			if (Pin.Kind == EMaterialGraphPinKind::Output) return &Pin;
		}
		return nullptr;
	}

	const FMaterialGraphLink* FindInputLink(const FMaterialGraph& Graph, uint32 InputPinId)
	{
		for (const FMaterialGraphLink& Link : Graph.Links)
		{
			if (Link.ToPinId == InputPinId) return &Link;
		}
		return nullptr;
	}

	// ─── EvaluateMaterial 본문을 만드는 컨텍스트 ───
	// 각 노드를 한 번만 평가해 로컬 변수에 저장 → 다이아몬드 그래프에서도 중복 평가 없음.
	class FHlslBuildContext
	{
	public:
		FHlslBuildContext(const FMaterialGraph& InGraph, FMaterialCompileResult& InResult, uint32 InPerMaterialSlot)
			: Graph(InGraph), Result(InResult), PerMaterialSlot(InPerMaterialSlot) {}

		FEmittedNode Emit(const FMaterialGraphNode& Node)
		{
			auto Found = Emitted.find(Node.NodeId);
			if (Found != Emitted.end()) return Found->second;

			// 텍스처 객체는 로컬을 만들지 않고 Texture2D 변수 이름을 그대로 사용.
			if (Node.Type == EMaterialGraphNodeType::TextureObject)
			{
				FEmittedNode Out;
				Out.Expr = RegisterTexture(Node);
				Out.Type = EMaterialGraphPinType::Texture2D;
				Out.bIsTexture = true;
				Emitted[Node.NodeId] = Out;
				return Out;
			}

			FString RhsExpr = "0";
			EMaterialGraphPinType ResultType = EMaterialGraphPinType::Float;

			switch (Node.Type)
			{
			case EMaterialGraphNodeType::TextureSample:
			{
				const FString TextureName = TextureExpressionForSampleNode(Node);
				const FString UV = InputExpr(Node, "UV", "Input.UV0", EMaterialGraphPinType::Float2, EMaterialGraphPinType::Float2);
				RhsExpr = TextureName + ".Sample(LinearWrapSampler, " + UV + ")";
				ResultType = EMaterialGraphPinType::Float4;
				break;
			}
			case EMaterialGraphNodeType::ScalarParameter:
			case EMaterialGraphNodeType::VectorParameter:
			case EMaterialGraphNodeType::ColorParameter:
			{
				const FString ParamName = Node.ParameterName.empty()
					? ("Param" + std::to_string(Node.NodeId))
					: Node.ParameterName;
				const EMaterialGraphPinType ParamType =
					Node.Type == EMaterialGraphNodeType::ScalarParameter ? EMaterialGraphPinType::Float :
					Node.Type == EMaterialGraphNodeType::VectorParameter ? EMaterialGraphPinType::Float4 :
					EMaterialGraphPinType::Color;
				RhsExpr = RegisterParameter(ParamName, ParamType, Node.Value);
				ResultType = ParamType;
				break;
			}
			case EMaterialGraphNodeType::ConstantFloat:
				RhsExpr = Literal(EMaterialGraphPinType::Float, Node.Value);
				ResultType = EMaterialGraphPinType::Float;
				break;
			case EMaterialGraphNodeType::ConstantFloat2:
				RhsExpr = Literal(EMaterialGraphPinType::Float2, Node.Value);
				ResultType = EMaterialGraphPinType::Float2;
				break;
			case EMaterialGraphNodeType::ConstantFloat3:
				RhsExpr = Literal(EMaterialGraphPinType::Float3, Node.Value);
				ResultType = EMaterialGraphPinType::Float3;
				break;
			case EMaterialGraphNodeType::ConstantFloat4:
				RhsExpr = Literal(EMaterialGraphPinType::Float4, Node.Value);
				ResultType = EMaterialGraphPinType::Float4;
				break;
			case EMaterialGraphNodeType::Add:
			case EMaterialGraphNodeType::Subtract:
			case EMaterialGraphNodeType::Multiply:
			case EMaterialGraphNodeType::Divide:
			case EMaterialGraphNodeType::Power:
			{
				// upstream의 실제 타입으로 OpType을 동적 결정. Float + Float → Float.
				FResolved RA = ResolveInput(Node, "A", "0.0f", EMaterialGraphPinType::Float);
				const bool bIsMulDiv = (Node.Type == EMaterialGraphNodeType::Multiply || Node.Type == EMaterialGraphNodeType::Divide);
				FResolved RB = ResolveInput(Node, "B", bIsMulDiv ? "1.0f" : "0.0f", EMaterialGraphPinType::Float);

				const EMaterialGraphPinType OpType = MaxType(RA.Source.Type, RB.Source.Type);
				const FString A = ConvertExpr(RA.Source.Expr, RA.Source.Type, OpType, &Result.Errors);
				const FString B = ConvertExpr(RB.Source.Expr, RB.Source.Type, OpType, &Result.Errors);

				const char* Op =
					Node.Type == EMaterialGraphNodeType::Add ? "+" :
					Node.Type == EMaterialGraphNodeType::Subtract ? "-" :
					Node.Type == EMaterialGraphNodeType::Multiply ? "*" :
					Node.Type == EMaterialGraphNodeType::Divide ? "/" : "";
				RhsExpr = Node.Type == EMaterialGraphNodeType::Power
					? "pow(" + A + ", " + B + ")"
					: "(" + A + " " + Op + " " + B + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::OneMinus:
			{
				FResolved RV = ResolveInput(Node, "Value", "0.0f", EMaterialGraphPinType::Float);
				const EMaterialGraphPinType OpType = RV.Source.Type;
				const FString V = ConvertExpr(RV.Source.Expr, RV.Source.Type, OpType, &Result.Errors);
				RhsExpr = "(1.0f - " + V + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::Saturate:
			{
				FResolved RV = ResolveInput(Node, "Value", "0.0f", EMaterialGraphPinType::Float);
				const EMaterialGraphPinType OpType = RV.Source.Type;
				const FString V = ConvertExpr(RV.Source.Expr, RV.Source.Type, OpType, &Result.Errors);
				RhsExpr = "saturate(" + V + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::Clamp:
			{
				FResolved RV = ResolveInput(Node, "Value", "0.0f", EMaterialGraphPinType::Float);
				FResolved RMn = ResolveInput(Node, "Min", "0.0f", EMaterialGraphPinType::Float);
				FResolved RMx = ResolveInput(Node, "Max", "1.0f", EMaterialGraphPinType::Float);
				// V의 타입 기준으로 Min/Max를 broadcast/swizzle.
				const EMaterialGraphPinType OpType = RV.Source.Type;
				const FString V = ConvertExpr(RV.Source.Expr, RV.Source.Type, OpType, &Result.Errors);
				const FString Mn = ConvertExpr(RMn.Source.Expr, RMn.Source.Type, OpType, &Result.Errors);
				const FString Mx = ConvertExpr(RMx.Source.Expr, RMx.Source.Type, OpType, &Result.Errors);
				RhsExpr = "clamp(" + V + ", " + Mn + ", " + Mx + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::Lerp:
			{
				// A/B 중 더 큰 타입을 따라감. Alpha는 항상 Float (또는 동일 vector — 여기선 Float로 강제).
				FResolved RA = ResolveInput(Node, "A", "0.0f", EMaterialGraphPinType::Float);
				FResolved RB = ResolveInput(Node, "B", "1.0f", EMaterialGraphPinType::Float);
				FResolved RAlpha = ResolveInput(Node, "Alpha", "0.5f", EMaterialGraphPinType::Float);

				const EMaterialGraphPinType OpType = MaxType(RA.Source.Type, RB.Source.Type);
				const FString A = ConvertExpr(RA.Source.Expr, RA.Source.Type, OpType, &Result.Errors);
				const FString B = ConvertExpr(RB.Source.Expr, RB.Source.Type, OpType, &Result.Errors);
				const FString Alpha = ConvertExpr(RAlpha.Source.Expr, RAlpha.Source.Type, EMaterialGraphPinType::Float, &Result.Errors);

				RhsExpr = "lerp(" + A + ", " + B + ", " + Alpha + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::TexCoord:
			{
				const int32 Idx = static_cast<int32>(Node.Value.X);
				if (Idx == 1)      RhsExpr = "Input.UV1";
				else if (Idx == 2) RhsExpr = "Input.UV2";
				else               RhsExpr = "Input.UV0";
				ResultType = EMaterialGraphPinType::Float2;
				break;
			}
			case EMaterialGraphNodeType::Panner:
			{
				const FString UV = InputExpr(Node, "UV", "Input.UV0", EMaterialGraphPinType::Float2, EMaterialGraphPinType::Float2);
				const FString Time = InputExpr(Node, "Time", "Input.Time", EMaterialGraphPinType::Float, EMaterialGraphPinType::Float);
				char Speed[80];
				std::snprintf(Speed, sizeof(Speed), "float2(%.6ff, %.6ff)", Node.Value.X, Node.Value.Y);
				RhsExpr = "(" + UV + " + " + FString(Speed) + " * " + Time + ")";
				ResultType = EMaterialGraphPinType::Float2;
				break;
			}
			case EMaterialGraphNodeType::Time:
				RhsExpr = "Input.Time";
				ResultType = EMaterialGraphPinType::Float;
				break;
			case EMaterialGraphNodeType::VertexColor:
				RhsExpr = "Input.VertexColor";
				ResultType = EMaterialGraphPinType::Float4;
				break;
			case EMaterialGraphNodeType::ParticleColor:
				RhsExpr = "Input.ParticleColor";
				ResultType = EMaterialGraphPinType::Float4;
				break;
			case EMaterialGraphNodeType::Append:
			{
				const FString A = InputExpr(Node, "A", "0.0f", EMaterialGraphPinType::Float, EMaterialGraphPinType::Float);
				const FString B = InputExpr(Node, "B", "0.0f", EMaterialGraphPinType::Float, EMaterialGraphPinType::Float);
				RhsExpr = "float2(" + A + ", " + B + ")";
				ResultType = EMaterialGraphPinType::Float2;
				break;
			}
			case EMaterialGraphNodeType::ComponentMask:
			{
				const FString V = InputExpr(Node, "Value", "float4(0, 0, 0, 0)", EMaterialGraphPinType::Float4, EMaterialGraphPinType::Float4);
				const FString Mask = NormalizeMask(Node.Mask);
				RhsExpr = "(" + V + ")." + Mask;
				ResultType = MaskType(Mask);
				break;
			}
			case EMaterialGraphNodeType::ConstantBiasScale:
			{
				FResolved RV = ResolveInput(Node, "Value", "0.0f", EMaterialGraphPinType::Float);
				const EMaterialGraphPinType OpType = RV.Source.Type;
				const FString V = ConvertExpr(RV.Source.Expr, RV.Source.Type, OpType, &Result.Errors);
				char Buf[80];
				std::snprintf(Buf, sizeof(Buf), "((%s + %.6ff) * %.6ff)", V.c_str(), Node.Value.X, Node.Value.Y);
				RhsExpr = Buf;
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::Distance:
			{
				FResolved RA = ResolveInput(Node, "A", "float3(0, 0, 0)", EMaterialGraphPinType::Float3);
				FResolved RB = ResolveInput(Node, "B", "float3(0, 0, 0)", EMaterialGraphPinType::Float3);
				const EMaterialGraphPinType OpType = MaxType(RA.Source.Type, RB.Source.Type);
				const FString A = ConvertExpr(RA.Source.Expr, RA.Source.Type, OpType, &Result.Errors);
				const FString B = ConvertExpr(RB.Source.Expr, RB.Source.Type, OpType, &Result.Errors);
				RhsExpr = "distance(" + A + ", " + B + ")";
				ResultType = EMaterialGraphPinType::Float;
				break;
			}
			case EMaterialGraphNodeType::Normalize:
			{
				FResolved RV = ResolveInput(Node, "Value", "float3(0, 0, 1)", EMaterialGraphPinType::Float3);
				const EMaterialGraphPinType OpType = RV.Source.Type;
				const FString V = ConvertExpr(RV.Source.Expr, RV.Source.Type, OpType, &Result.Errors);
				RhsExpr = "normalize(" + V + ")";
				ResultType = OpType;
				break;
			}
			case EMaterialGraphNodeType::Dot:
			{
				FResolved RA = ResolveInput(Node, "A", "float3(0, 0, 0)", EMaterialGraphPinType::Float3);
				FResolved RB = ResolveInput(Node, "B", "float3(0, 0, 0)", EMaterialGraphPinType::Float3);
				const EMaterialGraphPinType OpType = MaxType(RA.Source.Type, RB.Source.Type);
				const FString A = ConvertExpr(RA.Source.Expr, RA.Source.Type, OpType, &Result.Errors);
				const FString B = ConvertExpr(RB.Source.Expr, RB.Source.Type, OpType, &Result.Errors);
				RhsExpr = "dot(" + A + ", " + B + ")";
				ResultType = EMaterialGraphPinType::Float;
				break;
			}
			case EMaterialGraphNodeType::Cross:
			{
				const FString A = InputExpr(Node, "A", "float3(1, 0, 0)", EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3);
				const FString B = InputExpr(Node, "B", "float3(0, 1, 0)", EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3);
				RhsExpr = "cross(" + A + ", " + B + ")";
				ResultType = EMaterialGraphPinType::Float3;
				break;
			}
			case EMaterialGraphNodeType::ParticleSubUV:
			{
				// Cols × Rows 아틀라스에서 SubImageIndex(∈[0,1))를 정수 프레임으로 변환,
				// row/col을 계산해서 셀 내 UV 좌표(Input.UV0)를 합성.
				const int32 Cols = std::max(1, static_cast<int32>(Node.Value.X));
				const int32 Rows = std::max(1, static_cast<int32>(Node.Value.Y));
				const int32 Total = Cols * Rows;
				char Buf[256];
				std::snprintf(Buf, sizeof(Buf),
					"((float2(fmod(floor(Input.SubImageIndex * %d), %d), "
					"floor(Input.SubImageIndex * %d / %d)) + Input.UV0) "
					"* float2(1.0f/%d, 1.0f/%d))",
					Total, Cols, Total, Cols, Cols, Rows);
				RhsExpr = Buf;
				ResultType = EMaterialGraphPinType::Float2;
				break;
			}
			case EMaterialGraphNodeType::DynamicParameter:
			{
				// 풀 Float4 값. 출력 pin 이름(Param1~4/RGBA)에 따라 swizzle helper가 분배.
				RhsExpr = "Input.DynamicParam";
				ResultType = EMaterialGraphPinType::Float4;
				break;
			}
			case EMaterialGraphNodeType::Output:
			default:
				break;
			}

			const FString VarName = "n_" + std::to_string(Node.NodeId);
			BodyLines.push_back("    " + HlslType(ResultType) + " " + VarName + " = " + RhsExpr + ";");

			FEmittedNode Out;
			Out.Expr = VarName;
			Out.Type = ResultType;
			Emitted[Node.NodeId] = Out;
			return Out;
		}

		// 입력 pin의 expression을 TargetType으로 변환해 반환. 링크 없으면 DefaultExpr 사용.
		FString InputExpr(
			const FMaterialGraphNode& Node,
			const char* PinName,
			const FString& DefaultExpr,
			EMaterialGraphPinType DefaultType,
			EMaterialGraphPinType TargetType)
		{
			const FMaterialGraphPin* InputPin = FindPinByName(Node, PinName, EMaterialGraphPinKind::Input);
			if (!InputPin) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

			const FMaterialGraphLink* Link = FindInputLink(Graph, InputPin->PinId);
			if (!Link) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

			const FMaterialGraphPin* FromPin = Graph.FindPin(Link->FromPinId);
			const FMaterialGraphNode* FromNode = FromPin ? Graph.FindNode(FromPin->OwningNodeId) : nullptr;
			if (!FromPin || !FromNode) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

			FEmittedNode Source = Emit(*FromNode);
			// upstream 노드의 어느 출력 pin에서 나왔는지에 따라 swizzle 적용 (R/G/B/A/RGB).
			ApplyOutputPinSwizzle(FromPin->DisplayName.ToString(), Source.Expr, Source.Type);
			return ConvertExpr(Source.Expr, Source.Type, TargetType, &Result.Errors);
		}

		// 변환 없이 upstream의 실제 emit 결과(Expr, Type)와 연결 여부를 반환.
		// 수학 노드가 OpType을 동적으로 결정할 때 사용.
		struct FResolved
		{
			FEmittedNode Source;
			bool         bConnected = false;
		};

		FResolved ResolveInput(
			const FMaterialGraphNode& Node,
			const char* PinName,
			const FString& DefaultExpr,
			EMaterialGraphPinType DefaultType)
		{
			FResolved Out;
			Out.Source.Expr = DefaultExpr;
			Out.Source.Type = DefaultType;

			const FMaterialGraphPin* InputPin = FindPinByName(Node, PinName, EMaterialGraphPinKind::Input);
			if (!InputPin) return Out;

			const FMaterialGraphLink* Link = FindInputLink(Graph, InputPin->PinId);
			if (!Link) return Out;

			const FMaterialGraphPin* FromPin = Graph.FindPin(Link->FromPinId);
			const FMaterialGraphNode* FromNode = FromPin ? Graph.FindNode(FromPin->OwningNodeId) : nullptr;
			if (!FromPin || !FromNode) return Out;

			Out.Source = Emit(*FromNode);
			ApplyOutputPinSwizzle(FromPin->DisplayName.ToString(), Out.Source.Expr, Out.Source.Type);
			Out.bConnected = true;
			return Out;
		}

		// TextureSample의 Texture pin이 비어있으면 슬롯 기반 fallback.
		FString TextureExpressionForSampleNode(const FMaterialGraphNode& SampleNode)
		{
			const FMaterialGraphPin* TexturePin = FindPinByName(SampleNode, "Texture", EMaterialGraphPinKind::Input);
			if (TexturePin)
			{
				if (const FMaterialGraphLink* Link = FindInputLink(Graph, TexturePin->PinId))
				{
					if (const FMaterialGraphPin* FromPin = Graph.FindPin(Link->FromPinId))
					{
						if (const FMaterialGraphNode* FromNode = Graph.FindNode(FromPin->OwningNodeId))
						{
							if (FromNode->Type == EMaterialGraphNodeType::TextureObject)
							{
								return RegisterTexture(*FromNode);
							}
						}
					}
				}
			}

			FMaterialGraphNode Fallback;
			Fallback.NodeId = 0;
			Fallback.ParameterName = "Diffuse";
			Fallback.TextureSlot = EMaterialTextureSlot::Diffuse;
			return RegisterTexture(Fallback);
		}

		FString RegisterParameter(const FString& Name, EMaterialGraphPinType Type, const FVector4& Value)
		{
			const FString SafeName = SanitizeIdentifier(Name);
			const FString HlslName = "Param_" + SafeName;
			if (Params.find(Name) == Params.end())
			{
				FParamDecl Decl;
				Decl.Name = Name;
				Decl.HlslName = HlslName;
				Decl.Type = Type;
				Decl.Value = Value;
				Params.emplace(Name, Decl);

				FMaterialCompiledParameter Compiled;
				Compiled.Type = Type;
				Compiled.Value = Value;
				Result.Parameters[Name] = Compiled;
			}
			return HlslName;
		}

		// 동일 슬롯이면 하나의 Texture2D만 선언. register 충돌 방지.
		FString RegisterTexture(const FMaterialGraphNode& Node)
		{
			const EMaterialTextureSlot Slot = Node.TextureSlot;
			auto It = TexturesBySlot.find(Slot);
			if (It != TexturesBySlot.end())
			{
				return It->second.HlslName;
			}

			const FString Name = Node.ParameterName.empty() ? FString(ToString(Slot)) : Node.ParameterName;
			const FString HlslName = "Tex_" + SanitizeIdentifier(Name);

			FTextureDecl Decl;
			Decl.Name = Name;
			Decl.HlslName = HlslName;
			Decl.Path = Node.TexturePath;
			Decl.Slot = Slot;
			TexturesBySlot.emplace(Slot, Decl);

			FMaterialCompiledTexture Compiled;
			Compiled.Path = Node.TexturePath;
			Compiled.Slot = Slot;
			Result.Textures[FString(ToString(Slot)) + "Texture"] = Compiled;

			return HlslName;
		}

		FString BuildTextureDeclarations() const
		{
			if (TexturesBySlot.empty()) return FString();

			std::stringstream SS;
			for (const auto& Pair : TexturesBySlot)
			{
				const FTextureDecl& Decl = Pair.second;
				SS << "Texture2D " << Decl.HlslName
					<< " : register(t" << static_cast<int>(Decl.Slot) << ");\n";
			}
			SS << "\n";
			return SS.str();
		}

		FString BuildCBuffer() const
		{
			if (Params.empty()) return FString();

			std::stringstream SS;
			SS << "cbuffer PerMaterial : register(b" << PerMaterialSlot << ")\n";
			SS << "{\n";
			uint32 PadIndex = 0;
			for (const auto& Pair : Params)
			{
				const FParamDecl& Decl = Pair.second;
				switch (Decl.Type)
				{
				case EMaterialGraphPinType::Float:
					SS << "    float " << Decl.HlslName << ";\n";
					SS << "    float3 _Pad" << PadIndex++ << ";\n";
					break;
				case EMaterialGraphPinType::Float2:
				case EMaterialGraphPinType::UV:
					SS << "    float2 " << Decl.HlslName << ";\n";
					SS << "    float2 _Pad" << PadIndex++ << ";\n";
					break;
				case EMaterialGraphPinType::Float3:
				case EMaterialGraphPinType::Color:
					SS << "    float3 " << Decl.HlslName << ";\n";
					SS << "    float _Pad" << PadIndex++ << ";\n";
					break;
				case EMaterialGraphPinType::Float4:
				default:
					SS << "    float4 " << Decl.HlslName << ";\n";
					break;
				}
			}
			SS << "};\n\n";
			return SS.str();
		}

		FString BuildBody() const
		{
			std::stringstream SS;
			for (const FString& Line : BodyLines) SS << Line << "\n";
			return SS.str();
		}

	private:
		const FMaterialGraph&                Graph;
		FMaterialCompileResult&              Result;
		uint32                               PerMaterialSlot = ECBSlot::PerShader0;
		TMap<uint32, FEmittedNode>           Emitted;
		TArray<FString>                      BodyLines;
		TMap<FString, FParamDecl>            Params;
		// Slot 기반 dedupe — enum 키라 std::map 사용 (TMap이 enum 키 미지원일 수 있어 안전).
		std::map<EMaterialTextureSlot, FTextureDecl> TexturesBySlot;
	};

	FString OutputInputExpr(
		FHlslBuildContext& Context,
		const FMaterialGraph& Graph,
		const FMaterialGraphNode& Output,
		const char* PinName,
		const FString& DefaultExpr,
		EMaterialGraphPinType DefaultType,
		EMaterialGraphPinType TargetType,
		FMaterialCompileResult& Result)
	{
		const FMaterialGraphPin* InputPin = FindPinByName(Output, PinName, EMaterialGraphPinKind::Input);
		if (!InputPin) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

		const FMaterialGraphLink* Link = FindInputLink(Graph, InputPin->PinId);
		if (!Link) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

		const FMaterialGraphPin* FromPin = Graph.FindPin(Link->FromPinId);
		const FMaterialGraphNode* FromNode = FromPin ? Graph.FindNode(FromPin->OwningNodeId) : nullptr;
		if (!FromPin || !FromNode) return ConvertExpr(DefaultExpr, DefaultType, TargetType, &Result.Errors);

		FEmittedNode Source = Context.Emit(*FromNode);
		return ConvertExpr(Source.Expr, Source.Type, TargetType, &Result.Errors);
	}

	FString BuildEvaluateMaterial(const FMaterialGraph& Graph, FHlslBuildContext& Context, EMaterialDomain Domain, FMaterialCompileResult& Result)
	{
		const FMaterialGraphNode* Output = Graph.FindFirstNodeOfType(EMaterialGraphNodeType::Output);
		std::stringstream SS;
		SS << "FMaterialResult EvaluateMaterial(FMaterialPixelInput Input)\n";
		SS << "{\n";

		if (!Output)
		{
			SS << "    FMaterialResult Result;\n";
			SS << "    Result.Color = float3(1, 1, 1);\n";
			SS << "    Result.Emissive = float3(0, 0, 0);\n";
			SS << "    Result.Opacity = 1.0f;\n";
			SS << "    Result.UVOffset = float2(0, 0);\n";
			SS << "    return Result;\n";
			SS << "}\n\n";
			return SS.str();
		}

		FString ColorExpr, NormalExpr, RoughExpr, MetalExpr, EmissiveExpr, OpacityExpr, UVOffsetExpr;
		if (Domain == EMaterialDomain::Surface || Domain == EMaterialDomain::Decal)
		{
			ColorExpr    = OutputInputExpr(Context, Graph, *Output, "BaseColor", "float3(1, 1, 1)", EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3, Result);
			NormalExpr   = OutputInputExpr(Context, Graph, *Output, "Normal",    "float3(0, 0, 1)", EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3, Result);
			RoughExpr    = OutputInputExpr(Context, Graph, *Output, "Roughness", "0.5f",            EMaterialGraphPinType::Float,  EMaterialGraphPinType::Float,  Result);
			MetalExpr    = OutputInputExpr(Context, Graph, *Output, "Metallic",  "0.0f",            EMaterialGraphPinType::Float,  EMaterialGraphPinType::Float,  Result);
			EmissiveExpr = OutputInputExpr(Context, Graph, *Output, "Emissive",  "float3(0, 0, 0)", EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3, Result);
			OpacityExpr  = OutputInputExpr(Context, Graph, *Output, "Opacity",   "1.0f",            EMaterialGraphPinType::Float,  EMaterialGraphPinType::Float,  Result);
		}
		else
		{
			// 파티클은 PS에서 Color + Emissive를 더하므로, Color 미연결 시 (1,1,1)이면 흰색이 깔리는 버그.
			// 둘 다 0이 자연스러움 — 사용자가 둘 중 한쪽에만 연결해도 의도대로 결과가 나옴.
			ColorExpr    = OutputInputExpr(Context, Graph, *Output, "Color",    "float3(0, 0, 0)",  EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3, Result);
			EmissiveExpr = OutputInputExpr(Context, Graph, *Output, "Emissive", "float3(0, 0, 0)",  EMaterialGraphPinType::Float3, EMaterialGraphPinType::Float3, Result);
			OpacityExpr  = OutputInputExpr(Context, Graph, *Output, "Opacity",  "1.0f",             EMaterialGraphPinType::Float,  EMaterialGraphPinType::Float,  Result);
			UVOffsetExpr = OutputInputExpr(Context, Graph, *Output, "UVOffset", "float2(0, 0)",     EMaterialGraphPinType::Float2, EMaterialGraphPinType::Float2, Result);
		}

		// 모든 노드 로컬 선언을 먼저 흘려보내고, 마지막에 Result로 모은다.
		SS << Context.BuildBody();
		SS << "    FMaterialResult Result;\n";
		if (Domain == EMaterialDomain::Surface || Domain == EMaterialDomain::Decal)
		{
			SS << "    Result.BaseColor = " << ColorExpr << ";\n";
			SS << "    Result.Normal = "    << NormalExpr << ";\n";
			SS << "    Result.Roughness = " << RoughExpr << ";\n";
			SS << "    Result.Metallic = "  << MetalExpr << ";\n";
			SS << "    Result.Emissive = "  << EmissiveExpr << ";\n";
			SS << "    Result.Opacity = "   << OpacityExpr << ";\n";
		}
		else
		{
			SS << "    Result.Color = "    << ColorExpr << ";\n";
			SS << "    Result.Emissive = " << EmissiveExpr << ";\n";
			SS << "    Result.Opacity = "  << OpacityExpr << ";\n";
			SS << "    Result.UVOffset = " << UVOffsetExpr << ";\n";
		}
		SS << "    return Result;\n";
		SS << "}\n\n";
		return SS.str();
	}

	FString BuildCommonHeader(EMaterialDomain Domain, bool bReceiveLighting = false)
	{
		std::stringstream SS;
		SS << "#include \"Common/ConstantBuffers.hlsli\"\n";
		SS << "#include \"Common/VertexLayouts.hlsli\"\n";
		SS << "#include \"Common/Functions.hlsli\"\n";
		SS << "#include \"Common/SystemSamplers.hlsli\"\n";
		// AlphaBlend 도메인에서는 per-pixel fog 적용
		if (Domain == EMaterialDomain::ParticleSprite || Domain == EMaterialDomain::ParticleMesh)
		{
			SS << "#define USE_FOG 1\n";
			SS << "#include \"Common/Fog.hlsli\"\n";
		}
		if (Domain == EMaterialDomain::ParticleMesh && bReceiveLighting)
		{
			SS << "#include \"Common/ForwardLightData.hlsli\"\n";
		}
		SS << "\n";
		SS << "struct FMaterialPixelInput\n";
		SS << "{\n";
		SS << "    float2 UV0;\n";
		SS << "    float2 UV1;\n";
		SS << "    float2 UV2;\n";
		SS << "    float4 ParticleColor;\n";
		SS << "    float4 VertexColor;\n";
		SS << "    float  Time;\n";
		SS << "    float  SubImageIndex;\n";
		SS << "    float4 DynamicParam;\n";
		SS << "};\n\n";
		SS << "struct FMaterialResult\n";
		SS << "{\n";
		if (Domain == EMaterialDomain::Surface || Domain == EMaterialDomain::Decal)
		{
			SS << "    float3 BaseColor;\n";
			SS << "    float3 Normal;\n";
			SS << "    float Roughness;\n";
			SS << "    float Metallic;\n";
			SS << "    float3 Emissive;\n";
			SS << "    float Opacity;\n";
		}
		else
		{
			SS << "    float3 Color;\n";
			SS << "    float3 Emissive;\n";
			SS << "    float Opacity;\n";
			SS << "    float2 UVOffset;\n";
		}
		SS << "};\n\n";
		return SS.str();
	}

	FString BuildParticleSpriteMain()
	{
		return R"(
struct PS_Input_MaterialParticle
{
    float4 position       : SV_POSITION;
    float2 texcoord       : TEXCOORD0;
    float4 color          : COLOR;
    float  subImageIndex  : TEXCOORD1;
    float4 dynamicParam   : TEXCOORD2;
    float3 worldPos       : TEXCOORD3;
};

PS_Input_MaterialParticle VS(VS_Input_ParticleQuad quad, VS_Input_ParticleInstance inst)
{
    float sinR = sin(inst.rotation);
    float cosR = cos(inst.rotation);

    float2 rotUV = float2(
        quad.cornerUV.x * cosR - quad.cornerUV.y * sinR,
        quad.cornerUV.x * sinR + quad.cornerUV.y * cosR
    );

    float3 worldPos = inst.position
                    + FrameCameraRight * rotUV.x * inst.size
                    + FrameCameraUp * rotUV.y * inst.size;

    PS_Input_MaterialParticle output;
    output.position       = mul(float4(worldPos, 1.0f), mul(View, Projection));
    output.texcoord       = quad.cornerUV + 0.5f;
    output.color          = inst.color;
    output.subImageIndex  = inst.subImageIndex;
    output.dynamicParam   = inst.dynamicParam;
    output.worldPos       = worldPos;
    return output;
}

float4 PS(PS_Input_MaterialParticle input) : SV_TARGET
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0           = input.texcoord;
    MaterialInput.UV1           = float2(0, 0);
    MaterialInput.UV2           = float2(0, 0);
    MaterialInput.ParticleColor = input.color;
    MaterialInput.VertexColor   = input.color;
    MaterialInput.Time          = Time;
    MaterialInput.SubImageIndex = input.subImageIndex;
    MaterialInput.DynamicParam  = input.dynamicParam;

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    float4 FinalColor = float4(Result.Color + Result.Emissive, Result.Opacity);
    clip(FinalColor.a - 0.01f);
    return ApplyFogTranslucent(FinalColor, input.worldPos, CameraWorldPos);
}
)";
	}

	FString BuildParticleMeshMain(bool bReceiveLighting = false)
	{
		std::stringstream SS;
		SS << R"(
struct PS_Input_MaterialMeshParticle
{
    float4 position       : SV_POSITION;
    float3 normal         : NORMAL;
    float2 texcoord       : TEXCOORD0;
    float4 color          : COLOR;
    float  subImageIndex  : TEXCOORD1;
    float4 dynamicParam   : TEXCOORD2;
    float3 worldPos       : TEXCOORD3;
};

PS_Input_MaterialMeshParticle VS(VS_Input_PNCT vert, VS_Input_MeshParticleInstance inst)
{
    float4 worldPos = mul(float4(vert.position, 1.0f), inst.transform);
    // 비균일 스케일에서 노말 왜곡 방지: 역전치 행렬 사용
    float3x3 M = (float3x3)inst.transform;
    float3x3 invTransM = transpose(float3x3(
        cross(M[1], M[2]),
        cross(M[2], M[0]),
        cross(M[0], M[1])
    ));
    float3 worldNormal = mul(vert.normal, invTransM);

    PS_Input_MaterialMeshParticle output;
    output.position       = mul(worldPos, mul(View, Projection));
    output.normal         = normalize(worldNormal);
    output.texcoord       = vert.texcoord;
    output.color          = vert.color * inst.color;
    output.subImageIndex  = inst.subImageIndex;
    output.dynamicParam   = inst.dynamicParam;
    output.worldPos       = worldPos.xyz / worldPos.w;
    return output;
}

float4 PS(PS_Input_MaterialMeshParticle input) : SV_TARGET
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0           = input.texcoord;
    MaterialInput.UV1           = float2(0, 0);
    MaterialInput.UV2           = float2(0, 0);
    MaterialInput.ParticleColor = input.color;
    MaterialInput.VertexColor   = input.color;
    MaterialInput.Time          = Time;
    MaterialInput.SubImageIndex = input.subImageIndex;
    MaterialInput.DynamicParam  = input.dynamicParam;

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    float3 BaseColor = Result.Color;
)";

		if (bReceiveLighting)
		{
			SS << R"(
    float3 N = normalize(input.normal);
    float3 ambient = AmbientLight.Color.rgb * AmbientLight.Intensity;
    float NdotL = saturate(dot(N, -DirectionalLight.Direction));
    float3 directional = DirectionalLight.Color.rgb * DirectionalLight.Intensity * NdotL;
    float3 lighting = saturate(ambient + directional);
    BaseColor = BaseColor * lighting;
)";
		}

		SS << R"(
    float4 FinalColor = float4(BaseColor + Result.Emissive, Result.Opacity);
    clip(FinalColor.a - 0.01f);
    return ApplyFogTranslucent(FinalColor, input.worldPos, CameraWorldPos);
}
)";
		return SS.str();
	}

	FString BuildSurfaceMain()
	{
		return R"(
struct MaterialSurfaceVSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

MaterialSurfaceVSOutput VS(VS_Input_PNCTT input)
{
    MaterialSurfaceVSOutput output;
    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    output.position = mul(mul(worldPos, View), Projection);
    output.normal = normalize(mul(input.normal, (float3x3)NormalMatrix));
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

struct MaterialSurfacePSOutput
{
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 Culling : SV_TARGET2;
};

MaterialSurfacePSOutput PS(MaterialSurfaceVSOutput input)
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0           = input.texcoord;
    MaterialInput.UV1           = float2(0, 0);
    MaterialInput.UV2           = float2(0, 0);
    MaterialInput.ParticleColor = float4(1, 1, 1, 1);
    MaterialInput.VertexColor   = input.color;
    MaterialInput.Time          = Time;
    MaterialInput.SubImageIndex = 0.0f;
    MaterialInput.DynamicParam  = float4(0, 0, 0, 0);

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    MaterialSurfacePSOutput Output;
    Output.Color = float4(Result.BaseColor + Result.Emissive, Result.Opacity);
    Output.Normal = float4(normalize(input.normal), 1.0f);
    Output.Culling = float4(0, 0, 0, 0);
    return Output;
}
)";
	}

	FString BuildPostProcessMain()
	{
		return R"(
PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    FMaterialPixelInput MaterialInput;
    MaterialInput.UV0           = input.uv;
    MaterialInput.UV1           = float2(0, 0);
    MaterialInput.UV2           = float2(0, 0);
    MaterialInput.ParticleColor = float4(1, 1, 1, 1);
    MaterialInput.VertexColor   = float4(1, 1, 1, 1);
    MaterialInput.Time          = Time;
    MaterialInput.SubImageIndex = 0.0f;
    MaterialInput.DynamicParam  = float4(0, 0, 0, 0);

    FMaterialResult Result = EvaluateMaterial(MaterialInput);
    return float4(Result.Color + Result.Emissive, Result.Opacity);
}
)";
	}
}

bool FMaterialHlslGenerator::Generate(const FMaterialGraph& Graph, const FMaterialCompileOptions& Options, FMaterialCompileResult& OutResult)
{
	FString Guid = Options.MaterialGuid.empty() ? "Material" : SanitizeIdentifier(Options.MaterialGuid);
	OutResult.GeneratedShaderPath = "Shaders/Generated/Materials/" + Guid + "_" + ToString(Options.Domain) + ".hlsl";

	// ParticleSprite는 ParticleFrameCB가 b2를 점유하므로 PerMaterial은 b3로 밀어야 충돌이 없음.
	const uint32 PerMaterialSlot = (Options.Domain == EMaterialDomain::ParticleSprite)
		? ECBSlot::PerShader1   // b3
		: ECBSlot::PerShader0;  // b2

	FHlslBuildContext Context(Graph, OutResult, PerMaterialSlot);
	const FString EvaluateMaterial = BuildEvaluateMaterial(Graph, Context, Options.Domain, OutResult);

	if (!OutResult.Errors.empty())
	{
		return false;
	}

	std::stringstream SS;
	SS << "// Generated from " << Options.MaterialPath << "\n";
	SS << "// Domain: " << ToString(Options.Domain) << "\n\n";
	SS << BuildCommonHeader(Options.Domain, Options.bReceiveLighting);
	SS << Context.BuildTextureDeclarations();
	SS << Context.BuildCBuffer();
	SS << EvaluateMaterial;

	switch (Options.Domain)
	{
	case EMaterialDomain::ParticleSprite:
		SS << BuildParticleSpriteMain();
		break;
	case EMaterialDomain::ParticleMesh:
		SS << BuildParticleMeshMain(Options.bReceiveLighting);
		break;
	case EMaterialDomain::PostProcess:
		SS << BuildPostProcessMain();
		break;
	case EMaterialDomain::Surface:
	case EMaterialDomain::Decal:
	default:
		SS << BuildSurfaceMain();
		break;
	}

	OutResult.GeneratedHlsl = SS.str();
	return true;
}
