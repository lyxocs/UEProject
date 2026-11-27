#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "AIController.h"
#include "InputActionValue.h"
#include "WarlockLearning.h"
#include "WarlockGame.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class AWarlockPawn;
class UEnhancedInputLocalPlayerSubsystem;
class USkeletalMeshComponent;
class UAnimSequence;

namespace WarlockRules
{
    constexpr float Step = .03f;
    constexpr float GroundStep = .1f;
    constexpr float BaseSpeed = 210.f;
    constexpr float Radius = 30.f;
    constexpr int Slots = 8;
    // G, F, D, R, T, E, C, Y. Original map units are preserved.
    WARLOCKARENA_API FString SkillName(int Slot);
    WARLOCKARENA_API float Cooldown(int Slot, int Level, bool Alternate);
    WARLOCKARENA_API int LearnCost(int Slot);
    WARLOCKARENA_API int UpgradeCost(int Slot, int Level);
    WARLOCKARENA_API int LevelCap(int Slot);
    WARLOCKARENA_API float Impulse(float Damage, float PreviousKnockback, float Multiplier = 1.f);
    WARLOCKARENA_API bool SweepCircle(FVector Start,FVector End,FVector Center,float Radius,float& Time);
}

UCLASS()
class WARLOCKARENA_API AWarlockState : public APlayerState
{
    GENERATED_BODY()
public:
    AWarlockState();
    UPROPERTY(Replicated, BlueprintReadOnly) int Gold = 20;
    UPROPERTY(Replicated, BlueprintReadOnly) int Points = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int Team = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<int32> Levels;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<bool> Modes;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<int32> Items;
    // One learned family per slot. C: 0 = Reflect, 1 = Haste (S007).
    UPROPERTY(Replicated, BlueprintReadOnly) int DefenseChoice = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int LifeMastery = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int RangeMastery = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int TimeMastery = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int StoneSlot = -1;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bHasStone = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bBot = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int Appearance = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int ColorIndex = 0;
    int Level(int Slot) const;
    int LearnedCount() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockMatchState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly) int Round = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) FString Phase = TEXT("SHOP");
    UPROPERTY(Replicated, BlueprintReadOnly) float Remaining = 40;
    UPROPERTY(Replicated, BlueprintReadOnly) float ArenaRadius = 1280;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bTeams = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int AIDifficulty = 3;
    UPROPERTY(Replicated, BlueprintReadOnly) int ParticipantCount = 4;
    UPROPERTY(Replicated, BlueprintReadOnly) FString Announcement = TEXT("Prepare your spells");
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockPawn : public APawn
{
    GENERATED_BODY()
public:
    AWarlockPawn();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Hat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Halo;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ShieldMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Avatar;
    UPROPERTY() TObjectPtr<UAnimSequence> IdleAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> AttackAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> SpellAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> SlamAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> DeathAnimation;
    UPROPERTY(Replicated, BlueprintReadOnly) float LocomotionSpeed = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 CastSerial = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) uint8 LastCastSlot = 255;
    UPROPERTY(Replicated, BlueprintReadOnly) int PendingCast = -1;
    UPROPERTY(Replicated, BlueprintReadOnly) float CastRemaining = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float CastDuration = 0;
    FVector CastTarget = FVector::ZeroVector;
    UPROPERTY(Replicated, BlueprintReadOnly) int BlinkSerial = 0;
    int AppliedAppearance = -1;
    int AppliedColorIndex = -1;
    int SeenBlinkSerial = 0;
    float BlinkArrivalAge = 1;
    FVector AvatarRestScale=FVector(.92f);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Crown;
    UPROPERTY() TArray<TObjectPtr<class UStaticMesh>> CrownMeshes;
    float DeathVisualAge = 0;
    float FacingHold = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100;
    UPROPERTY(Replicated, BlueprintReadOnly) float Knockback = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int ImpactSerial = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float ImpactPower = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) FVector ImpactDirection = FVector::ZeroVector;
    int SeenImpactSerial=0;
    float ImpactVisualAge=1;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bAlive = true;
    UPROPERTY(Replicated, BlueprintReadOnly) float ShieldRemaining = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<float> Cooldowns;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input") FVector MoveInput = FVector::ZeroVector;
    FVector AimPoint = FVector(600,0,0);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") FVector Momentum = FVector::ZeroVector;
    FVector ClickTarget = FVector::ZeroVector;
    TWeakObjectPtr<AWarlockPawn> LastAttacker;
    TMap<TWeakObjectPtr<AWarlockPawn>, float> Contributions;
    TWeakObjectPtr<AWarlockPawn> ChainTarget;
    TWeakObjectPtr<class AWarlockSpellFX> ChainVisual;
    FVector ChainAnchor = FVector::ZeroVector;
    float ChainTime = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float Protection = 0;
    float LavaProtection = 0;
    float BootsCooldown = 0;
    float ShotTimer = 0;
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly) float SecondBlink = 0;
    bool bPendingSecondBlink = false;
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly) float HasteRemaining = 0;
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly) float HastePool = 0;
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly) float HasteSpeed = 0;
    float ChainDamageTimer = 0;
    int ShotsLeft = 0;
    int ShotsTotal = 0;
    int SpraySign = 1;
    int ChainSteps = 0;
    FVector SprayTarget = FVector::ZeroVector;
    bool bClickMoving = false;
    bool bAppearanceReady = false;
    UPROPERTY() TObjectPtr<UMaterialInterface> WizardMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> AppearanceMaterials;
    UPROPERTY(EditAnywhere, Category="Appearance") TSoftObjectPtr<class UWarlockAppearanceSet> AppearanceData;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") float CameraDistance = 1700;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Camera") TWeakObjectPtr<AWarlockPawn> Spectating;
    AWarlockState* State() const;
    float MaxHealth() const;
    float MoveSpeed() const;
    float KnockbackScale() const;
    bool IsEnemy(const AWarlockPawn* Other) const;
    UFUNCTION(BlueprintCallable) void ResetForRound(FVector Position);
    UFUNCTION(BlueprintCallable, Category="Simulation") void StepSimulation(float Dt);
    UFUNCTION(BlueprintCallable, Category="Simulation") void StepGround();
    UFUNCTION(BlueprintCallable) bool Cast(int Slot);
    UFUNCTION(BlueprintCallable) void CancelCast();
    void ReleaseSpell(int Slot);
    UFUNCTION(BlueprintCallable) void Hit(AWarlockPawn* Attacker, float Damage, const FVector& Direction, float Push = 1, int SkillSlot = -1);
    void Die();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockProjectile : public AActor
{
    GENERATED_BODY()
public:
    AWarlockProjectile();
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<class UMaterialBillboardComponent>> FireParticles;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> FireParticleMaterials;
    struct FFireParticle {FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;float Age=10,Life=.3f,Size=1;};
    TArray<FFireParticle> FireParticleStates;
    FVector LastVisualPosition=FVector::ZeroVector;
    float VisualAge=0,EmissionTime=0;
    int NextParticle=0;
    void InitializeFireParticles();
    virtual void Tick(float Dt) override;
    UPROPERTY(Replicated) TObjectPtr<AWarlockPawn> Caster;
    FVector Velocity = FVector::ZeroVector;
    float Damage = 7;
    float Push = 1;
    float Life = 1;
    UPROPERTY(Replicated) float Radius = 25;
    UPROPERTY(Replicated) int Kind = 0;
    int Bounces = 0;
    TWeakObjectPtr<AWarlockPawn> LastHit;
    void Step(float Dt);
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockPulse : public AActor
{
    GENERATED_BODY()
public:
    AWarlockPulse();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(Replicated) float Size = 150;
    UPROPERTY(Replicated) int Kind = 0;
    float Age = 0;
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockArena : public AActor
{
    GENERATED_BODY()
public:
    AWarlockArena();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Floor;
    TArray<FVector> Pillars;
    UPROPERTY(Replicated) TArray<float> PillarHealth;
    UPROPERTY() TArray<TObjectPtr<class UMaterialBillboardComponent>> AmbientMotes;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> AmbientMaterials;
    float AmbientAge=0;
    TArray<TWeakObjectPtr<UStaticMeshComponent>> PillarMeshes;
    float LastRadius = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int PaletteIndex = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FLinearColor PaletteStone = FLinearColor::White;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> StoneMaterials;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> LavaMaterials;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BoundaryRing;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BoundaryMaterial;
    FLinearColor PaletteCore,PaletteBank,PaletteAccent;
    bool bPaletteInitialized=false;
    void InitializePresentation();
    void UpdatePresentation(float Dt);
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    void DamagePillar(int Index, float Damage, AWarlockPawn* Caster, int SkillSlot = -1);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class WARLOCKARENA_API AWarlockMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AWarlockMode();
    UPROPERTY() TArray<TObjectPtr<AWarlockPawn>> Fighters;
    UPROPERTY() TObjectPtr<AWarlockArena> Arena;
    float Accumulator = 0;
    float GroundAccumulator = 0;
    float ShrinkTimer = 0;
    float BotThinkTimer = 0;
    bool bFinished = false;
    bool bVisualPreview = false;
    bool bTraining = false,bTrainingStep = false,bTrainingInitialized = false;
    double TrainingClock=0,TrainingWallStart=0;
    int TrainingEpisodes=0,TrainingLimit=1000,TrainingSeed=7919,TrainingTier=5;
    bool bEvaluation=false;
    float TrainingEpisodeTime=0;
    int TrainingWins=0,TrainingLosses=0,TrainingDraws=0;
    FString TrainingOutput;
    void TrainingBatch();
    void ResetTrainingEpisode();
    void FinishTraining();
    struct FTrainingMeteor{TWeakObjectPtr<AWarlockPawn> Caster;FVector Target;int Level,Generation;float Remaining;};
    TArray<FTrainingMeteor> TrainingMeteors;
    void ImpactMeteor(TWeakObjectPtr<AWarlockPawn> Caster,FVector Target,int Level,int Generation);
    int WinnerScoreAtEnd = 0;
    int RoundGeneration = 0;
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void Logout(AController* Exiting) override;
    virtual void Tick(float Dt) override;
    AWarlockMatchState* Match() const;
    void EnsureRoster();
    bool AreHumansReady();
    void CheckReady();
    UFUNCTION(BlueprintCallable) void StartRound();
    void EndRound(int WinningTeam);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void RestartMatch(bool Teams);
    void CheckVictory();
    UFUNCTION(BlueprintCallable) bool Buy(AWarlockPawn* Pawn, int Product, FString& Reason);
    UFUNCTION(BlueprintCallable) int Price(AWarlockPawn* Pawn, int Product) const;
    UFUNCTION(BlueprintPure) int NextCost(AWarlockPawn* Pawn, int Product) const;
    UFUNCTION(BlueprintPure) FString PurchaseReason(AWarlockPawn* Pawn, int Product) const;
    UFUNCTION(BlueprintPure) FString ProductDetails(AWarlockPawn* Pawn, int Product) const;
    UFUNCTION(BlueprintPure) int SalePrice(AWarlockPawn* Pawn, int Product) const;
    UFUNCTION(BlueprintCallable) bool Sell(AWarlockPawn* Pawn, int Product, FString& Reason);
    UFUNCTION(BlueprintCallable) bool AssignStone(AWarlockPawn* Pawn, int Slot, FString& Reason);
    UFUNCTION(BlueprintCallable) bool SetSkillMode(AWarlockPawn* Pawn, int Slot, FString& Reason);
    void Pulse(FVector Position, float Size, int Kind);
    void Fire(AWarlockPawn* From, FVector Direction, float Damage, float Push, float Speed, float Life, int Kind = 0);
    UFUNCTION(BlueprintCallable) void SimulateProjectiles(float Dt);
    void Meteor(AWarlockPawn* From, FVector Target, int Level, bool Rolling);
};

UCLASS()
class WARLOCKARENA_API AWarlockAI : public AAIController
{
    GENERATED_BODY()
public:
    AWarlockAI();
    virtual void BeginPlay() override;
    UFUNCTION(BlueprintCallable) void Think();
    void Shop();
    FRandomStream Random;
    bool bDisablePolicy=false,bLearning=false;
    int DifficultyOverride=0;
    int ShopPlanOverride=-1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int PolicyDecisions=0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int LastPolicyAction=-1;
    FWarlockExperience Experience;
    float DecisionTime() const;
    FWarlockDifficulty Difficulty() const;
    bool ExecutePolicyAction(int Action);
    bool ChoosePolicyAction();
    void LearnTransition(bool Terminal=false,float Outcome=0);
    float LearningScore() const;
    float NextDecision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ReactionRemaining = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector ObservedTargetPosition = FVector::ZeroVector;
    TWeakObjectPtr<AWarlockPawn> ObservedTarget;
    int ObservedBlinkSerial = -1;
    FVector ObservedMomentum = FVector::ZeroVector;
    bool bObservedThreat = false;
    bool bHasObservation = false;
    float LastObservationTime = 0;
    FVector ObservedVelocity = FVector::ZeroVector;
    float StrafeTimer = 0;
    float StrafeSign = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int Tactic = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float TacticWait = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float FeintCooldown = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ComboCooldown = 0;
    int TacticCastSerial = 0;
    int TacticBlinkSerial = 0;
    FVector TacticTarget = FVector::ZeroVector;
    UFUNCTION(BlueprintCallable) void UpdateTactics(float Dt);
    UFUNCTION(BlueprintCallable) bool TryFeint();
    UFUNCTION(BlueprintCallable) bool TryBlinkSmite();
    UFUNCTION(BlueprintCallable) void ResetPerception();
};

UCLASS()
class WARLOCKARENA_API AWarlockController : public APlayerController
{
    GENERATED_BODY()
public:
    AWarlockController();
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<UInputAction>> Actions;
    UPROPERTY(BlueprintReadOnly) bool bShopOpen = false;
    UPROPERTY(BlueprintReadOnly) bool bHelpOpen = false;
    UPROPERTY(BlueprintReadOnly) int SelectedProduct = 3;
    // 0 solo auto; 1 keyboard only; 2 mouse only; 3 pad only; 4 keyboard+mouse only.
    UPROPERTY(BlueprintReadOnly) int LocalControl = 0;
    UPROPERTY(BlueprintReadOnly) int LocalPadIndex = 0;
    UPROPERTY(BlueprintReadOnly) int SelectedSpell = 0;
    UPROPERTY(BlueprintReadOnly) int AimingSlot = -1;
    UPROPERTY(BlueprintReadOnly) int ShopTopRow = 0;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AimGuide;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> AimGuideMaterial;
    int MouseHeldSlot=-1,AimFeedbackSlot=-1;
    float AimFeedbackTime=0;
    FVector ConfirmedAim=FVector::ZeroVector;
    static bool UsesTargetGuide(int Slot){return Slot==0||Slot==2||Slot==3||Slot==4||Slot==5||Slot==7;}
    UFUNCTION(BlueprintCallable) void StartTargeting(int Slot);
    UFUNCTION(BlueprintCallable) void ReleaseTargeting(int Slot);
    UFUNCTION(BlueprintCallable) void CancelTargeting();
    void UpdateTargetGuide(float Dt);
    void MousePrimaryReleased();
    UFUNCTION(BlueprintCallable) void ScrollShop(int Rows);
    void SelectShopProduct(int Product);
    FString Notice;
    float NoticeTime = 0;
    FVector LastAimDirection = FVector(1,0,0);
    float AimDistance = 650;
    bool bGamepadAiming = false;
    float InputSendTime = 0;
    FVector LocalMove = FVector::ZeroVector;
    FVector LocalAim = FVector::ZeroVector;
    int LastShopDirection = 0;
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;
    virtual void PlayerTick(float Dt) override;
    virtual bool InputKey(const struct FInputKeyEventArgs& Args) override;
    bool UsesMouse() const{return LocalControl==0||LocalControl==2||LocalControl==4;}
    bool UsesKeyboard() const{return LocalControl==0||LocalControl==1||LocalControl==4;}
    bool UsesGamepad() const{return LocalControl==0||LocalControl==3;}
    FString ControlLabel() const;
    void ApplyLocalRoles(int FirstRole,int SecondRole);
    void FlushLocalControls();
    void RefreshKeyboardMappings();
    UFUNCTION(BlueprintCallable, Exec) void OpenPausePage(int32 Page);
    UFUNCTION(BlueprintCallable, Exec) void ConfigureLocalPlayers(int32 Count);
    UFUNCTION(BlueprintCallable, Exec) void SetParticipantCount(int32 Count);
    UPROPERTY(BlueprintReadWrite) bool bPlayersMenuOpen=false;
    UPROPERTY(BlueprintReadWrite) bool bKeybindMenuOpen=false;
    UPROPERTY(BlueprintReadWrite) int32 SelectedBinding=0;
    UPROPERTY(BlueprintReadOnly) int32 WaitingBinding=-1;
    UFUNCTION(BlueprintCallable) void StartKeyCapture(int32 Index);
    UFUNCTION(BlueprintCallable) void ResetKeys();
    void Move(const FInputActionValue& Value);
    void Aim(const FInputActionValue& Value);
    void CastAction(const FInputActionValue& Value, int Slot);
    void ClickMove();
    UFUNCTION(BlueprintCallable, Exec) void ToggleShop();
    UFUNCTION(Exec) void PreviewCharacter(int Appearance=0);
    UFUNCTION(Exec) void CaptureScene(const FString& Filename, float Delay=5.f);
    UFUNCTION(Exec) void BeginTestRound();
    UFUNCTION(Exec) void PreviewSpell(int Slot=0);
    void ToggleHelp();
    void Zoom(const FInputActionValue& Value);
    void CycleAppearance();
    void CycleResolution();
    void CycleQuality();
    UPROPERTY(BlueprintReadWrite) bool bDisplaySettingsOpen=false;
    UPROPERTY(BlueprintReadWrite) int32 SelectedDisplayRow=0;
    UFUNCTION(BlueprintCallable, Exec) void EditDisplaySetting(int32 Row,int32 Direction=1);
    UFUNCTION(BlueprintCallable, Exec) void SetGraphicsPreset(int32 Preset);
    void ToggleDisplaySettings();
    void NavigateShop(int Direction);
    void ConfirmShop();
    UFUNCTION(BlueprintCallable) void AddLocalPlayer();
    UFUNCTION(BlueprintCallable) void AddMixedPlayer();
    UFUNCTION(BlueprintCallable, Exec) void AddKeyboardMousePlayer();
    UFUNCTION(BlueprintCallable, Exec) void TogglePause();
    UFUNCTION(BlueprintCallable, Exec) void ToggleReady();
    UFUNCTION(BlueprintCallable, Exec) void SetAIDifficulty(int Tier);
    UFUNCTION(BlueprintPure) static bool IsTrainedAIReady(){return FWarlockPolicy::Shared().bReady;}
    void StopAction();
    void MousePrimary();
    void MouseSecondary();
    void MouseWheel(const FInputActionValue& Value);
    void ToggleTeams();
    void ToggleSplitLayout();
    void ToggleMode();
    void SellSelected();
    void StoneSelected();
    void AlternativeSelected();
    void SetNotice(const FString& Text);
    AWarlockPawn* Fighter() const;
    UFUNCTION(BlueprintPure) UEnhancedInputLocalPlayerSubsystem* LocalInput() const;
    UFUNCTION(Server, Unreliable) void ServerInput(FVector_NetQuantize Direction, FVector_NetQuantize Target);
    UFUNCTION(Server, Reliable) void ServerClick(FVector_NetQuantize Target);
    UFUNCTION(Server, Reliable) void ServerCast(int Slot, FVector_NetQuantize Target);
    UFUNCTION(Server, Reliable) void ServerBuy(int Product);
    UFUNCTION(Server, Reliable) void ServerMode();
    UFUNCTION(Server, Reliable) void ServerShopMode(int Slot);
    UFUNCTION(Server, Reliable) void ServerSell(int Product);
    UFUNCTION(Server, Reliable) void ServerStone(int Slot);
    UFUNCTION(Server, Reliable) void ServerAppearance();
    UFUNCTION(Server, Reliable) void ServerRestart(bool Teams);
    UFUNCTION(Server, Reliable) void ServerStop();
    UFUNCTION(Server, Reliable) void ServerToggleMatchPause();
    UFUNCTION(Server, Reliable) void ServerReady();
    UFUNCTION(Server, Reliable) void ServerAIDifficulty(int Tier);
    UFUNCTION(Client, Reliable) void ClientNotice(const FString& Text);
};

UCLASS()
class WARLOCKARENA_API AWarlockHUD : public AHUD
{
    GENERATED_BODY()
public:
    AWarlockHUD();
    UPROPERTY() TObjectPtr<class UFont> InterfaceFont;
    struct FHealthDisplay {float Actual=1,Trail=1,Hold=0;};
    TMap<TWeakObjectPtr<AWarlockPawn>,FHealthDisplay> HealthDisplays;
    float SmoothedFrameSeconds=0;
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
};
