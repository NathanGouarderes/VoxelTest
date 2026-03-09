using UnrealBuildTool;

public class VoxelModule : ModuleRules
{
    public VoxelModule(ReadOnlyTargetRules Target) : base(Target)
    {
        // Modules dont tu as besoin
        PrivateDependencyModuleNames.AddRange(
            new string[] {} //Core, CoreUObject et Engine sont généralement requis pour travailler avec des Actor, UObject, etc.
        );

        // Si ton module doit exposer des API à l’extérieur :
        PublicDependencyModuleNames.AddRange(
            new string[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent" }
        );

        // Configuration supplémentaire si nécessaire
    }
}