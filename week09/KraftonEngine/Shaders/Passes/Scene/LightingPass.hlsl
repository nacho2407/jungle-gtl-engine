#include "../../ViewModes/UberLit.hlsl"

// Thin wrapper kept for pass-local organization.
// The renderer's lighting stage compiles ViewModes/UberLit.hlsl directly
// and uses VS_Fullscreen / PS_UberLit as the unified entry points.
