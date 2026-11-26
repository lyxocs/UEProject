#include "WarlockPresentation.h"
#include "WarlockGame.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UWarlockPresentation::UWarlockPresentation()
{
    FWarlockArenaPalette P;
    P.Name=TEXT("余烬玄岩");P.Stone=FLinearColor(.85f,.93f,1.04f);Palettes.Add(P);
    P.Name=TEXT("月影银灰");P.Stone=FLinearColor(.70f,.86f,1.12f);P.LavaCore=FLinearColor(.47f,.16f,.035f);P.LavaBank=FLinearColor(.20f,.045f,.007f);P.Accent=FLinearColor(.32f,.52f,.65f);Palettes.Add(P);
    P.Name=TEXT("古铜苔石");P.Stone=FLinearColor(.78f,1.02f,.88f);P.LavaCore=FLinearColor(.54f,.18f,.025f);P.LavaBank=FLinearColor(.24f,.05f,.006f);P.Accent=FLinearColor(.42f,.59f,.40f);Palettes.Add(P);
    P.Name=TEXT("暮光紫岩");P.Stone=FLinearColor(.98f,.82f,1.07f);P.LavaCore=FLinearColor(.52f,.09f,.018f);P.LavaBank=FLinearColor(.22f,.025f,.006f);P.Accent=FLinearColor(.55f,.38f,.61f);Palettes.Add(P);
}
FWarlockArenaPalette UWarlockPresentation::Palette(int Index)
{
    const auto Settings=GetDefault<UWarlockPresentation>();
    return Settings->Palettes.IsEmpty()?FWarlockArenaPalette():Settings->Palettes[FMath::Abs(Index)%Settings->Palettes.Num()];
}
int UWarlockPresentation::PaletteForRound(int Round,const FString& Phase)
{
    const int Count=FMath::Max(1,GetDefault<UWarlockPresentation>()->Palettes.Num());
    int Upcoming=Phase==TEXT("SHOP")?Round+1:FMath::Max(1,Round);
    return FMath::Max(0,Upcoming-1)%Count;
}
void AWarlockArena::InitializePresentation()
{
    if(GetNetMode()==NM_DedicatedServer)return;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(auto Mesh:Meshes)for(int I=0;I<Mesh->GetNumMaterials();++I)
        {
            auto Mat=Mesh->GetMaterial(I);if(!Mat||!Mat->GetMaterial())continue;
            const FName Name=Mat->GetMaterial()->GetFName();
            if(Name!=TEXT("M_Stone")&&Name!=TEXT("M_Lava"))continue;
            auto MID=Mesh->CreateDynamicMaterialInstance(I);if(!MID)continue;
            if(Name==TEXT("M_Stone"))StoneMaterials.Add(MID);else LavaMaterials.Add(MID);
        }
    }
    auto Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/WarlockArena/Materials/M_ArenaBoundary.M_ArenaBoundary"));
    if(Plane&&Base)
    {
        BoundaryRing=NewObject<UStaticMeshComponent>(this);BoundaryRing->SetupAttachment(Floor);BoundaryRing->SetAbsolute(true,true,true);
        BoundaryRing->SetStaticMesh(Plane);BoundaryRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);BoundaryRing->SetCastShadow(false);BoundaryRing->RegisterComponent();
        BoundaryMaterial=UMaterialInstanceDynamic::Create(Base,this);BoundaryRing->SetMaterial(0,BoundaryMaterial);
    }
    UpdatePresentation(0);
}
void AWarlockArena::UpdatePresentation(float Dt)
{
    if(GetNetMode()==NM_DedicatedServer)return;
    auto Match=GetWorld()->GetGameState<AWarlockMatchState>();if(!Match)return;
    PaletteIndex=UWarlockPresentation::PaletteForRound(Match->Round,Match->Phase);auto P=UWarlockPresentation::Palette(PaletteIndex);
    float A=bPaletteInitialized?1-FMath::Exp(-FMath::Max(0.f,Dt)*GetDefault<UWarlockPresentation>()->PaletteBlendSpeed):1;
    PaletteStone=FMath::Lerp(PaletteStone,P.Stone,A);PaletteCore=FMath::Lerp(PaletteCore,P.LavaCore,A);PaletteBank=FMath::Lerp(PaletteBank,P.LavaBank,A);PaletteAccent=FMath::Lerp(PaletteAccent,P.Accent,A);bPaletteInitialized=true;
    for(auto M:StoneMaterials)M->SetVectorParameterValue(TEXT("ThemeTint"),PaletteStone);
    for(auto M:LavaMaterials){M->SetVectorParameterValue(TEXT("LavaCore"),PaletteCore);M->SetVectorParameterValue(TEXT("LavaBank"),PaletteBank);}
    if(BoundaryRing){BoundaryRing->SetWorldLocation(FVector(0,0,4));BoundaryRing->SetWorldScale3D(FVector(Match->ArenaRadius/50,Match->ArenaRadius/50,1));BoundaryMaterial->SetVectorParameterValue(TEXT("Tint"),PaletteAccent*.7f);}
}
