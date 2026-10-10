#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/SaveGame.h"
#include "MetalPrototype.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class USoundWave;
class UAudioComponent;
class USoundWaveProcedural;
struct FKey;

UCLASS()
class METAL_API UMetalCheckpoint : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Hour = 0;
    UPROPERTY() float DoorHealth = 100.f;
    UPROPERTY() float Oxygen = 100.f;
    UPROPERTY() bool TopClosed = false;
    UPROPERTY() bool BottomClosed = false;
};

USTRUCT()
struct FMetalThreat
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AActor> Robot;
    UPROPERTY() int32 Stage = 1;
    UPROPERTY() float Timer = 20.f;
    UPROPERTY() float StrikeTimer = 0.f;
    UPROPERTY() float WaitTime = 0.f;
    UPROPERTY() int32 Lane = 0;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Limbs;
    UPROPERTY() TArray<FTransform> RestPose;
};

UCLASS()
class METAL_API UMetalJournal : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<int32> Knowledge = {0, 0, 0, 0};
    UPROPERTY() bool bMoonlightUnlocked = false;
};

UCLASS()
class METAL_API AMetalCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AMetalCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Torch;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Device;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Needle;
    AActor* FindInteraction() const;
private:
    float LastClick = -100.f;
    void Forward(float Value);
    void Right(float Value);
    void LookX(float Value);
    void LookY(float Value);
    void Interact();
    void Power();
    void Detector();
    void Flashlight();
    void Retry();
    void Pause();
    void Hide();
    void Journal();
    void NewGame();
};

UCLASS()
class METAL_API AMetalGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMetalGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    void Use(AActor* Item);
    void SetPower(bool bReset);
    void ToggleDetector();
    void RetryHour();
    void StartNewGame();
    void SetPaused();
    void SyncMenuInput();
    void ReturnToMenu();
    float DeathScreenTime = 0.f;
    bool bJournalOpen = false;
    bool bComputerOpen = false;
    uint64 ComputerInputFrame = 0;
    FString ComputerText;
    FString ComputerMessage;
    void TypeComputerKey(const FKey& Key);
    void CloseComputer();
    void ToggleJournal();
    void LearnMonster(int32 Index, int32 Level);
    UPROPERTY() TObjectPtr<UMetalJournal> JournalData;
    FString InteractionLabel(AActor* Item) const;
    FString Status() const;

    float Battery = 100.f;
    float Oxygen = 100.f;
    float DoorHealth = 100.f;
    float Seconds = 0.f;
    float Intermission = 10.f;
    float AcquireTime = 0.f;
    float NoticeTime = 0.f;
    int32 Hour = 0;
    int32 PowerLevel = 0;
    int32 Target = INDEX_NONE;
    bool bDocked = false;
    bool bDetector = false;
    bool bDead = false;
    bool bScreamer = false;
    float ScreamerTime = 0.f;
    float ScreamerBlackoutAt = 1.65f;
    bool bWon = false;
    bool bPaused = false;
    bool bWelcome = false;
    bool bCorPreview = false;
    bool bCarryingStool = false;
    bool bHiding = false;
    void ToggleHiding();
    void TryHide(AActor* Item);
    UPROPERTY() TObjectPtr<AActor> Stool;
    bool bTopClosed = false;
    bool bBottomClosed = false;
    FString DeathReason;
    FString Notice;
    UPROPERTY() TArray<FMetalThreat> Threats;

