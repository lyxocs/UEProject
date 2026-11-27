#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WarlockOnline.generated.h"

// A direct-address dedicated room. No third-party account or discovery service.
UCLASS()
class WARLOCKARENA_API UWarlockOnline : public UGameInstance
{
    GENERATED_BODY()
public:
    FString Address=TEXT("21.6.1.43:7777"),DisplayName=TEXT("Warlock"),Status;
    TSharedPtr<class SWidget> Panel;
    int LastTestCommand=0;
    FDelegateHandle NetworkFailureHandle,TravelFailureHandle;
    virtual void Init() override;
    virtual void Shutdown() override;
    UFUNCTION(BlueprintCallable,Exec) void JoinRoom(const FString& Endpoint,const FString& Name);
    UFUNCTION(BlueprintCallable,Exec) void LeaveRoom();
    void ShowConnectionPanel();
    void CloseConnectionPanel();
    void NetworkFailure(UWorld* World,class UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error);
    void TravelFailure(UWorld* World,ETravelFailure::Type Type,const FString& Error);
    static bool ValidEndpoint(const FString& Endpoint);
};
