struct VSOutput
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
};

VSOutput main(uint VertexID : SV_VertexID)
{
	VSOutput Output;
	Output.UV = float2((VertexID << 1) & 2, VertexID & 2);
	Output.Position = float4(Output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return Output;
}