private:
    UPROPERTY() TObjectPtr<AActor> Dock;
    UPROPERTY() TObjectPtr<AActor> Computer;
    bool bPausedBeforeJournal = false;
    UPROPERTY() TObjectPtr<AActor> Vent;
    UPROPERTY() TObjectPtr<AActor> VentGrille;
    bool bVentOpen = false;
    UPROPERTY() TObjectPtr<AActor> StoolHome;
    UPROPERTY() TObjectPtr<AActor> StoolVent;
    UPROPERTY() TObjectPtr<AActor> Wardrobe;
    UPROPERTY() TObjectPtr<AActor> ClosetLeft;
    UPROPERTY() TObjectPtr<AActor> ClosetRight;
    UPROPERTY() TObjectPtr<AActor> ClosetRobot;
    bool bClosetOpen = false;
    float ClosetProgress = 0.f;
    int32 ClosetWarnStage = 0;
    void SetClosetOpen(bool Open);
    void UpdateClosetRobot(float Dt);
    UPROPERTY() TObjectPtr<AActor> VentRobot;
    int32 VentStage = 0;
    float VentTimer = 45.f;
    FVector HideReturn;
    void UpdateVentRobot(float Dt);
    void UpdateVentScare(float Dt);
    void ResetStool();
    UPROPERTY() TObjectPtr<AActor> Door;
    UPROPERTY() TObjectPtr<AActor> TopWindow;
    UPROPERTY() TObjectPtr<AActor> BottomWindow;
    UPROPERTY() TObjectPtr<AActor> DockDevice;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> DockNeedle;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> RoomLights;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DoorMaterial;
    UPROPERTY() TObjectPtr<UMetalCheckpoint> Checkpoint;
    UPROPERTY() TObjectPtr<USoundWave> WindowScream;
    UPROPERTY() TObjectPtr<UAudioComponent> ActiveScream;
    UPROPERTY() TObjectPtr<UAudioComponent> MenuMusic;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> MenuMusicWave;
    TArray<uint8> MenuMusicPCM;
    void UpdateMenuMusic();
    UPROPERTY() TObjectPtr<USceneComponent> CorHead;
    UPROPERTY() TObjectPtr<USceneComponent> CorArm;
    int32 Candidate = INDEX_NONE;
    float ElapsedReal = 0.f;
    bool bSmoke = false;
    bool bPreview = false;
    bool bCaptured = false;
    bool bSmokeFinished = false;
    bool bScarePreview = false;
    bool bCorScarePreview = false;
    int32 ScarePreviewFrame = 0;
    int32 ScareLane = 1;
    FVector ScareCameraStart;
    FQuat ScareRotationStart;
    FVector ScareRobotStart;
    FVector ScareDirection;
    bool bScareImpact = false;
    bool bScareFall = false;
    void UpdateScreamer(float DeltaSeconds);
    void UpdateCorScreamer(float DeltaSeconds);
    void PlayScareSound(bool Impact);
    void BuildRoom();
    void CreateRobots();
    AActor* CreateCor();
    void ResetThreat(FMetalThreat& Threat, float Delay);
    void MoveThreat(FMetalThreat& Threat);
    void StartHour();
    void SaveCheckpoint();
    void LoadCheckpoint();
    void UpdateDetector(float DeltaSeconds);
    void Fail(const FString& Reason);
    void ShowNotice(const FString& Message, float Duration = 3.f);
    void PlayTone(float Frequency, float Duration, float Volume, const FVector* Location = nullptr);
    void RunSmokeTest();
    AActor* Box(const FString& Name, FVector Location, FVector Size, FLinearColor Color, bool bCollision = true);
    UStaticMeshComponent* MeshPart(AActor* ParentActor, const TCHAR* Shape, FVector RelativeLocation, FVector Scale, FLinearColor Color);
    AActor* CreateDevice(FVector Location);
    void Light(FVector Location, FLinearColor Color, float Intensity, float Radius);
};

UCLASS()
class METAL_API AMetalHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    void SelectMoonEntry() { JournalPage = 4; }
private:
    void Stroke(float X1, float Y1, float X2, float Y2, FLinearColor Color, float Thickness);
    void TextAt(const FString& Text, float X, float Y, float Scale, FLinearColor Color);
    void Bar(const FString& Label, float Value, float X, float Y, float Width, FLinearColor Color);
    void DrawMainMenu(AMetalGameMode* Game);
    void DrawDeathScreen(AMetalGameMode* Game);
    void DrawJournal(AMetalGameMode* Game);
    void DrawComputer();
    int32 JournalPage = 0;
    bool bShowControls = false;
};
