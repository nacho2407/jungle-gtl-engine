#include "Materials/Graph/MaterialGraphCompiler.h"

namespace
{
	const FMaterialGraphNode* FindNodeByInputPin(const FMaterialGraph& Graph, uint32 InputPinId)
	{
		for (const FMaterialGraphLink& Link : Graph.Links)
		{
			if (Link.ToPinId != InputPinId) continue;
			const FMaterialGraphPin* FromPin = Graph.FindPin(Link.FromPinId);
			return FromPin ? Graph.FindNode(FromPin->OwningNodeId) : nullptr;
		}
		return nullptr;
	}

	bool VisitNodeForCycles(const FMaterialGraph& Graph, const FMaterialGraphNode& Node, TSet<uint32>& Visiting, TSet<uint32>& Visited, TArray<FString>& Errors)
	{
		if (Visited.find(Node.NodeId) != Visited.end()) return true;
		if (Visiting.find(Node.NodeId) != Visiting.end())
		{
			Errors.push_back("Cycle detected in material graph.");
			return false;
		}

		Visiting.insert(Node.NodeId);
		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			if (Pin.Kind != EMaterialGraphPinKind::Input) continue;
			if (const FMaterialGraphNode* Upstream = FindNodeByInputPin(Graph, Pin.PinId))
			{
				if (!VisitNodeForCycles(Graph, *Upstream, Visiting, Visited, Errors))
				{
					return false;
				}
			}
		}
		Visiting.erase(Node.NodeId);
		Visited.insert(Node.NodeId);
		return true;
	}
}

bool FMaterialGraphCompiler::Compile(const FMaterialGraph& Graph, const FMaterialCompileOptions& Options, FMaterialCompileResult& OutResult)
{
	OutResult = FMaterialCompileResult{};

	const FMaterialGraphNode* Output = nullptr;
	for (const FMaterialGraphNode& Node : Graph.Nodes)
	{
		if (Node.Type != EMaterialGraphNodeType::Output) continue;
		if (Output)
		{
			OutResult.Errors.push_back("Material graph has more than one Output node.");
			return false;
		}
		Output = &Node;
	}

	if (!Output)
	{
		OutResult.Errors.push_back("Material graph has no Output node.");
		return false;
	}

	TSet<uint32> Visiting;
	TSet<uint32> Visited;
	if (!VisitNodeForCycles(Graph, *Output, Visiting, Visited, OutResult.Errors))
	{
		return false;
	}

	return FMaterialHlslGenerator::Generate(Graph, Options, OutResult);
}
