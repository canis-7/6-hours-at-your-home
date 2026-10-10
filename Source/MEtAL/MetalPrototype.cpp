#include "MetalPrototype.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "ProceduralMeshComponent.h"
#include "Algo/Reverse.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Metal
{
    const FLinearColor Cyan(.08f, .85f, .85f);
    const FLinearColor Red(.65f, .025f, .075f);
    const FLinearColor Gold(1.f, .48f, .025f);
    const FLinearColor Wall(.24f, .28f, .33f);
    const float Ranges[] = { 850.f, 1100.f, 1450.f, 1850.f };
    const float ScanTimes[] = { 2.8f, 1.8f, 1.f, .35f };
    const float Drain[] = { .065f, .12f, .19f, .28f };
    const float WindowReactionSeconds = 8.f;
    const float ScareImpactSeconds = 1.65f;
    float VentReactionSeconds(int32 Hour)
    {
        const float Delays[] = { 6.f, 5.f, 4.f, 3.f, 2.5f, 2.f };
        return Delays[FMath::Clamp(Hour, 0, 5)];
    }
    float ThreatSpeed(int32 Hour)
    {
        const float Multipliers[] = { 1.f, 1.2f, 1.45f, 1.7f, 2.f, 2.35f };
        return Multipliers[FMath::Clamp(Hour, 0, 5)];
    }
    const TCHAR* SaveSlot = TEXT("MEtAL_Prototype_Checkpoint");

    void Paint(UStaticMeshComponent* Mesh, FLinearColor Color)
    {
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (Base)
        {
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Mesh);
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Mesh->SetMaterial(0, Material);
        }
    }

    AMetalGameMode* Game(const UObject* Context)
    {
        return Cast<AMetalGameMode>(UGameplayStatics::GetGameMode(Context));
    }
}

AMetalCharacter::AMetalCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(28.f, 84.f);
    GetCharacterMovement()->MaxWalkSpeed = 250.f;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = true;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 65));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = 85.f;
    Torch = CreateDefaultSubobject<USpotLightComponent>(TEXT("Lampe"));
    Torch->SetupAttachment(Camera);
    Torch->SetRelativeLocation(FVector(15, 0, -5));
    Torch->SetIntensity(22000.f);
    Torch->AttenuationRadius = 2300.f;
    Torch->InnerConeAngle = 12.f;
    Torch->OuterConeAngle = 28.f;
    Torch->SetLightColor(FLinearColor(.8f, .88f, 1.f));
    Torch->SetVisibility(false);
    Device = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Toupie"));
    Device->SetupAttachment(Camera);
    Device->SetRelativeLocation(FVector(50, 23, -15));
    Device->SetRelativeScale3D(FVector(.19f, .19f, .04f));
    Device->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Device->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Needle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pointeur"));
    Needle->SetupAttachment(Camera);
    Needle->SetRelativeLocation(FVector(50, 23, -12.5f));
    Needle->SetRelativeScale3D(FVector(.14f, .017f, .01f));
    Needle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Needle->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
}

void AMetalCharacter::BeginPlay()
{
    Super::BeginPlay();
    Metal::Paint(Device, Metal::Red);
    Metal::Paint(Needle, Metal::Gold);
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
    if (auto* G = Metal::Game(this)) G->SyncMenuInput();
}

void AMetalCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("Forward"), this, &AMetalCharacter::Forward);
    Input->BindAxis(TEXT("Right"), this, &AMetalCharacter::Right);
    Input->BindAxis(TEXT("LookX"), this, &AMetalCharacter::LookX);
    Input->BindAxis(TEXT("LookY"), this, &AMetalCharacter::LookY);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &AMetalCharacter::Interact);
    Input->BindAction(TEXT("Power"), IE_Pressed, this, &AMetalCharacter::Power);
    Input->BindAction(TEXT("Detector"), IE_Pressed, this, &AMetalCharacter::Detector);
    Input->BindAction(TEXT("Flashlight"), IE_Pressed, this, &AMetalCharacter::Flashlight);
    Input->BindAction(TEXT("Retry"), IE_Pressed, this, &AMetalCharacter::Retry);
    Input->BindAction(TEXT("Pause"), IE_Pressed, this, &AMetalCharacter::Pause);
    Input->BindAction(TEXT("NewGame"), IE_Pressed, this, &AMetalCharacter::NewGame);
    Input->BindKey(EKeys::H, IE_Pressed, this, &AMetalCharacter::Hide);
    Input->BindKey(EKeys::J, IE_Pressed, this, &AMetalCharacter::Journal);
}

void AMetalCharacter::Forward(float V)
{
    if (auto* G = Metal::Game(this); G && !G->bDead && !G->bPaused && !G->bWon && !G->bHiding)
        AddMovementInput(GetActorForwardVector(), V);
}
void AMetalCharacter::Right(float V)
{
    if (auto* G = Metal::Game(this); G && !G->bDead && !G->bPaused && !G->bWon && !G->bHiding)
        AddMovementInput(GetActorRightVector(), V);
}
void AMetalCharacter::LookX(float V)
{
    if (auto* G = Metal::Game(this); G && !G->bPaused && !G->bDead && !G->bWon && !G->bHiding) AddControllerYawInput(V * .6f);
}
void AMetalCharacter::LookY(float V)
{
    if (auto* G = Metal::Game(this); G && !G->bPaused && !G->bDead && !G->bWon && !G->bHiding) AddControllerPitchInput(V * .6f);
}
AActor* AMetalCharacter::FindInteraction() const
{
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (auto* G = Metal::Game(this); G && G->bCarryingStool) Params.AddIgnoredActor(G->Stool);
    FVector Start = Camera->GetComponentLocation();
    GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Camera->GetForwardVector() * 250.f, ECC_Visibility, Params);
    return Hit.GetActor();
}
void AMetalCharacter::Interact() { if (auto* G = Metal::Game(this)) { if (G->bHiding) G->ToggleHiding(); else G->Use(FindInteraction()); } }
void AMetalCharacter::Power()
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (auto* G = Metal::Game(this)) G->SetPower(Now - LastClick < .3f);
    LastClick = Now - LastClick < .3f ? -100.f : Now;
}
void AMetalCharacter::Detector() { if (auto* G = Metal::Game(this)) G->ToggleDetector(); }
void AMetalCharacter::Hide() { if (auto* G = Metal::Game(this)) G->TryHide(FindInteraction()); }
void AMetalCharacter::Journal() { if (auto* G = Metal::Game(this)) G->ToggleJournal(); }
void AMetalCharacter::Flashlight()
{
    if (auto* G = Metal::Game(this); G && !G->bDead && !G->bPaused && !G->bWon && !G->bHiding) Torch->ToggleVisibility();
}
void AMetalCharacter::Retry() { if (auto* G = Metal::Game(this); G && G->bDead && !G->bScreamer) G->RetryHour(); }
void AMetalCharacter::Pause() { if (auto* G = Metal::Game(this)) G->SetPaused(); }
void AMetalCharacter::NewGame()
{
    if (auto* G = Metal::Game(this); G && (G->bPaused || G->bWon)) G->StartNewGame();
}

void AMetalCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* G = Metal::Game(this);
    if (!G) return;
    if (G->bComputerOpen)
    {
        if (auto* PC = Cast<APlayerController>(GetController()); PC && GFrameCounter > G->ComputerInputFrame)
        {
            for (TCHAR Letter = 'A'; Letter <= 'Z'; ++Letter)
            {
                const FKey Key(FName(*FString::Chr(Letter)));
                if (PC->WasInputKeyJustPressed(Key)) G->TypeComputerKey(Key);
            }
            if (PC->WasInputKeyJustPressed(EKeys::BackSpace)) G->TypeComputerKey(EKeys::BackSpace);
            if (PC->WasInputKeyJustPressed(EKeys::Enter)) G->TypeComputerKey(EKeys::Enter);
        }
        Device->SetVisibility(false); Needle->SetVisibility(false);
        return;
    }
    if (G->bCarryingStool)
    {
        G->Stool->SetActorLocation(Camera->GetComponentLocation() + GetActorForwardVector() * 65.f - FVector(0, 0, 46));
        G->Stool->SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0));
    }
    if (G->bScreamer || G->bCorPreview || G->bHiding) { Device->SetVisibility(false); Needle->SetVisibility(false); return; }
    if (Camera->GetRelativeLocation().Z > 100 && (!GetMovementBase() || GetMovementBase()->GetOwner() != G->Stool)) Camera->SetRelativeLocation(FVector(0, 0, 65));
    Device->SetVisibility(!G->bDocked && !G->bCarryingStool);
    Needle->SetVisibility(!G->bDocked && !G->bCarryingStool);
    if (G->Target != INDEX_NONE && G->Threats.IsValidIndex(G->Target))
    {
        FVector Direction = G->Threats[G->Target].Robot->GetActorLocation() - GetActorLocation();
        float Yaw = Direction.Rotation().Yaw - Camera->GetComponentRotation().Yaw;
        Needle->SetRelativeRotation(FRotator(0, Yaw, 0));
    }
    else Needle->SetRelativeRotation(FRotator(0, 0, 0));
}

AMetalGameMode::AMetalGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AMetalCharacter::StaticClass();
    HUDClass = AMetalHUD::StaticClass();
}

AActor* AMetalGameMode::Box(const FString& Name, FVector Loc, FVector Size, FLinearColor Color, bool Collision)
{
    AActor* Actor = GetWorld()->SpawnActor<AActor>();
#if WITH_EDITOR
    Actor->SetActorLabel(Name);
#endif
    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, TEXT("Shape"));
    Actor->SetRootComponent(Mesh);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(Collision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->RegisterComponent();
    Actor->SetActorLocation(Loc);
    Mesh->SetWorldScale3D(Size / 100.f);
    Metal::Paint(Mesh, Color);
    return Actor;
}

UStaticMeshComponent* AMetalGameMode::MeshPart(AActor* ParentActor, const TCHAR* Shape, FVector Loc, FVector Scale, FLinearColor Color)
{
    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(ParentActor);
    Mesh->SetupAttachment(ParentActor->GetRootComponent());
    Mesh->SetAbsolute(false, false, true);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Shape));
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    // Positions are world-centimetre offsets, independent of the root's shape scale.
    Mesh->SetWorldLocation(ParentActor->GetActorLocation() + Loc);
    Mesh->SetWorldScale3D(Scale);
    Metal::Paint(Mesh, Color);
    return Mesh;
}

void AMetalGameMode::Light(FVector Loc, FLinearColor Color, float Intensity, float Radius)
{
    AActor* Actor = GetWorld()->SpawnActor<AActor>();
    UPointLightComponent* Point = NewObject<UPointLightComponent>(Actor);
    Actor->SetRootComponent(Point);
    Point->SetMobility(EComponentMobility::Movable);
    Point->RegisterComponent();
    Actor->SetActorLocation(Loc);
    Point->SetIntensity(Intensity);
    Point->SetAttenuationRadius(Radius);
    Point->SetLightColor(Color);
    RoomLights.Add(Point);
}

AActor* AMetalGameMode::CreateDevice(FVector Loc)
{
    AActor* Actor = Box(TEXT("Prototype MEtAL"), Loc, FVector(20, 20, 3), Metal::Red, false);
    MeshPart(Actor, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 2), FVector(.22f, .22f, .03f), Metal::Red);
    DockNeedle = MeshPart(Actor, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, 0, 4), FVector(.16f, .025f, .01f), Metal::Gold);
    MeshPart(Actor, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 5), FVector(.06f, .06f, .02f), Metal::Gold);
    Actor->SetActorHiddenInGame(true);
    return Actor;
}

void AMetalGameMode::BuildRoom()
{
    Box(TEXT("Sol"), FVector(0, 0, -12), FVector(720, 1020, 24), FLinearColor(.15f, .12f, .10f));
    // Four ceiling panels leave a real 120 cm opening above the central aisle.
    Box(TEXT("Plafond ouest"), FVector(-240, 0, 292), FVector(240, 1020, 24), Metal::Wall);
    Box(TEXT("Plafond est"), FVector(180, 0, 292), FVector(360, 1020, 24), Metal::Wall);
    Box(TEXT("Plafond nord"), FVector(-60, -315, 292), FVector(120, 390, 24), Metal::Wall);
    Box(TEXT("Plafond sud"), FVector(-60, 255, 292), FVector(120, 510, 24), Metal::Wall);
    const FLinearColor DuctMetal(.19f, .23f, .26f);
    Box(TEXT("Conduit gauche"), FVector(-125, -60, 355), FVector(10, 140, 150), DuctMetal);
    Box(TEXT("Conduit droit"), FVector(5, -60, 355), FVector(10, 140, 150), DuctMetal);
    Box(TEXT("Conduit fond"), FVector(-60, -125, 355), FVector(120, 10, 150), DuctMetal);
    Box(TEXT("Tunnel sol"), FVector(-60, 170, 306), FVector(140, 340, 12), DuctMetal);
    Box(TEXT("Tunnel plafond"), FVector(-60, 170, 430), FVector(140, 340, 12), DuctMetal);
    for (float X : { -125.f, 5.f }) Box(TEXT("Tunnel paroi"), FVector(X, 170, 368), FVector(10, 340, 124), DuctMetal);
    Box(TEXT("Coude fond"), FVector(-60, 340, 368), FVector(140, 12, 124), DuctMetal);
    VentRobot = Box(TEXT("Le Rampant - conduit"), FVector(-60, 220, 365), FVector(100), FLinearColor(.08f, .1f, .12f), false);
    Cast<UStaticMeshComponent>(VentRobot->GetRootComponent())->SetVisibility(false);
    MeshPart(VentRobot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0, 10, 0), FVector(.55f, .85f, .3f), FLinearColor(.12f, .15f, .17f));
    MeshPart(VentRobot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, -35, 4), FVector(.5f, .3f, .34f), FLinearColor(.4f, .43f, .4f));
    for (float X : { -15.f, 15.f })
        MeshPart(VentRobot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(X, -52, 8), FVector(.095f, .045f, .095f), Metal::Red);
    for (float X : { -38.f, 38.f }) for (float Y : { -25.f, 25.f })
    {
        auto* Leg = MeshPart(VentRobot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(X, Y, -7), FVector(.44f, .075f, .09f), FLinearColor(.23f, .26f, .28f));
        Leg->SetWorldRotation(FRotator(0, FMath::Sign(X * Y) * 35.f, 0));
        MeshPart(VentRobot, TEXT("/Engine/BasicShapes/Cone.Cone"), FVector(X * 1.35f, Y - 10, -15), FVector(.12f, .12f, .36f), FLinearColor(.36f, .4f, .42f));
    }
    VentRobot->SetActorHiddenInGame(true);
    Box(TEXT("Fond sombre du conduit"), FVector(-60, -60, 430), FVector(140, 140, 10), FLinearColor(.035f, .04f, .045f));
    for (float Z : { 315.f, 365.f, 415.f })
    {
        Box(TEXT("Renfort conduit"), FVector(-118, -60, Z), FVector(4, 120, 4), Metal::Wall, false);
        Box(TEXT("Renfort conduit"), FVector(-2, -60, Z), FVector(4, 120, 4), Metal::Wall, false);
        Box(TEXT("Renfort conduit"), FVector(-60, -118, Z), FVector(120, 4, 4), Metal::Wall, false);
        Box(TEXT("Renfort conduit"), FVector(-60, -2, Z), FVector(120, 4, 4), Metal::Wall, false);
    }
    Vent = Box(TEXT("Aeration - inspection"), FVector(-60, -60, 273), FVector(136, 136, 4), DuctMetal);
    auto* VentHit = Cast<UStaticMeshComponent>(Vent->GetRootComponent());
    VentHit->SetVisibility(false);
    VentHit->SetCastShadow(false);
    VentHit->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    VentGrille = Box(TEXT("Grille coulissante"), FVector(-60, -124, 270), FVector(136, 8, 6), DuctMetal, false);
    MeshPart(VentGrille, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, 128, 0), FVector(1.36f, .08f, .06f), DuctMetal);
    for (float X : { -64.f, 64.f })
        MeshPart(VentGrille, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(X, 64, 0), FVector(.08f, 1.28f, .06f), DuctMetal);
    for (int32 I = 1; I < 10; ++I)
        MeshPart(VentGrille, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, I * 12.8f, 0), FVector(1.2f, .045f, .045f), DuctMetal);
    Box(TEXT("Mur gauche"), FVector(-350, 0, 140), FVector(20, 1000, 280), Metal::Wall);
    Box(TEXT("Mur droit nord"), FVector(350, -445, 140), FVector(20, 110, 280), Metal::Wall);
    Box(TEXT("Mur droit sud"), FVector(350, 125, 140), FVector(20, 750, 280), Metal::Wall);
    Box(TEXT("Linteau porte"), FVector(350, -320, 260), FVector(20, 140, 40), Metal::Wall);
    for (float Y : { -500.f, 500.f })
    {
        Box(TEXT("Mur fenetre gauche"), FVector(-220, Y, 140), FVector(260, 20, 280), Metal::Wall);
        Box(TEXT("Mur fenetre droit"), FVector(220, Y, 140), FVector(260, 20, 280), Metal::Wall);
        Box(TEXT("Allege"), FVector(0, Y, 40), FVector(180, 20, 80), Metal::Wall);
        Box(TEXT("Linteau fenetre"), FVector(0, Y, 255), FVector(180, 20, 50), Metal::Wall);
        Box(TEXT("Cadre gauche"), FVector(-86, Y - FMath::Sign(Y) * 7, 155), FVector(8, 26, 156), FLinearColor(.55f, .55f, .6f));
        Box(TEXT("Cadre droit"), FVector(86, Y - FMath::Sign(Y) * 7, 155), FVector(8, 26, 156), FLinearColor(.55f, .55f, .6f));
        Box(TEXT("Appui fenetre"), FVector(0, Y - FMath::Sign(Y) * 10, 80), FVector(190, 40, 8), FLinearColor(.5f, .5f, .55f));
    }
    TopWindow = Box(TEXT("Volet nord"), FVector(162, -490, 155), FVector(160, 12, 145), FLinearColor(.15f, .22f, .28f));
    TopWindow->Tags.Add(TEXT("TopWindow"));
    BottomWindow = Box(TEXT("Volet sud"), FVector(162, 490, 155), FVector(160, 12, 145), FLinearColor(.15f, .22f, .28f));
    BottomWindow->Tags.Add(TEXT("BottomWindow"));
    Door = Box(TEXT("Porte"), FVector(350, -320, 115), FVector(16, 140, 230), FLinearColor(.35f, .21f, .12f));
    Door->Tags.Add(TEXT("Door"));
    DoorMaterial = Cast<UMaterialInstanceDynamic>(Cast<UStaticMeshComponent>(Door->GetRootComponent())->GetMaterial(0));
    Box(TEXT("Poignee"), FVector(338, -270, 115), FVector(8, 17, 5), Metal::Gold, false);
    Wardrobe = Box(TEXT("Fond placard"), FVector(340, 220, 125), FVector(8, 320, 250), FLinearColor(.075f, .055f, .035f));
    Wardrobe->Tags.Add(TEXT("Closet"));
    for (float Y : { 60.f, 380.f })
        Box(TEXT("Cote placard"), FVector(300, Y, 125), FVector(85, 8, 250), FLinearColor(.23f, .15f, .11f))->Tags.Add(TEXT("Closet"));
    Box(TEXT("Dessus placard"), FVector(300, 220, 249), FVector(85, 320, 8), FLinearColor(.23f, .15f, .11f));
    ClosetLeft = Box(TEXT("Porte gauche placard"), FVector(255, 140, 125), FVector(8, 152, 248), FLinearColor(.23f, .15f, .11f));
    ClosetRight = Box(TEXT("Porte droite placard"), FVector(255, 300, 125), FVector(8, 152, 248), FLinearColor(.23f, .15f, .11f));
    for (AActor* Leaf : { ClosetLeft.Get(), ClosetRight.Get() })
    {
        Leaf->Tags.Add(TEXT("Closet"));
        MeshPart(Leaf, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-7, Leaf == ClosetLeft ? 60 : -60, 0), FVector(.04f, .04f, .25f), Metal::Gold);
    }
    ClosetRobot = Box(TEXT("Le Guetteur - placard"), FVector(315, 220, 125), FVector(100), FLinearColor::Black, false);
    Cast<UStaticMeshComponent>(ClosetRobot->GetRootComponent())->SetVisibility(false);
    const FLinearColor Rust(.32f, .13f, .045f);
    MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, 0, -25), FVector(.35f, .55f, .85f), Rust);
    MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(-5, 0, 35), FVector(.5f, .6f, .55f), Rust);
    MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-30, 0, 22), FVector(.38f, .3f, .2f), FLinearColor(.13f, .085f, .045f));
    for (float Y : { -20.f, 20.f })
    {
        MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Cone.Cone"), FVector(0, Y, 73), FVector(.18f, .23f, .45f), Rust);
        MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(-28, Y * .7f, 40), FVector(.07f, .09f, .1f), Metal::Gold);
        MeshPart(ClosetRobot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-5, Y * 1.6f, -28), FVector(.12f, .13f, .9f), Rust);
    }
    ClosetRobot->SetActorHiddenInGame(true);
    Box(TEXT("Bureau"), FVector(205, -418, 80), FVector(180, 80, 10), FLinearColor(.34f, .23f, .13f));
    for (float X : { 135.f, 275.f }) for (float Y : { -445.f, -390.f })
        Box(TEXT("Pied bureau"), FVector(X, Y, 39), FVector(10, 10, 78), FLinearColor(.12f, .12f, .13f));
    Dock = Box(TEXT("Socle blanc - porte"), FVector(230, -411, 90), FVector(36, 36, 10), FLinearColor(.9f, .9f, .93f));
    Cast<UStaticMeshComponent>(Dock->GetRootComponent())->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    MeshPart(Dock, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 5.5f), FVector(.10f, .10f, .008f), FLinearColor(.05f, .06f, .07f));
    Dock->Tags.Add(TEXT("Dock"));
    DockDevice = CreateDevice(FVector(230, -411, 99));
    Computer = Box(TEXT("Ordinateur MEtAL"), FVector(162, -440, 115), FVector(55, 9, 40), FLinearColor(.025f, .028f, .03f));
    Box(TEXT("Pied ecran"), FVector(162, -440, 93), FVector(5, 10, 12), FLinearColor(.13f, .13f, .15f));
    Box(TEXT("Clavier"), FVector(155, -404, 88), FVector(55, 20, 3), FLinearColor(.12f, .13f, .15f));
    Box(TEXT("Chaise"), FVector(190, -320, 46), FVector(55, 55, 8), FLinearColor(.11f, .12f, .14f));
    Box(TEXT("Dossier"), FVector(190, -291, 79), FVector(55, 8, 64), FLinearColor(.11f, .12f, .14f));
    for (float X : { 167.f, 213.f }) for (float Y : { -343.f, -298.f })
        Box(TEXT("Pied chaise"), FVector(X, Y, 21), FVector(4, 4, 42), FLinearColor(.13f, .13f, .13f));
    Box(TEXT("Lit"), FVector(-238, 242, 30), FVector(150, 260, 60), FLinearColor(.14f, .15f, .18f));
    Box(TEXT("Matelas"), FVector(-238, 242, 67), FVector(148, 258, 18), FLinearColor(.35f, .38f, .41f));
    Box(TEXT("Oreiller"), FVector(-238, 337, 82), FVector(102, 55, 12), FLinearColor(.6f, .63f, .65f));
    Box(TEXT("Tapis"), FVector(-20, 45, 1), FVector(260, 300, 2), FLinearColor(.22f, .17f, .15f), false);
    StoolHome = Box(TEXT("Place du marchepied"), FVector(-260, -120, 3), FVector(82, 72, 2), Metal::Cyan);
    StoolVent = Box(TEXT("Marchepied sous aeration"), FVector(-60, -60, 4), FVector(82, 72, 2), Metal::Cyan);
    Stool = Box(TEXT("Marchepied transportable"), FVector(-260, -120, 38), FVector(64, 52, 8), FLinearColor(.28f, .32f, .35f));
    Cast<UStaticMeshComponent>(Stool->GetRootComponent())->CanCharacterStepUpOn = ECB_Yes;
    for (float X : { -24.f, 24.f }) for (float Y : { -18.f, 18.f })
        MeshPart(Stool, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(X, Y, -19), FVector(.06f, .06f, .34f), FLinearColor(.12f, .15f, .17f));
    for (float Y : { -16.f, -8.f, 0.f, 8.f, 16.f })
        MeshPart(Stool, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, Y, 4.2f), FVector(.54f, .018f, .006f), FLinearColor(.055f, .06f, .065f));
    ResetStool();
    Box(TEXT("Sol couloir"), FVector(735, -320, -12), FVector(770, 230, 24), FLinearColor(.07f, .075f, .08f));
    Box(TEXT("Sol exterieur nord"), FVector(0, -900, -12), FVector(600, 800, 24), FLinearColor(.025f, .035f, .04f));
    Box(TEXT("Sol exterieur sud"), FVector(0, 900, -12), FVector(600, 800, 24), FLinearColor(.025f, .035f, .04f));
    Light(FVector(0, 0, 242), FLinearColor(.85f, .75f, .62f), 20000.f, 1200.f);
    Light(FVector(0, -430, 205), FLinearColor(.3f, .5f, 1.f), 650.f, 1000.f);
    Light(FVector(0, 430, 205), FLinearColor(.3f, .5f, 1.f), 650.f, 1000.f);
}

AActor* AMetalGameMode::CreateCor()
{
    const FLinearColor Steel(.12f, .15f, .17f);
    const FLinearColor Edge(.3f, .34f, .37f);
    const FLinearColor Face(.52f, .56f, .57f);
    const FLinearColor Dark(.025f, .032f, .038f);
    AActor* Cor = Box(TEXT("COR - gardien de la porte"), FVector::ZeroVector, FVector(100), Steel, false);
    Cast<UStaticMeshComponent>(Cor->GetRootComponent())->SetVisibility(false);
    Cor->Tags.Add(TEXT("Cor"));
    auto Pivot = [&](FVector Position)
    {
        USceneComponent* C = NewObject<USceneComponent>(Cor);
        C->SetupAttachment(Cor->GetRootComponent());
        C->RegisterComponent();
        C->SetRelativeLocation(Position);
        return C;
    };
    CorHead = Pivot(FVector(0, 0, 85));
    CorArm = Pivot(FVector(9, 0, 42));
    USceneComponent* Group = Cor->GetRootComponent();
    auto Part = [&](const TCHAR* Shape, FVector Loc, FVector Scale, FLinearColor Color)
    {
        Loc.X = -Loc.X;
        UStaticMeshComponent* Mesh = MeshPart(Cor, Shape, Loc, Scale, Color);
        Mesh->AttachToComponent(Group, FAttachmentTransformRules::KeepWorldTransform);
        return Mesh;
    };
    auto Plate = [&](TArray<FVector2D> Outline, float Depth, float Y, FLinearColor Color)
    {
        for (FVector2D& Point : Outline) Point.X = -Point.X;
        float Area = 0;
        for (int32 I = 0; I < Outline.Num(); ++I)
        {
            FVector2D A = Outline[I], B = Outline[(I + 1) % Outline.Num()];
            Area += A.X * B.Y - B.X * A.Y;
        }
        if (Area < 0) Algo::Reverse(Outline);
        UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Cor);
        Mesh->SetupAttachment(Cor->GetRootComponent());
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->RegisterComponent();
        TArray<FVector> Vertices, Normals;
        TArray<int32> Triangles;
        TArray<FVector2D> UVs;
        auto Vertex = [&](int32 I, bool Back) { return FVector(Outline[I].X, Y + (Back ? Depth / 2 : -Depth / 2), Outline[I].Y); };
        auto Triangle = [&](FVector A, FVector B, FVector C)
        {
            const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
            const int32 Index = Vertices.Num();
            Vertices.Append({ A, B, C });
            Normals.Append({ Normal, Normal, Normal });
            UVs.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1) });
            Triangles.Append({ Index, Index + 1, Index + 2 });
        };
        for (int32 I = 1; I < Outline.Num() - 1; ++I)
        {
            Triangle(Vertex(0, false), Vertex(I, false), Vertex(I + 1, false));
            Triangle(Vertex(0, true), Vertex(I + 1, true), Vertex(I, true));
        }
        for (int32 I = 0; I < Outline.Num(); ++I)
        {
            const int32 J = (I + 1) % Outline.Num();
            Triangle(Vertex(I, false), Vertex(I, true), Vertex(J, true));
            Triangle(Vertex(I, false), Vertex(J, true), Vertex(J, false));
        }
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
        UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Mesh);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Material);
        Mesh->AttachToComponent(Group, FAttachmentTransformRules::KeepWorldTransform);
    };
    auto Bone = [&](FVector A, FVector B, float Width, FLinearColor Color)
    {
        A.X = -A.X;
        B.X = -B.X;
        UStaticMeshComponent* Mesh = MeshPart(Cor, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), (A + B) / 2, FVector(Width / 100, Width / 100, FVector::Distance(A, B) / 100), Color);
        Mesh->SetWorldRotation(FRotationMatrix::MakeFromZ(B - A).Rotator());
        Mesh->AttachToComponent(Group, FAttachmentTransformRules::KeepWorldTransform);
    };
    auto Joint = [&](FVector Loc, float Width)
    {
        Part(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Loc, FVector(Width / 100), Edge);
    };
    Group = CorHead;
    Plate({ {-48, 82}, {16, 120}, {28, 112}, {29, 52} }, 20, 0, Edge);
    Plate({ {-38, 83}, {15, 113}, {22, 106}, {22, 64} }, 2, -11, Face);
    Group = Cor->GetRootComponent();
    Bone(FVector(3, 0, 48), FVector(12, 0, 78), 10, Dark);
    Plate({ {-27, -22}, {-22, 30}, {-2, 60}, {12, 56}, {10, -22} }, 22, 2, Steel);
    Plate({ {-18, -8}, {-15, 25}, {-1, 48}, {5, 44}, {1, -8} }, 2, -11, Edge);
    // Uneven legs and broad left foot preserve the sketch's silhouette.
    Bone(FVector(-17, 0, -17), FVector(-27, 0, -58), 16, Steel);
    Joint(FVector(-27, 0, -58), 17);
    Bone(FVector(-27, 0, -58), FVector(-37, -2, -88), 14, Steel);
    Part(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(-46, -9, -94), FVector(.48f, .43f, .18f), Edge);
    Bone(FVector(7, 0, -18), FVector(17, 1, -58), 13, Steel);
    Joint(FVector(17, 1, -58), 14);
    Bone(FVector(17, 1, -58), FVector(21, -1, -94), 11, Steel);
    Part(TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(21, -7, -95), FVector(.18f, .35f, .18f), Edge);
    // Raised hooked arm, triangular pincer, and hanging right arm.
    Group = CorArm;
    Bone(FVector(-9, 0, 42), FVector(-46, 0, 67), 15, Steel);
    Bone(FVector(-46, 0, 67), FVector(-67, 0, 66), 15, Steel);
    Joint(FVector(-67, 0, 66), 17);
    Bone(FVector(-67, 0, 66), FVector(-75, -1, 36), 13, Steel);
    Plate({ {-77, 42}, {-98, 12}, {-62, 23} }, 12, -1, Edge);
    Plate({ {-76, 33}, {-87, 17}, {-70, 24} }, 2, -8, Dark);
    Group = Cor->GetRootComponent();
    Bone(FVector(8, 1, 40), FVector(25, 1, 8), 11, Steel);
    Joint(FVector(25, 1, 8), 12);
    Bone(FVector(25, 1, 8), FVector(24, 0, -20), 9, Steel);
    Plate({ {17, -15}, {31, -15}, {31, -31}, {19, -30} }, 13, 0, Edge);
    Group = CorHead;
    for (const FVector Eye : { FVector(-7, -14, 100), FVector(15, -14, 88) })
    {
        Part(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Eye, FVector(.11f, .045f, .16f), Dark);
        Part(TEXT("/Engine/BasicShapes/Sphere.Sphere"), Eye + FVector(0, -2, 0), FVector(.044f, .025f, .1f), Metal::Gold);
        UPointLightComponent* Glow = NewObject<UPointLightComponent>(Cor);
        Glow->SetupAttachment(Cor->GetRootComponent());
        Glow->RegisterComponent();
        Glow->SetRelativeLocation(FVector(-Eye.X, Eye.Y - 5, Eye.Z));
        Glow->SetLightColor(Metal::Gold);
        Glow->SetIntensity(8.f);
        Glow->SetAttenuationRadius(70.f);
        Glow->AttachToComponent(CorHead, FAttachmentTransformRules::KeepWorldTransform);
    }
    Bone(FVector(-15, -13, 78), FVector(8, -13, 69), 2, Dark);
    return Cor;
}

void AMetalGameMode::CreateRobots()
{
    for (int32 Lane = 0; Lane < 3; ++Lane)
    {
        FMetalThreat T;
        T.Lane = Lane;
        if (Lane == 0)
        {
            T.Robot = CreateCor();
            Threats.Add(T);
            continue;
        }
        T.Robot = Box(FString::Printf(TEXT("Robot provisoire %d"), Lane + 1), FVector::ZeroVector, FVector(52, 28, 70), FLinearColor(.18f, .19f, .2f), false);
        MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Sphere.Sphere"), FVector(0, 0, 65), FVector(.46f, .46f, .46f), FLinearColor(.42f, .43f, .46f));
        MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, -24, 67), FVector(.22f, .08f, .08f), Metal::Red);
        MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-15, 0, -52), FVector(.12f, .15f, .42f), FLinearColor(.25f, .27f, .29f));
        T.Limbs.Add(MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-35, 0, -7), FVector(.1f, .12f, .65f), FLinearColor(.28f, .3f, .32f)));
        T.Limbs.Add(MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(35, 0, -7), FVector(.1f, .12f, .65f), FLinearColor(.28f, .3f, .32f)));
        T.Limbs.Add(MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(15, 0, -52), FVector(.12f, .15f, .42f), FLinearColor(.25f, .27f, .29f)));
        T.Limbs.Add(MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(15, -15, -76), FVector(.24f, .45f, .14f), FLinearColor(.12f, .13f, .15f)));
        MeshPart(T.Robot, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(-15, -15, -76), FVector(.24f, .45f, .14f), FLinearColor(.12f, .13f, .15f));
        for (UStaticMeshComponent* Limb : T.Limbs) T.RestPose.Add(Limb->GetRelativeTransform());
        UPointLightComponent* CameraGlow = NewObject<UPointLightComponent>(T.Robot);
        CameraGlow->SetupAttachment(T.Robot->GetRootComponent());
        CameraGlow->RegisterComponent();
        CameraGlow->SetWorldLocation(T.Robot->GetActorLocation() + FVector(0, -30, 65));
        CameraGlow->SetIntensity(55.f);
        CameraGlow->SetLightColor(Metal::Red);
        CameraGlow->SetAttenuationRadius(190.f);
        Threats.Add(T);
    }
}

void AMetalGameMode::RestartPlayer(AController* Player)
{
    if (!Player) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AMetalCharacter* Pawn = GetWorld()->SpawnActor<AMetalCharacter>(FVector(70, -235, 90), FRotator(0, -65, 0), Params);
    if (Pawn) { Player->Possess(Pawn); Player->SetControlRotation(FRotator(-10, -65, 0)); }
}

void AMetalGameMode::BeginPlay()
{
    Super::BeginPlay();
    bSmoke = FParse::Param(FCommandLine::Get(), TEXT("MetalSmokeTest"));
    bPreview = FParse::Param(FCommandLine::Get(), TEXT("MetalPreview"));
    bScarePreview = FParse::Param(FCommandLine::Get(), TEXT("MetalScarePreview"));
    bCorScarePreview = FParse::Param(FCommandLine::Get(), TEXT("MetalCorScarePreview"));
    bScarePreview |= bCorScarePreview;
    bScarePreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalVentScarePreview"));
    bPreview |= bScarePreview;
    bCorPreview = FParse::Param(FCommandLine::Get(), TEXT("MetalCorPreview"));
    bPreview |= bCorPreview;
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalVentPreview"));
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalStoolPreview"));
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalClosetPreview"));
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalDeathPreview"));
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalJournalPreview"));
    bPreview |= FParse::Param(FCommandLine::Get(), TEXT("MetalMoonPreview"));
    bWelcome = FParse::Param(FCommandLine::Get(), TEXT("MetalWelcome"));
    bPaused = bWelcome;
    BuildRoom();
    CreateRobots();
    WindowScream = LoadObject<USoundWave>(nullptr, TEXT("/Game/Audio/WindowScreamer.WindowScreamer"));
    Checkpoint = NewObject<UMetalCheckpoint>(this);
    LoadCheckpoint();
    if (!bSmoke && !bPreview) JournalData = Cast<UMetalJournal>(UGameplayStatics::LoadGameFromSlot(TEXT("MEtAL_Journal"), 0));
    if (!JournalData) JournalData = NewObject<UMetalJournal>(this);
    JournalData->Knowledge.SetNum(4);
    Hour = Checkpoint->Hour;
    DoorHealth = Checkpoint->DoorHealth;
    Oxygen = Checkpoint->Oxygen;
    bTopClosed = Checkpoint->TopClosed;
    bBottomClosed = Checkpoint->BottomClosed;
    TopWindow->SetActorLocation(FVector(bTopClosed ? 0 : 162, -490, 155));
    BottomWindow->SetActorLocation(FVector(bBottomClosed ? 0 : 162, 490, 155));
    for (FMetalThreat& T : Threats) ResetThreat(T, 30.f + T.Lane * 12.f);
    ShowNotice(TEXT("Jack / MEtAL : prototype termine. Le courant semble instable..."), 8.f);
    SyncMenuInput();
    if (bCorPreview)
    {
        Threats[0].Robot->SetActorLocation(FVector(0, -30, 105));
        Threats[0].Robot->SetActorRotation(FRotator::ZeroRotator);
        Threats[0].Robot->SetActorHiddenInGame(false);
        if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            P->SetActorLocation(FVector(0, -450, 90));
            P->Controller->SetControlRotation(FRotator(-12, 90, 0));
            P->Camera->FieldOfView = 95;
            P->Torch->SetVisibility(true);
            P->Torch->SetIntensity(1200.f);
            RoomLights[0]->SetIntensity(4000.f);
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalVentPreview")))
    {
        Use(Stool); Use(StoolVent);
        Use(Vent);
        VentRobot->SetActorHiddenInGame(false);
        VentRobot->SetActorLocation(FVector(-60, 110, 365));
        if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            P->SetActorLocation(FVector(-60, -60, 128));
            P->Controller->SetControlRotation(FRotator(58, 90, 0));
            P->Torch->SetVisibility(true);
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalStoolPreview")))
    {
        Use(Stool);
        Use(StoolVent);
        if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            P->SetActorLocation(FVector(-60, -225, 90));
            P->Controller->SetControlRotation(FRotator(-30, 90, 0));
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalClosetPreview")))
    {
        Hour = 2; Intermission = 0; ClosetProgress = .7f;
        SetClosetOpen(true);
        if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            P->SetActorLocation(FVector(85, 220, 90));
            P->Controller->SetControlRotation(FRotator(3, 0, 0));
            P->Torch->SetVisibility(true); P->Torch->SetIntensity(2500);
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalDeathPreview"))) Fail(TEXT("OXYGENE"));
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalJournalPreview")))
    {
        LearnMonster(0,3); LearnMonster(1,1); LearnMonster(2,2);
        ToggleJournal();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalMoonPreview")))
    {
        Use(Computer);
        for (TCHAR Letter : FString(TEXT("MOONLIGHT"))) TypeComputerKey(FKey(FName(*FString::Chr(Letter))));
    }
    UE_LOG(LogTemp, Display, TEXT("METAL: room ready, %d robots, checkpoint hour %d"), Threats.Num(), Hour + 1);
}

void AMetalGameMode::ResetThreat(FMetalThreat& T, float Delay)
{
    for (int32 I = 0; I < T.Limbs.Num(); ++I) T.Limbs[I]->SetRelativeTransform(T.RestPose[I]);
    T.Stage = 1;
    T.Timer = Delay;
    T.StrikeTimer = 0.f;
    T.WaitTime = 0.f;
    MoveThreat(T);
}

void AMetalGameMode::MoveThreat(FMetalThreat& T)
{
    const float DoorDistances[] = { 1650, 1120, 740, 430 };
    const float WindowDistances[] = { 1850, 1250, 880, 580 };
    FVector Position = T.Lane == 0
        ? FVector(DoorDistances[T.Stage - 1], -320, 105)
        : FVector(0, (T.Lane == 1 ? -1 : 1) * WindowDistances[T.Stage - 1], 105);
    T.Robot->SetActorLocation(Position);
    T.Robot->SetActorRotation(T.Lane == 0 ? FRotator(0, -90, 0) : FRotator(0, T.Lane == 1 ? 180 : 0, 0));
    T.Robot->SetActorHiddenInGame(T.Stage == 1 || Intermission > 0);
}

void AMetalGameMode::LoadCheckpoint()
{
    if (bSmoke || bPreview) return;
    if (UMetalCheckpoint* Saved = Cast<UMetalCheckpoint>(UGameplayStatics::LoadGameFromSlot(Metal::SaveSlot, 0)))
    {
        if (Saved->Hour >= 0 && Saved->Hour < 6 && Saved->DoorHealth > 0.f && Saved->DoorHealth <= 100.f)
            Checkpoint = Saved;
    }
}

void AMetalGameMode::SaveCheckpoint()
{
    Checkpoint->Hour = Hour;
    Checkpoint->DoorHealth = DoorHealth;
    Checkpoint->Oxygen = Oxygen;
    Checkpoint->TopClosed = bTopClosed;
    Checkpoint->BottomClosed = bBottomClosed;
    if (!bSmoke && !bPreview) UGameplayStatics::SaveGameToSlot(Checkpoint, Metal::SaveSlot, 0);
}

void AMetalGameMode::StartHour()
{
    VentStage = 0; VentTimer = 45.f / Metal::ThreatSpeed(Hour);
    ClosetProgress = 0; ClosetWarnStage = 0;
    ClosetRobot->SetActorHiddenInGame(Hour < 2);
    ClosetRobot->SetActorLocation(FVector(315, 220, 125));
    VentRobot->SetActorHiddenInGame(true);
    Seconds = 0.f;
    Battery = 100.f;
    SaveCheckpoint();
    const float Speed = Metal::ThreatSpeed(Hour);
    for (FMetalThreat& T : Threats) ResetThreat(T, (22.f + T.Lane * 14.f) / Speed);
    RoomLights[0]->SetIntensity(48.f);
    RoomLights[1]->SetIntensity(95.f);
    RoomLights[2]->SetIntensity(95.f);
    ShowNotice(TEXT("PANNE DE COURANT / Survis jusqu'au retour du courant."), 5.f);
    PlayTone(60.f, .8f, .35f);
    UE_LOG(LogTemp, Display, TEXT("METAL: hour %d started; door %.1f"), Hour + 1, DoorHealth);
}

void AMetalGameMode::ShowNotice(const FString& Message, float Duration)
{
    Notice = Message;
    NoticeTime = Duration;
}

void AMetalGameMode::PlayTone(float Frequency, float Duration, float Volume, const FVector* Location)
{
    if (bSmoke) return;
    const int32 Rate = 22050;
    const int32 Count = FMath::RoundToInt(Rate * Duration);
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Count * sizeof(int16));
    for (int32 I = 0; I < Count; ++I)
    {
        float Envelope = FMath::Min(I / (Rate * .015f), 1.f) * FMath::Min((Count - I) / (Rate * .05f), 1.f);
        const int16 Sample = static_cast<int16>(FMath::Sin(2.f * PI * Frequency * I / Rate) * 10000.f * Envelope);
        FMemory::Memcpy(Bytes.GetData() + I * sizeof(int16), &Sample, sizeof(int16));
    }
    USoundWaveProcedural* Sound = NewObject<USoundWaveProcedural>();
    Sound->SetSampleRate(Rate);
    Sound->NumChannels = 1;
    Sound->Duration = Duration;
    Sound->SoundGroup = SOUNDGROUP_Effects;
    Sound->QueueAudio(Bytes.GetData(), Bytes.Num());
    if (Location) UGameplayStatics::PlaySoundAtLocation(this, Sound, *Location, Volume);
    else UGameplayStatics::PlaySound2D(this, Sound, Volume);
}

void AMetalGameMode::Use(AActor* Item)
{
    if (!Item || bDead || bWon || bPaused) return;
    if (Item == Computer && !bCarryingStool)
    {
        bComputerOpen = true; bPaused = true;
        ComputerInputFrame = GFrameCounter;
        ComputerText.Empty(); ComputerMessage = TEXT("Prototype MEtAL / terminal local pret.");
        if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this,0))) P->GetCharacterMovement()->StopMovementImmediately();
        SyncMenuInput();
        return;
    }
    if (Item->ActorHasTag(TEXT("Closet")) && !bCarryingStool) { SetClosetOpen(!bClosetOpen); return; }
    if (bCarryingStool)
    {
        if (Item != StoolHome && Item != StoolVent)
        {
            ShowNotice(TEXT("Pose le marchepied sur un des deux emplacements bleus."));
            return;
        }
        FVector Position = Item->GetActorLocation();
        Position.Z = 38;
        Stool->SetActorLocation(Position);
        Stool->SetActorRotation(FRotator::ZeroRotator);
        Stool->SetActorEnableCollision(true);
        bCarryingStool = false;
        for (AActor* Place : { StoolHome.Get(), StoolVent.Get() })
        {
            Place->SetActorHiddenInGame(true);
            Place->SetActorEnableCollision(false);
        }
        ShowNotice(Item == StoolVent ? TEXT("Avance sur le marchepied pour regarder dans le conduit.") : TEXT("Marchepied range."));
        return;
    }
    if (Item == Stool)
    {
        AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
        if (P && P->GetMovementBase() && P->GetMovementBase()->GetOwner() == Stool)
        {
            ShowNotice(TEXT("Descends du marchepied avant de le prendre."));
            return;
        }
        bCarryingStool = true;
        Stool->SetActorEnableCollision(false);
        for (AActor* Place : { StoolHome.Get(), StoolVent.Get() })
        {
            Place->SetActorHiddenInGame(false);
            Place->SetActorEnableCollision(true);
        }
        ShowNotice(TEXT("Marchepied porte. Vise un emplacement bleu puis E pour poser."));
        return;
    }
    if (Item == Vent)
    {
        bVentOpen = !bVentOpen;
        if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); P && P->GetMovementBase() && P->GetMovementBase()->GetOwner() == Stool)
        {
            P->Camera->SetRelativeLocation(FVector(0, 0, bVentOpen ? 205.f : 65.f));
            if (bVentOpen) P->Controller->SetControlRotation(FRotator(10, 90, 0));
        }
        VentGrille->SetActorLocation(FVector(bVentOpen ? 85.f : -60.f, -124, 270));
        ShowNotice(bVentOpen ? TEXT("Conduit ouvert. F / lampe pour regarder a l'interieur.") : TEXT("Grille refermee."));
        const FVector SoundPosition = Vent->GetActorLocation();
        PlayTone(230.f, .18f, .18f, &SoundPosition);
        return;
    }
    if (Item == Dock)
    {
        bDocked = !bDocked;
        bDetector = false;
        Target = Candidate = INDEX_NONE;
        AcquireTime = 0;
        DockDevice->SetActorHiddenInGame(!bDocked);
        ShowNotice(bDocked ? TEXT("Toupie posee / porte alimentee. Detection indisponible.") : TEXT("Toupie reprise / la porte n'est plus protegee."));
        PlayTone(bDocked ? 500.f : 340.f, .12f, .25f);
    }
    else if (Item == TopWindow || Item == BottomWindow)
    {
        bool& Closed = Item == TopWindow ? bTopClosed : bBottomClosed;
        Closed = !Closed;
        FVector Loc = Item->GetActorLocation();
        Loc.X = Closed ? 0.f : 162.f;
        Item->SetActorLocation(Loc);
        ShowNotice(Closed ? TEXT("Fenetre fermee / ventilation reduite.") : TEXT("Fenetre ouverte / surveille les robots."));
        const int32 Lane = Item == TopWindow ? 1 : 2;
        if (Threats.IsValidIndex(Lane) && Threats[Lane].Stage == 4)
        {
            Threats[Lane].StrikeTimer = Metal::WindowReactionSeconds;
            if (!Closed) ShowNotice(TEXT("Le robot attend encore ! Tu as 8 secondes pour refermer."), Metal::WindowReactionSeconds);
        }
        PlayTone(170.f, .17f, .25f, &Loc);
    }
}

void AMetalGameMode::SetPower(bool Reset)
{
    if (bDead || bWon || bPaused || bDocked) return;
    PowerLevel = Reset ? 0 : FMath::Min(PowerLevel + 1, 3);
    bDetector = Battery > 0;
    Candidate = Target = INDEX_NONE;
    AcquireTime = 0;
    ShowNotice(FString::Printf(TEXT("Puissance %d / 4 : portee %.1f m. Double-clic : niveau 1."), PowerLevel + 1, Metal::Ranges[PowerLevel] / 100.f));
    PlayTone(400.f + PowerLevel * 140.f, .1f, .2f);
}

void AMetalGameMode::ToggleDetector()
{
    if (bDead || bWon || bPaused || bDocked) return;
    bDetector = !bDetector && Battery > 0;
    Target = Candidate = INDEX_NONE;
    AcquireTime = 0;
}

void AMetalGameMode::UpdateDetector(float Dt)
{
    if (!bDetector || bDocked || Battery <= 0 || Intermission > 0)
    {
        Target = Candidate = INDEX_NONE;
        AcquireTime = 0;
        return;
    }
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn) return;
    int32 Closest = INDEX_NONE;
    float Distance = Metal::Ranges[PowerLevel];
    for (int32 I = 0; I < Threats.Num(); ++I)
    {
        // First-stage threats remain invisible to the detector, at every power level.
        if (Threats[I].Stage < 2) continue;
        float D = FVector::Dist2D(Pawn->GetActorLocation(), Threats[I].Robot->GetActorLocation());
        if (D <= Distance) { Closest = I; Distance = D; }
    }
    if (Closest != Candidate) { Candidate = Closest; AcquireTime = 0; Target = INDEX_NONE; }
    if (Candidate != INDEX_NONE)
    {
        const bool PreviouslyLocked = Target != INDEX_NONE;
        AcquireTime += Dt;
        if (AcquireTime >= Metal::ScanTimes[PowerLevel])
        {
            Target = Candidate;
            if (!PreviouslyLocked) PlayTone(900.f, .16f, .22f);
        }
    }
    else { Target = INDEX_NONE; AcquireTime = 0; }
}

FString AMetalGameMode::InteractionLabel(AActor* Item) const
{
    if (Item == Computer && !bCarryingStool) return TEXT("E / Utiliser l'ordinateur");
    if (bHiding) return TEXT("E / Sortir de l'armoire");
    if (Item && Item->ActorHasTag(TEXT("Closet")) && !bCarryingStool) return bClosetOpen ? TEXT("E / Fermer - F / Lampe - H / Se cacher") : TEXT("E / Ouvrir le placard - H / Se cacher");
    if (bCarryingStool)
    {
        if (Item == StoolHome) return TEXT("E / Ranger le marchepied");
        if (Item == StoolVent) return TEXT("E / Poser sous l'aeration");
        return TEXT("Marchepied porte / vise un des deux emplacements bleus");
    }
    if (Item == Stool) return TEXT("E / Porter le marchepied - avance dessus pour monter");
    if (Item == Vent) return bVentOpen ? TEXT("E / Refermer la grille") : TEXT("E / Ouvrir la grille et regarder");
    if (Item == Dock) return bDocked ? TEXT("E / Reprendre la toupie") : TEXT("E / Poser la toupie et proteger la porte");
    if (Item == TopWindow) return bTopClosed ? TEXT("E / Ouvrir la fenetre nord") : TEXT("E / Fermer la fenetre nord");
    if (Item == BottomWindow) return bBottomClosed ? TEXT("E / Ouvrir la fenetre sud") : TEXT("E / Fermer la fenetre sud");
    if (Item == Door) return TEXT("La porte est protegee quand la toupie est sur le socle.");
    return FString();
}

FString AMetalGameMode::Status() const
{
    if (bDocked) return Battery > 0 ? TEXT("SOCLE / porte protegee") : TEXT("SOCLE / batterie vide !");
    if (!bDetector) return TEXT("DETECTEUR ETEINT");
    if (Target != INDEX_NONE) return TEXT("SIGNAL LOCALISE");
    if (Candidate != INDEX_NONE) return TEXT("ACQUISITION DU SIGNAL...");
    return TEXT("RECHERCHE / aucun signal proche");
}

void AMetalGameMode::Fail(const FString& Reason)
{
    if (bDead) return;
    bDead = true;
    bDetector = false;
    Target = INDEX_NONE;
    DeathReason = Reason;
    if (Reason == TEXT("PORTE")) LearnMonster(0, 3);
    else if (Reason.StartsWith(TEXT("FENETRE"))) LearnMonster(1, 3);
    else if (Reason == TEXT("CONDUIT")) LearnMonster(2, 3);
    else if (Reason == TEXT("PLACARD")) LearnMonster(3, 3);
    DeathScreenTime = 0;
    if (Reason == TEXT("CONDUIT") || Reason == TEXT("PLACARD"))
    {
        if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            bScreamer = true; ScareLane = Reason == TEXT("PLACARD") ? 4 : 3; ScreamerTime = 0; ScreamerBlackoutAt = 1.4f; bScareImpact = false;
            ScareCameraStart = P->Camera->GetComponentLocation();
            ScareRotationStart = P->Camera->GetComponentQuat();
            ScareRobotStart = (ScareLane == 4 ? ClosetRobot : VentRobot)->GetActorLocation();
            if (ScareLane == 4) SetClosetOpen(true);
            P->Camera->bUsePawnControlRotation = false;
            P->GetCharacterMovement()->StopMovementImmediately();
            P->Torch->SetVisibility(true);
            VentRobot->SetActorHiddenInGame(false);
            PlayScareSound(false);
        }
        return;
    }
    if (Reason.StartsWith(TEXT("FENETRE")) || Reason == TEXT("PORTE"))
    {
        if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            ScareLane = Reason == TEXT("PORTE") ? 0 : (Reason == TEXT("FENETRE NORD") ? 1 : 2);
            ScreamerBlackoutAt = ScareLane == 0 ? 1.25f : Metal::ScareImpactSeconds;
            if (ScareLane == 0) Door->SetActorHiddenInGame(true);
            bScreamer = true;
            ScreamerTime = 0;
            bScareImpact = bScareFall = false;
            ScareCameraStart = P->Camera->GetComponentLocation();
            ScareRotationStart = P->Camera->GetComponentQuat();
            ScareRobotStart = Threats[ScareLane].Robot->GetActorLocation();
            ScareDirection = (ScareRobotStart - ScareCameraStart).GetSafeNormal2D();
            P->Camera->bUsePawnControlRotation = false;
            P->GetCharacterMovement()->StopMovementImmediately();
            P->Torch->SetVisibility(true);
            P->Torch->SetIntensity(3500.f);
            for (FMetalThreat& T : Threats) T.Robot->SetActorHiddenInGame(T.Lane != ScareLane);
            PlayScareSound(false);
        }
    }
    else PlayTone(45.f, 1.4f, .7f);
    UE_LOG(LogTemp, Display, TEXT("METAL: player died: %s"), *Reason);
}

void AMetalGameMode::PlayScareSound(bool Impact)
{
    if (bSmoke) return;
    if (!Impact && WindowScream)
    {
        if (IsValid(ActiveScream)) ActiveScream->Stop();
        ActiveScream = UGameplayStatics::SpawnSound2D(this, WindowScream, .8f);
        UE_LOG(LogTemp, Display, TEXT("METAL_AUDIO: supplied screamer started=%d duration=%.2f"), IsValid(ActiveScream) && ActiveScream->IsPlaying(), WindowScream->Duration);
        return;
    }
    const int32 Rate = 22050;
    const float Duration = Impact ? .65f : 1.1f;
    const int32 Count = FMath::RoundToInt(Rate * Duration);
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Count * sizeof(int16));
    FRandomStream Noise(9271);
    float Phase = 0;
    for (int32 I = 0; I < Count; ++I)
    {
        const float T = float(I) / Rate;
        const float Frequency = Impact ? FMath::Lerp(120.f, 38.f, T / Duration) : 1100.f - 750.f * T / Duration + 130.f * FMath::Sin(T * 85.f);
        Phase += 2.f * PI * Frequency / Rate;
        const float Envelope = FMath::Min(T / .012f, 1.f) * FMath::Exp(-T * (Impact ? 7.f : 2.6f));
        const float Wave = .65f * FMath::Sin(Phase) + .18f * FMath::Sin(Phase * 2.7f) + .17f * Noise.FRandRange(-1.f, 1.f);
        const int16 Sample = static_cast<int16>(Wave * Envelope * 22000.f);
        FMemory::Memcpy(Bytes.GetData() + I * sizeof(int16), &Sample, sizeof(int16));
    }
    USoundWaveProcedural* Sound = NewObject<USoundWaveProcedural>();
    Sound->SetSampleRate(Rate);
    Sound->NumChannels = 1;
    Sound->Duration = Duration;
    Sound->QueueAudio(Bytes.GetData(), Bytes.Num());
    UGameplayStatics::PlaySound2D(this, Sound, .65f);
}

void AMetalGameMode::UpdateScreamer(float Dt)
{
    if (ScareLane >= 3) { UpdateVentScare(Dt); return; }
    if (ScareLane == 0) { UpdateCorScreamer(Dt); return; }
    AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!P || !Threats.IsValidIndex(ScareLane)) { bScreamer = false; return; }
    ScreamerTime += Dt;
    // Rush and grab take 0.4 s; the remaining movement is accelerated 1.6x.
    // Audio plays at its original speed and continues through the impact blackout.
    const float T = ScreamerTime < .4f ? ScreamerTime * 2.75f : 1.1f + (ScreamerTime - .4f) * 1.6f;
    FMetalThreat& Robot = Threats[ScareLane];
    const FVector Right(-ScareDirection.Y, ScareDirection.X, 0);
    const FVector Floor(0, -30, 24);
    FVector GrabRobot = ScareCameraStart + ScareDirection * 68.f;
    GrabRobot.Z = 105;
    FVector StandingRobot = Floor + ScareDirection * 110.f;
    StandingRobot.Z = 105;
    const float Throw = FMath::SmoothStep(1.1f, 1.85f, T);
    const FVector RobotLocation = T < 1.1f
        ? FMath::Lerp(ScareRobotStart, GrabRobot, FMath::SmoothStep(0.f, .6f, T))
        : FMath::Lerp(GrabRobot, StandingRobot, Throw);
    Robot.Robot->SetActorLocation(RobotLocation);
    Robot.Robot->SetActorRotation(FRotator(0, ScareDirection.Rotation().Yaw - 90.f, 0));
    FVector CameraPosition = FMath::Lerp(ScareCameraStart, Floor, Throw);
    if (T > .6f && T < 1.1f) CameraPosition += Right * FMath::Sin(T * 70.f) * 1.8f;
    if (T > 1.1f && T < 1.85f) CameraPosition.Z += FMath::Sin(Throw * PI) * 22.f;
    FRotator Look = (RobotLocation + FVector(0, 0, 65) - CameraPosition).Rotation();
    Look.Roll = -12.f * Throw;
    P->Camera->SetWorldLocation(CameraPosition);
    P->Camera->SetWorldRotation(FQuat::Slerp(ScareRotationStart, Look.Quaternion(), FMath::Clamp(T / .35f, 0.f, 1.f)));
    P->Camera->FieldOfView = FMath::Lerp(85.f, 100.f, Throw);
    auto LimbBetween = [](UStaticMeshComponent* Limb, FVector A, FVector B, float Width)
    {
        Limb->SetWorldLocation((A + B) / 2);
        Limb->SetWorldRotation(FRotationMatrix::MakeFromZ(B - A).Rotator());
        Limb->SetWorldScale3D(FVector(Width, Width, FVector::Distance(A, B) / 100.f));
    };
    for (int32 I = 0; I < 2; ++I)
    {
        const float Side = I == 0 ? -1.f : 1.f;
        const FVector Shoulder = RobotLocation + Right * Side * 32 + FVector(0, 0, 24);
        const FVector GrabHand = ScareCameraStart + Right * Side * 22 + ScareDirection * 8 - FVector(0, 0, 12);
        const FVector RestHand = RobotLocation + Right * Side * 38 - FVector(0, 0, 30);
        FVector Hand = T < .6f ? FMath::Lerp(RestHand, GrabHand, T / .6f) : FMath::Lerp(GrabHand, RestHand, Throw);
        LimbBetween(Robot.Limbs[I], Shoulder, Hand, .13f);
    }
    FVector Foot = RobotLocation - Right * 15 - ScareDirection * 15 - FVector(0, 0, 76);
    const FVector RaisedFoot = Floor + ScareDirection * 60 + FVector(0, 0, 121);
    if (T >= 2.f && T < 2.75f) Foot = FMath::Lerp(Foot, RaisedFoot, FMath::SmoothStep(2.f, 2.65f, T));
    else if (T >= 2.75f) Foot = FMath::Lerp(RaisedFoot, Floor + ScareDirection * 4 - FVector(0, 0, 9), FMath::Clamp((T - 2.75f) / .35f, 0.f, 1.f));
    Robot.Limbs[3]->SetWorldLocation(Foot);
    Robot.Limbs[3]->SetWorldRotation(Robot.Robot->GetActorRotation());
    Robot.Limbs[3]->SetWorldScale3D(FVector(.32f, .52f, .16f));
    LimbBetween(Robot.Limbs[2], RobotLocation - Right * 15 - FVector(0, 0, 30), Foot + FVector(0, 0, 10), .16f);
    if (T >= 1.85f && !bScareFall) { bScareFall = true; PlayTone(85.f, .25f, .5f); }
    if (T >= 3.1f && !bScareImpact) { bScareImpact = true; PlayScareSound(true); P->Torch->SetVisibility(false); }
    const float EndTime = WindowScream ? FMath::Max(2.25f, WindowScream->Duration + .1f) : 2.25f;
    if (ScreamerTime >= EndTime) bScreamer = false;
}

void AMetalGameMode::UpdateCorScreamer(float Dt)
{
    AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!P || !Threats.IsValidIndex(0)) { bScreamer = false; return; }
    ScreamerTime += Dt;
    const float T = ScreamerTime;
    const float Rush = FMath::SmoothStep(0.f, .24f, T);
    const float Grab = FMath::SmoothStep(.18f, .42f, T);
    const float Snap = FMath::SmoothStep(1.02f, 1.25f, T);
    const FVector Right(-ScareDirection.Y, ScareDirection.X, 0);
    FVector Close = ScareCameraStart + ScareDirection * FMath::Lerp(72.f, 39.f, Snap);
    Close.Z = ScareCameraStart.Z - 85.f + Grab * 12.f;
    AActor* Robot = Threats[0].Robot;
    Robot->SetActorLocation(FMath::Lerp(ScareRobotStart, Close, Rush));
    Robot->SetActorRotation(FRotator(0, ScareDirection.Rotation().Yaw - 90.f, 0));
    // The raised pincer swings forward around its shoulder to seize the viewer.
    CorArm->SetRelativeRotation(FRotator(Grab * 28.f, -Grab * 62.f, 0));
    CorHead->SetRelativeRotation(FRotator(0, Grab * FMath::Sin(T * 24.f) * 7.f, Grab * 12.f - Snap * 30.f));
    FVector Camera = ScareCameraStart + FVector(0, 0, Grab * 12.f);
    Camera += Right * (Grab * FMath::Sin(T * 48.f) * 1.3f + Snap * 8.f);
    const FVector Face = Robot->GetActorLocation() + FVector(0, 0, 88);
    FRotator Look = (Face - Camera).Rotation();
    Look.Roll = Grab * FMath::Sin(T * 30.f) * 5.f + Snap * 32.f;
    Look.Yaw += Snap * 22.f;
    P->Camera->SetWorldLocation(Camera);
    P->Camera->SetWorldRotation(FQuat::Slerp(ScareRotationStart, Look.Quaternion(), FMath::Clamp(T / .16f, 0.f, 1.f)));
    P->Camera->FieldOfView = FMath::Lerp(85.f, 96.f, Grab);
    if (T >= ScreamerBlackoutAt && !bScareImpact)
    {
        bScareImpact = true;
        PlayScareSound(true);
        P->Torch->SetVisibility(false);
    }
    const float End = WindowScream ? FMath::Max(1.7f, WindowScream->Duration + .1f) : 1.7f;
    if (T >= End) bScreamer = false;
}

void AMetalGameMode::SetClosetOpen(bool Open)
{
    bClosetOpen = Open;
    ClosetLeft->SetActorLocation(Open ? FVector(179, 64, 125) : FVector(255, 140, 125));
    ClosetRight->SetActorLocation(Open ? FVector(179, 376, 125) : FVector(255, 300, 125));
    ClosetLeft->SetActorRotation(FRotator(0, Open ? 90.f : 0.f, 0));
    ClosetRight->SetActorRotation(FRotator(0, Open ? -90.f : 0.f, 0));
}

void AMetalGameMode::TryHide(AActor* Item)
{
    if (bHiding || (Item && Item->ActorHasTag(TEXT("Closet")))) ToggleHiding();
}

void AMetalGameMode::UpdateClosetRobot(float Dt)
{
    if (Hour < 2) { ClosetRobot->SetActorHiddenInGame(true); return; }
    ClosetRobot->SetActorHiddenInGame(false);
    bool Lit = false;
    if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); P && bClosetOpen && !bHiding && P->Torch->IsVisible())
    {
        const FVector Face = ClosetRobot->GetActorLocation() + FVector(-28, 0, 40);
        const FVector Aim = Face - P->Camera->GetComponentLocation();
        if (Aim.Size() < 400.f && FVector::DotProduct(Aim.GetSafeNormal(), P->Camera->GetForwardVector()) > .94f)
        {
            FHitResult Hit;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(P);
            Lit = !GetWorld()->LineTraceSingleByChannel(Hit, P->Camera->GetComponentLocation(), Face, ECC_Visibility, Params);
        }
    }
    const float Advance = (.018f + (Hour - 2) * .003f) * (bHiding ? .4f : 1.f);
    ClosetProgress = FMath::Clamp(ClosetProgress + (Lit ? -.32f : Advance) * Dt, 0.f, 1.f);
    ClosetRobot->SetActorLocation(FVector(FMath::Lerp(315.f, 238.f, ClosetProgress), 220, 125));
    const int32 Stage = FMath::FloorToInt(ClosetProgress * 4.f);
    if (Stage > ClosetWarnStage)
    {
        const FVector Sound = ClosetRobot->GetActorLocation();
        PlayTone(145.f - Stage * 20.f, .4f, .55f, &Sound);
        ShowNotice(Stage >= 3 ? TEXT("LE PLACARD S'AGITE ! Ouvre et eclaire son visage !") : TEXT("Quelque chose avance dans le placard..."), 4);
    }
    ClosetWarnStage = Stage;
    if (Lit) LearnMonster(3, ClosetProgress < .05f ? 3 : 1);
    if (Stage >= 2) LearnMonster(3, 2);
    if (ClosetProgress >= 1.f) Fail(TEXT("PLACARD"));
}

void AMetalGameMode::ToggleHiding()
{
    if (bDead || bWon || bPaused || bCarryingStool) return;
    auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!P) return;
    bHiding = !bHiding;
    P->GetCharacterMovement()->StopMovementImmediately();
    P->Torch->SetVisibility(false);
    P->Camera->SetRelativeLocation(FVector(0, 0, 65));
    if (bHiding)
    {
        SetClosetOpen(false);
        HideReturn = P->GetActorLocation();
        P->SetActorLocation(FVector(215, 220, 90));
        P->Controller->SetControlRotation(FRotator(0, 180, 0));
        bDetector = false;
        ShowNotice(TEXT("Cache dans l'armoire. Attends que les bruits s'eloignent."), 5);
    }
    else
    {
        P->SetActorLocation(HideReturn);
        if (VentStage == 4) Fail(TEXT("CONDUIT"));
    }
}

void AMetalGameMode::UpdateVentRobot(float Dt)
{
    if (Hour < 1)
    {
        VentStage = 0;
        VentRobot->SetActorHiddenInGame(true);
        return;
    }
    VentTimer -= Dt;
    if (VentStage == 1)
        VentRobot->SetActorLocation(FVector(-60, FMath::Lerp(-45.f, 220.f, FMath::Clamp(VentTimer / 12.f, 0.f, 1.f)), 365));
    if (VentStage == 3)
        VentRobot->SetActorLocation(FVector(-60, -60, FMath::Lerp(65.f, 365.f, FMath::Clamp(VentTimer / 1.3f, 0.f, 1.f))));
    if (VentStage == 4 && !bHiding) { Fail(TEXT("CONDUIT")); return; }
    if (VentTimer > 0) return;
    const FVector Sound(-60, -60, 290);
    if (VentStage == 0)
    {
        VentStage = 1; VentTimer = 12.f;
        LearnMonster(2, 1);
        VentRobot->SetActorLocation(FVector(-60, 220, 365));
        VentRobot->SetActorRotation(FRotator::ZeroRotator);
        VentRobot->SetActorHiddenInGame(false);
        PlayTone(180, .6f, .6f, &Sound);
        ShowNotice(TEXT("Des griffes raclent le conduit... Le marchepied permet de regarder."), 5);
    }
    else if (VentStage == 1)
    {
        VentStage = 2; VentTimer = Metal::VentReactionSeconds(Hour);
        LearnMonster(2, 2);
        VentRobot->SetActorLocation(FVector(-60, -45, 365));
        ShowNotice(FString::Printf(TEXT("IL VA DESCENDRE ! Armoire / %.1f secondes."), VentTimer), VentTimer);
        PlayTone(95, .8f, .75f, &Sound);
    }
    else if (VentStage == 2)
    {
        VentStage = 3; VentTimer = 1.3f;
        bVentOpen = true;
        VentGrille->SetActorLocation(FVector(85, -124, 270));
    }
    else if (VentStage == 3)
    {
        VentStage = 4; VentTimer = 6.f;
        VentRobot->SetActorLocation(FVector(-60, -60, 65));
        if (!bHiding) Fail(TEXT("CONDUIT"));
        else ShowNotice(TEXT("Il cherche dans la chambre... Reste cache."), 6);
    }
    else
    {
        VentStage = 0; VentTimer = FMath::RandRange(40.f, 58.f) / Metal::ThreatSpeed(Hour);
        LearnMonster(2, 3);
        VentRobot->SetActorHiddenInGame(true);
        ShowNotice(TEXT("Les griffes s'eloignent dans le conduit. Tu peux sortir."), 5);
        PlayTone(270, .4f, .3f, &Sound);
    }
}

void AMetalGameMode::UpdateVentScare(float Dt)
{
    auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!P) { bScreamer = false; return; }
    ScreamerTime += Dt;
    const float T = ScreamerTime;
    const FVector Direction = (ScareRobotStart - ScareCameraStart).GetSafeNormal2D();
    const float Leap = FMath::SmoothStep(0.f, .3f, T);
    const float Fall = FMath::SmoothStep(.45f, 1.3f, T);
    FVector Camera = FMath::Lerp(ScareCameraStart, FVector(0, -50, 25), Fall);
    FVector Body = FMath::Lerp(ScareRobotStart, Camera + Direction * FMath::Lerp(105.f, 78.f, Fall), Leap);
    Body.Z += FMath::Sin(Leap * PI) * 80.f;
    AActor* Attacker = ScareLane == 4 ? ClosetRobot.Get() : VentRobot.Get();
    if (ScareLane == 4) Body.Z -= 35.f;
    Attacker->SetActorHiddenInGame(false);
    Attacker->SetActorLocation(Body);
    Attacker->SetActorRotation(FRotator(0, Direction.Rotation().Yaw - (ScareLane == 4 ? 0.f : 90.f), 0));
    FRotator Look = (Body - Camera).Rotation();
    Look.Roll = Fall * 24 + FMath::Sin(T * 70) * Leap * 3;
    P->Camera->SetWorldLocation(Camera);
    P->Camera->SetWorldRotation(FQuat::Slerp(ScareRotationStart, Look.Quaternion(), Leap));
    if (T >= ScreamerBlackoutAt && !bScareImpact) { bScareImpact = true; PlayScareSound(true); P->Torch->SetVisibility(false); }
    if (T >= (WindowScream ? FMath::Max(2.f, WindowScream->Duration + .1f) : 2.f)) bScreamer = false;
}

void AMetalGameMode::ResetStool()
{
    bCarryingStool = false;
    Stool->SetActorLocation(FVector(-260, -120, 38));
    Stool->SetActorRotation(FRotator::ZeroRotator);
    Stool->SetActorEnableCollision(true);
    for (AActor* Place : { StoolHome.Get(), StoolVent.Get() })
    {
        Place->SetActorHiddenInGame(true);
        Place->SetActorEnableCollision(false);
    }
}

void AMetalGameMode::RetryHour()
{
    if (bScreamer) return;
    if (!Checkpoint) return;
    if (IsValid(ActiveScream)) ActiveScream->Stop();
    ActiveScream = nullptr;
    bHiding = false;
    SetClosetOpen(false);
    ClosetProgress = 0;
    ClosetRobot->SetActorRotation(FRotator::ZeroRotator);
    VentStage = 0; VentTimer = 45.f;
    VentRobot->SetActorHiddenInGame(true);
    ResetStool();
    bDead = bWon = bPaused = bDocked = bDetector = false;
    bJournalOpen = false;
    bComputerOpen = false;
    DeathScreenTime = 0;
    bVentOpen = false;
    VentGrille->SetActorLocation(FVector(-60, -124, 270));
    Door->SetActorHiddenInGame(false);
    CorHead->SetRelativeRotation(FRotator::ZeroRotator);
    CorArm->SetRelativeRotation(FRotator::ZeroRotator);
    bWelcome = false;
    SyncMenuInput();
    Target = Candidate = INDEX_NONE;
    PowerLevel = 0;
    Battery = 100;
    Hour = Checkpoint->Hour;
    DoorHealth = Checkpoint->DoorHealth;
    Oxygen = Checkpoint->Oxygen;
    bTopClosed = Checkpoint->TopClosed;
    bBottomClosed = Checkpoint->BottomClosed;
    TopWindow->SetActorLocation(FVector(bTopClosed ? 0 : 162, -490, 155));
    BottomWindow->SetActorLocation(FVector(bBottomClosed ? 0 : 162, 490, 155));
    DockDevice->SetActorHiddenInGame(true);
    if (AMetalCharacter* Pawn = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Pawn->SetActorLocation(FVector(70, -235, 90), false, nullptr, ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->Camera->bUsePawnControlRotation = true;
        Pawn->Camera->SetRelativeLocation(FVector(0, 0, 65));
        Pawn->Camera->SetRelativeRotation(FRotator::ZeroRotator);
        Pawn->Camera->FieldOfView = 85.f;
        Pawn->Torch->SetIntensity(22000.f);
        Pawn->Torch->SetVisibility(false);
        if (Pawn->Controller) Pawn->Controller->SetControlRotation(FRotator(-10, -65, 0));
    }
    Intermission = 0;
    StartHour();
}

void AMetalGameMode::StartNewGame()
{
    // The N shortcut also fires while the terminal receives typed letters.
    if (bComputerOpen || bJournalOpen) return;
    Checkpoint = NewObject<UMetalCheckpoint>(this);
    RetryHour();
    Intermission = 10.f;
    for (FMetalThreat& T : Threats) T.Robot->SetActorHiddenInGame(true);
    RoomLights[0]->SetIntensity(20000);
    RoomLights[1]->SetIntensity(650);
    RoomLights[2]->SetIntensity(650);
    ShowNotice(TEXT("Nouvelle partie / Jack termine son prototype."), 7.f);
}

void AMetalGameMode::SetPaused()
{
    if (bComputerOpen) { CloseComputer(); return; }
    if (bJournalOpen) { ToggleJournal(); return; }
    if (bDead || bWon) return;
    bPaused = !bPaused;
    if (!bPaused) bWelcome = false;
    SyncMenuInput();
    if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) P->GetCharacterMovement()->StopMovementImmediately();
}

void AMetalGameMode::SyncMenuInput()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        const bool MenuInput = bPaused || (bDead && !bScreamer);
        PC->bShowMouseCursor = MenuInput;
        PC->bEnableClickEvents = MenuInput;
        PC->bEnableMouseOverEvents = MenuInput;
        if (MenuInput)
        {
            FInputModeGameAndUI Mode;
            Mode.SetHideCursorDuringCapture(false);
            Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Mode);
        }
        else PC->SetInputMode(FInputModeGameOnly());
    }
}

void AMetalGameMode::CloseComputer()
{
    bComputerOpen = false;
    bPaused = false;
    SyncMenuInput();
}

void AMetalGameMode::TypeComputerKey(const FKey& Key)
{
    if (!bComputerOpen) return;
    if (Key == EKeys::BackSpace) { ComputerText = ComputerText.LeftChop(1); return; }
    if (Key == EKeys::Enter)
    {
        ComputerMessage = TEXT("Commande inconnue."); ComputerText.Empty(); return;
    }
    const FString Letter = Key.GetFName().ToString().ToLower();
    if (Letter.Len() != 1 || Letter[0] < 'a' || Letter[0] > 'z' || ComputerText.Len() >= 40) return;
    ComputerText += Letter;
    if (ComputerText != TEXT("moonlight")) return;
    if (JournalData)
    {
        JournalData->bMoonlightUnlocked = true;
        if (!bSmoke && !bPreview) UGameplayStatics::SaveGameToSlot(JournalData, TEXT("MEtAL_Journal"), 0);
    }
    CloseComputer();
    ToggleJournal();
    if (auto* PC = UGameplayStatics::GetPlayerController(this,0))
        if (auto* HUD = Cast<AMetalHUD>(PC->GetHUD())) HUD->SelectMoonEntry();
    PlayTone(660.f,.45f,.18f);
    ShowNotice(TEXT("Une page oubliee vient de reapparaitre."),4.f);
}

void AMetalGameMode::LearnMonster(int32 Index, int32 Level)
{
    if (!JournalData || !JournalData->Knowledge.IsValidIndex(Index)) return;
    Level = FMath::Clamp(Level, 0, 3);
    if (Level <= JournalData->Knowledge[Index]) return;
    JournalData->Knowledge[Index] = Level;
    ShowNotice(TEXT("Nouvelle note dans le carnet / J pour lire."), 3);
    if (!bSmoke && !bPreview) UGameplayStatics::SaveGameToSlot(JournalData, TEXT("MEtAL_Journal"), 0);
}

void AMetalGameMode::ToggleJournal()
{
    if (bScreamer || bComputerOpen) return;
    if (!bJournalOpen)
    {
        bPausedBeforeJournal = bPaused;
        bPaused = true;
        bJournalOpen = true;
        if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) P->GetCharacterMovement()->StopMovementImmediately();
    }
    else { bJournalOpen = false; bPaused = bPausedBeforeJournal; }
    SyncMenuInput();
}

void AMetalGameMode::ReturnToMenu()
{
    if (bScreamer) return;
    RetryHour();
    bWelcome = bPaused = true;
    SyncMenuInput();
}

void AMetalGameMode::UpdateMenuMusic()
{
    if (bSmoke) return;
    if (!bWelcome || !bPaused)
    {
        if (IsValid(MenuMusic)) MenuMusic->Stop();
        MenuMusic = nullptr;
        return;
    }
    if (!MenuMusicWave)
    {
        constexpr int32 Rate = 22050, Count = Rate * 16;
        MenuMusicPCM.SetNumUninitialized(Count * 2);
        const float Notes[] = { 220.f, 261.6256f, 329.6276f, 293.6648f, 220.f, 196.f, 164.8138f, 196.f };
        for (int32 I = 0; I < Count; ++I)
        {
            const float T = float(I) / Rate;
            const int32 N = FMath::Min(7, int32(T / 2));
            const float Local = FMath::Fmod(T, 2.f);
            const float Env = FMath::Min(Local / .08f, 1.f) * FMath::Exp(-Local * 2.6f);
            const float Pad = .12f * FMath::Sin(2*PI*55*T) + .06f * FMath::Sin(2*PI*82.5f*T) + .04f * FMath::Sin(2*PI*110*T);
            const float Bell = .22f * Env * (FMath::Sin(2*PI*Notes[N]*Local) + .22f*FMath::Sin(2*PI*Notes[N]*2*Local));
            const float Edge = FMath::Min(1.f, FMath::Min(T / .6f, (16-T) / .6f));
            const int16 Sample = int16((Pad + Bell) * Edge * 16000);
            FMemory::Memcpy(MenuMusicPCM.GetData() + I*2, &Sample, 2);
        }
        MenuMusicWave = NewObject<USoundWaveProcedural>(this);
        MenuMusicWave->SetSampleRate(Rate); MenuMusicWave->NumChannels = 1;
        MenuMusicWave->Duration = INDEFINITELY_LOOPING_DURATION;
    }
    if (!IsValid(MenuMusic))
    {
        MenuMusicWave->ResetAudio();
        MenuMusicWave->QueueAudio(MenuMusicPCM.GetData(), MenuMusicPCM.Num());
        MenuMusic = UGameplayStatics::SpawnSound2D(this, MenuMusicWave, .35f);
        UE_LOG(LogTemp, Display, TEXT("METAL_MENU_MUSIC: playing=%d"), IsValid(MenuMusic) && MenuMusic->IsPlaying());
    }
    if (MenuMusicWave->GetAvailableAudioByteCount() < 22050 * 4)
        MenuMusicWave->QueueAudio(MenuMusicPCM.GetData(), MenuMusicPCM.Num());
}

void AMetalGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    ElapsedReal += Dt;
    UpdateMenuMusic();
    if (bDead && !bScreamer)
    {
        if (DeathScreenTime == 0) SyncMenuInput();
        DeathScreenTime += Dt;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MetalStoolPreview")))
    {
        if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            if (ElapsedReal > 3.5f && ElapsedReal < 4.05f) P->AddMovementInput(FVector(0, 1, 0));
            if (ElapsedReal > 5.f && ElapsedReal - Dt <= 5.f)
            {
                UE_LOG(LogTemp, Display, TEXT("METAL_STOOL_WALK: %s position=%s"), P->GetMovementBase() && P->GetMovementBase()->GetOwner() == Stool ? TEXT("PASS") : TEXT("FAIL"), *P->GetActorLocation().ToString());
            }
        }
    }
    if (bSmoke && !bSmokeFinished && ElapsedReal > 1.f)
    {
        bSmokeFinished = true;
        RunSmokeTest();
        return;
    }
    if (bScarePreview)
    {
        if (!bCaptured && ElapsedReal > .5f)
        {
            bCaptured = true;
            const int32 Lane = bCorScarePreview ? 0 : 1;
            Threats[Lane].Stage = 4;
            MoveThreat(Threats[Lane]);
            if (FParse::Param(FCommandLine::Get(), TEXT("MetalVentScarePreview"))) VentRobot->SetActorLocation(FVector(-60, -60, 65));
            Fail(FParse::Param(FCommandLine::Get(), TEXT("MetalVentScarePreview")) ? TEXT("CONDUIT") : (bCorScarePreview ? TEXT("PORTE") : TEXT("FENETRE NORD")));
        }
        const float Shots[] = { .82f, 1.5f, 2.f, 3.6f };
        if (ScarePreviewFrame < 4 && ElapsedReal > Shots[ScarePreviewFrame])
        {
            ++ScarePreviewFrame;
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("HighResShot 1"));
        }
    }
    if (bPreview && !bScarePreview && !bCaptured && ElapsedReal > 3.f)
    {
        bCaptured = true;
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("HighResShot 1"));
    }
    if (bPreview && ElapsedReal > 8.f) { FGenericPlatformMisc::RequestExit(false); return; }
    if (bCorPreview) return;
    if (bScreamer) { UpdateScreamer(Dt); return; }
    if (bDead || bWon || bPaused) return;
    if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)); P && bVentOpen && !bHiding && P->GetMovementBase() && P->GetMovementBase()->GetOwner() == Stool && Stool->GetActorLocation().X == -60.f && P->Camera->GetRelativeLocation().Z < 100.f)
    {
        P->Camera->SetRelativeLocation(FVector(0, 0, 205));
        P->Controller->SetControlRotation(FRotator(10, 90, 0));
        ShowNotice(TEXT("Tu regardes dans le conduit. Recule pour redescendre."));
    }
    NoticeTime = FMath::Max(0.f, NoticeTime - Dt);
    if (Intermission > 0)
    {
        ClosetRobot->SetActorHiddenInGame(true);
        Intermission -= Dt;
        Battery = FMath::Min(100.f, Battery + 12.f * Dt);
        // Powered ventilation restores oxygen, while the damaged door stays damaged.
        Oxygen = FMath::Min(100.f, Oxygen + 10.f * Dt);
        for (FMetalThreat& T : Threats) T.Robot->SetActorHiddenInGame(true);
        if (Intermission <= 0) { Intermission = 0; StartHour(); }
        return;
    }
    Seconds += Dt;
    UpdateVentRobot(Dt);
    if (!bDead) UpdateClosetRobot(Dt);
    if (bDead) return;
    const int32 OpenWindows = (!bTopClosed ? 1 : 0) + (!bBottomClosed ? 1 : 0);
    const float OxygenRate = OpenWindows == 2 ? .75f : (OpenWindows == 1 ? -.19f : -.85f);
    Oxygen = FMath::Clamp(Oxygen + OxygenRate * Dt, 0.f, 100.f);
    const float Energy = bDocked ? .145f : (bDetector ? Metal::Drain[PowerLevel] : 0.f);
    Battery = FMath::Max(0.f, Battery - Energy * Dt);
    if (Battery <= 0) bDetector = false;
    UpdateDetector(Dt);
    if (DoorMaterial) DoorMaterial->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(FLinearColor(.55f, .04f, .025f), FLinearColor(.35f, .21f, .12f), DoorHealth / 100.f));
    if (Oxygen <= 0) { Fail(TEXT("OXYGENE")); return; }
    const float Speed = Metal::ThreatSpeed(Hour);
    for (FMetalThreat& T : Threats)
    {
        T.Timer -= Dt;
        if (T.Stage < 4 && T.Timer <= 0)
        {
            ++T.Stage;
            if (T.Stage >= 3) LearnMonster(T.Lane == 0 ? 0 : 1, 1);
            T.Timer = T.Stage == 2 ? 15.f / Speed : 10.f / Speed;
            MoveThreat(T);
            FVector Loc = T.Robot->GetActorLocation();
            PlayTone(T.Stage == 4 ? 120.f : 210.f, .25f, .65f, &Loc);
            if (T.Stage == 4)
            {
                LearnMonster(T.Lane == 0 ? 0 : 1, 2);
                T.StrikeTimer = T.Lane == 0 ? 3.f / Speed : Metal::WindowReactionSeconds;
                T.WaitTime = 0;
                if (T.Lane != 0)
                {
                    const bool Closed = T.Lane == 1 ? bTopClosed : bBottomClosed;
                    ShowNotice(Closed ? TEXT("Un robot attend devant une fenetre fermee...")
                        : (T.Lane == 1 ? TEXT("ROBOT / fenetre nord ! 8 secondes pour fermer.") : TEXT("ROBOT / fenetre sud ! 8 secondes pour fermer.")), Metal::WindowReactionSeconds);
                }
                // The reaction window starts at arrival, not before this frame.
                continue;
            }
        }
        if (T.Stage != 4) continue;
        T.WaitTime += Dt;
        if (T.Lane == 0)
        {
            T.StrikeTimer -= Dt;
            if (bDocked && Battery > 0)
            {
                if (T.WaitTime >= 5.f) { LearnMonster(0, 3); ShowNotice(TEXT("Le robot recule. La porte a tenu.")); ResetThreat(T, FMath::RandRange(24.f, 39.f) / Speed); }
            }
            else if (T.StrikeTimer <= 0)
            {
                T.StrikeTimer = 3.5f / Speed;
                DoorHealth = FMath::Max(0.f, DoorHealth - 12.f);
                FVector Loc = Door->GetActorLocation();
                PlayTone(70.f, .35f, .8f, &Loc);
                ShowNotice(TEXT("COUP SUR LA PORTE / Pose la toupie sur le socle !"));
                if (DoorHealth <= 0) { Fail(TEXT("PORTE")); return; }
            }
        }
        else
        {
            const bool Closed = T.Lane == 1 ? bTopClosed : bBottomClosed;
            T.StrikeTimer = Closed ? Metal::WindowReactionSeconds : T.StrikeTimer - Dt;
            if (!Closed && T.StrikeTimer <= 0) { Fail(T.Lane == 1 ? TEXT("FENETRE NORD") : TEXT("FENETRE SUD")); return; }
            if (Closed && T.WaitTime >= 22.f + Hour * 2.f)
            {
                LearnMonster(1, 3);
                ResetThreat(T, FMath::RandRange(30.f, 49.f) / Speed);
                ShowNotice(TEXT("Des pas s'eloignent de la fenetre..."));
            }
        }
    }
    if (Seconds >= 360.f)
    {
        ++Hour;
        VentStage = 0;
        VentRobot->SetActorHiddenInGame(true);
        bDetector = false;
        Target = Candidate = INDEX_NONE;
        if (Hour >= 6)
        {
            bWon = true;
            for (FMetalThreat& T : Threats) T.Robot->SetActorHiddenInGame(true);
            RoomLights[0]->SetIntensity(20000);
            if (!bSmoke && !bPreview) UGameplayStatics::DeleteGameInSlot(Metal::SaveSlot, 0);
            return;
        }
        Intermission = 12.f;
        RoomLights[0]->SetIntensity(20000);
        RoomLights[1]->SetIntensity(650);
        RoomLights[2]->SetIntensity(650);
        ShowNotice(TEXT("COURANT RETABLI / Recharge. Les degats de la porte restent."), 10.f);
    }
}

void AMetalGameMode::RunSmokeTest()
{
    bool Success = true;
    auto Check = [&Success](bool Condition, const TCHAR* Name)
    {
        UE_LOG(LogTemp, Display, TEXT("METAL_TEST %s : %s"), Name, Condition ? TEXT("PASS") : TEXT("FAIL"));
        Success &= Condition;
    };
    Check(Threats.Num() == 3 && Dock && Door && TopWindow && BottomWindow, TEXT("room and interactables"));
    Check(JournalData && JournalData->Knowledge.Num()==4 && JournalData->Knowledge[0]==0, TEXT("journal starts with locked dossiers"));
    TypeComputerKey(EKeys::M);
    Check(!JournalData->bMoonlightUnlocked && ComputerText.IsEmpty(),TEXT("secret cannot be typed outside computer terminal"));
    Use(Computer);
    Check(bComputerOpen && bPaused,TEXT("interacting with desk computer opens typing session"));
    const int32 HourBeforeTyping = Hour;
    Hour = 2;
    bool ExercisedNewGameShortcut = false;
    if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); P && P->InputComponent)
    {
        for (int32 I=0; I<P->InputComponent->GetNumActionBindings(); ++I)
        {
            FInputActionBinding& Binding = P->InputComponent->GetActionBinding(I);
            if (Binding.GetActionName() == TEXT("NewGame"))
            {
                Binding.ActionDelegate.Execute(EKeys::N);
                ExercisedNewGameShortcut = true;
            }
        }
    }
    Check(ExercisedNewGameShortcut && Hour==2 && bComputerOpen && bPaused,TEXT("N action cannot reset hour or kick player from computer"));
    Hour = HourBeforeTyping;
    for(TCHAR Letter:FString(TEXT("moonligx")))TypeComputerKey(FKey(FName(*FString::Chr(FChar::ToUpper(Letter)))));
    Check(!JournalData->bMoonlightUnlocked,TEXT("incorrect computer command does not unlock secret"));
    TypeComputerKey(EKeys::BackSpace); TypeComputerKey(EKeys::H); TypeComputerKey(EKeys::T);
    Check(JournalData->bMoonlightUnlocked && bJournalOpen && !bComputerOpen,TEXT("moonlight unlocks and opens secret journal entry"));
    ToggleJournal();
    TArray<uint8> SavedJournal;
    UGameplayStatics::SaveGameToMemory(JournalData,SavedJournal);
    UMetalJournal* RestoredJournal=Cast<UMetalJournal>(UGameplayStatics::LoadGameFromMemory(SavedJournal));
    Check(RestoredJournal && RestoredJournal->bMoonlightUnlocked,TEXT("moonlight discovery survives save serialization"));
    LearnMonster(0,1); LearnMonster(0,2); LearnMonster(0,1);
    Check(JournalData->Knowledge[0]==2, TEXT("journal unlocks progressively without losing notes"));
    ToggleJournal();
    const float BeforeReading = Seconds;
    Tick(1);
    Check(bJournalOpen && bPaused && Seconds==BeforeReading, TEXT("journal pauses active gameplay"));
    ToggleJournal();
    Check(!bPaused && !bJournalOpen, TEXT("closing journal restores previous play state"));
    SetPaused(); ToggleJournal(); ToggleJournal();
    Check(bPaused, TEXT("journal preserves previously paused menu"));
    SetPaused();
    Check(Threats[0].Robot->ActorHasTag(TEXT("Cor")) && !Threats[1].Robot->ActorHasTag(TEXT("Cor")) && !Threats[2].Robot->ActorHasTag(TEXT("Cor")), TEXT("Cor replaces only the door robot"));
    Check(WindowScream && WindowScream->Duration > 0 && WindowScream->NumChannels > 0, TEXT("supplied screamer audio loads"));
    VentTimer = 0;
    Hour = 0;
    UpdateVentRobot(360.f);
    Check(VentStage == 0 && VentRobot->IsHidden() && !bDead, TEXT("vent robot cannot arrive during first hour"));
    Hour = 1;
    UpdateVentRobot(.1f);
    Check(VentStage == 1 && !VentRobot->IsHidden(), TEXT("vent robot can arrive starting second hour"));
    UpdateVentRobot(12.f);
    Check(VentStage == 2 && VentTimer == Metal::VentReactionSeconds(Hour), TEXT("vent warning uses current hour reaction time"));
    for (int32 H = 1; H < 6; ++H)
        Check(Metal::VentReactionSeconds(H) < Metal::VentReactionSeconds(H - 1), TEXT("vent reaction time decreases every hour"));
    Check(Metal::VentReactionSeconds(0) == 6.f && Metal::VentReactionSeconds(5) == 2.f, TEXT("vent warning ranges from six to two seconds"));
    ToggleHiding();
    UpdateVentRobot(Metal::VentReactionSeconds(Hour)); UpdateVentRobot(1.3f);
    Check(bHiding && !bDead && VentStage == 4, TEXT("wardrobe protects against vent descent"));
    UpdateVentRobot(6.f);
    ToggleHiding();
    Check(!bDead && !bHiding && VentStage == 0, TEXT("safe exit after vent robot departure"));
    VentStage = 3; VentTimer = 0;
    UpdateVentRobot(.1f);
    Check(bDead && bScreamer && ScareLane == 3, TEXT("unhidden player receives vent screamer"));
    UpdateScreamer(3.f); RetryHour();
    const float BeforeVentOxygen = Oxygen;
    Hour = 1;
    UpdateClosetRobot(10);
    Check(ClosetRobot->IsHidden(), TEXT("closet robot absent before third hour"));
    Hour = 2; ClosetProgress = .5f;
    if (auto* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        P->SetActorLocation(FVector(100, 220, 90));
        P->Camera->bUsePawnControlRotation = false;
        P->Torch->SetVisibility(true);
        SetClosetOpen(true);
        UpdateClosetRobot(0);
        P->Camera->SetWorldRotation((ClosetRobot->GetActorLocation() + FVector(-28, 0, 40) - P->Camera->GetComponentLocation()).Rotation());
        UpdateClosetRobot(1);
        Check(ClosetProgress < .3f && !ClosetRobot->IsHidden(), TEXT("aimed flashlight repels third hour closet robot"));
        SetClosetOpen(false);
        const float ClosedProgress = ClosetProgress;
        UpdateClosetRobot(1);
        Check(ClosetProgress > ClosedProgress, TEXT("closed doors block flashlight and do not repel robot"));
        SetClosetOpen(true);
        P->Camera->SetWorldRotation(FRotator(0, 180, 0));
        const float AwayProgress = ClosetProgress;
        UpdateClosetRobot(1);
        Check(ClosetProgress > AwayProgress, TEXT("flashlight must point toward closet robot"));
        ClosetProgress = .999f; UpdateClosetRobot(1);
        Check(bDead && bScreamer && ScareLane == 4, TEXT("neglected closet robot triggers death"));
        UpdateScreamer(3); RetryHour();
    }
    Use(Stool);
    Check(bCarryingStool && !Stool->GetActorEnableCollision() && !StoolVent->IsHidden(), TEXT("stool pickup enables two placement targets"));
    Use(Dock);
    Check(bCarryingStool && !bDocked, TEXT("carried stool cannot be placed on arbitrary interactable"));
    Use(StoolVent);
    Check(!bCarryingStool && Stool->GetActorLocation().Equals(FVector(-60, -60, 38)) && Stool->GetActorEnableCollision(), TEXT("stool places exactly under vent with collision"));
    Use(Stool);
    Use(StoolHome);
    Check(!bCarryingStool && Stool->GetActorLocation().Equals(FVector(-260, -120, 38)), TEXT("stool returns only to its home slot"));
    if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        Check(P->GetCharacterMovement()->MaxStepHeight >= 42.f, TEXT("stool top is low enough to walk up"));
    Use(Vent);
    Check(bVentOpen && VentGrille->GetActorLocation().X == 85.f && Oxygen == BeforeVentOxygen && Threats.Num() == 3, TEXT("vent inspection opens without adding combat or changing oxygen"));
    Use(Vent);
    Check(!bVentOpen && VentGrille->GetActorLocation().X == -60.f, TEXT("vent grille closes again"));
    Check(Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)) != nullptr, TEXT("possessed first-person player"));
    if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        const FRotator Aim = (Dock->GetActorLocation() - P->Camera->GetComponentLocation()).Rotation();
        P->Camera->bUsePawnControlRotation = false;
        P->Camera->SetWorldRotation(Aim);
        Check(P->FindInteraction() == Dock, TEXT("player can aim at and reach desk dock"));
        const FVector SavedCamera = P->Camera->GetComponentLocation();
        P->Camera->SetWorldLocation(FVector(-60, -100, 155));
        P->Camera->SetWorldRotation((Vent->GetActorLocation() - P->Camera->GetComponentLocation()).Rotation());
        Check(P->FindInteraction() == Vent, TEXT("ceiling vent can be reached with interaction trace"));
        Use(Vent);
        Check(P->FindInteraction() == Vent, TEXT("open vent remains reachable to close"));
        Use(Vent);
        P->Camera->SetWorldLocation(SavedCamera);
        P->Camera->bUsePawnControlRotation = true;
    }
    Intermission = 0;
    StartHour();
    Use(Dock);
    bDetector = true;
    UpdateDetector(5);
    Check(bDocked && Target == INDEX_NONE, TEXT("docking prevents scanning"));
    Use(Dock);
    bDetector = true;
    PowerLevel = 3;
    for (FMetalThreat& T : Threats) { T.Stage = 1; T.Robot->SetActorLocation(FVector(100, -200, 105)); }
    UpdateDetector(5);
    Check(Target == INDEX_NONE, TEXT("first-stage robots ignored even nearby at max power"));
    Threats[1].Stage = 2;
    Threats[1].Robot->SetActorLocation(FVector(0, -600, 105));
    UpdateDetector(5);
    Check(Target == 1, TEXT("advanced nearby robot acquired through wall"));
    Threats[1].Robot->SetActorLocation(FVector(5000, 0, 105));
    UpdateDetector(5);
    Check(Target == INDEX_NONE, TEXT("out-of-range robot ignored"));
    SetPower(true);
    Check(PowerLevel == 0, TEXT("double-click resets power"));
    Use(TopWindow);
    Use(BottomWindow);
    Check(bTopClosed && bBottomClosed, TEXT("both windows close via interaction"));
    Oxygen = 100;
    const float OldBattery = Battery;
    Tick(1.f);
    Check(Oxygen < 100 && Battery < OldBattery, TEXT("closed windows reduce oxygen and detector drains battery"));
    DoorHealth = 64;
    SaveCheckpoint();
    DoorHealth = 28;
    bDead = true;
    RetryHour();
    Check(DoorHealth == 64 && !bDead && Seconds == 0, TEXT("retry restores hour-start checkpoint"));
    DoorHealth = 52;
    Seconds = 359.9f;
    Tick(.2f);
    Tick(12.1f);
    Check(Hour == 1 && DoorHealth == 52 && Battery == 100, TEXT("next hour recharges without repairing door"));
    for (FMetalThreat& T : Threats) ResetThreat(T, 1000.f);
    bTopClosed = false;
    Threats[1].Stage = 3;
    Threats[1].Timer = 0;
    Tick(.1f);
    Check(!bDead && Threats[1].StrikeTimer == Metal::WindowReactionSeconds, TEXT("window arrival grants full reaction time"));
    Tick(7.f);
    Check(!bDead, TEXT("window robot does not kill before eight seconds"));
    Use(TopWindow);
    Tick(2.f);
    Check(!bDead && Threats[1].StrikeTimer == Metal::WindowReactionSeconds, TEXT("closing window interrupts attack"));
    Use(TopWindow);
    Tick(7.f);
    Check(!bDead, TEXT("reopening occupied window grants fresh reaction time"));
    Tick(1.1f);
    Check(bDead && DeathReason == TEXT("FENETRE NORD"), TEXT("open window still lethal after reaction time"));
    Check(bScreamer, TEXT("window death starts screamer"));
    const int32 DeathHour = Hour;
    RetryHour();
    Check(bScreamer && bDead && Hour == DeathHour, TEXT("retry blocked during screamer"));
    UpdateScreamer(.3f);
    Check(FVector::Dist2D(Threats[ScareLane].Robot->GetActorLocation(), ScareCameraStart) < 80.f, TEXT("robot rush reaches player within 0.3 seconds"));
    UpdateScreamer(.7f);
    if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        Check(P->Camera->GetComponentLocation().Z < 40.f, TEXT("screamer throws camera to floor"));
    UpdateScreamer(3.f);
    Check(!bScreamer && bDead && bScareImpact, TEXT("screamer impact finishes before death screen"));
    RetryHour();
    if (AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        Check(P->Camera->bUsePawnControlRotation && P->Camera->GetRelativeLocation().Equals(FVector(0, 0, 65)), TEXT("retry restores first-person camera"));
    Fail(TEXT("FENETRE SUD"));
    Check(bScreamer && ScareLane == 2, TEXT("south window also triggers correct robot"));
    UpdateScreamer(4.f);
    RetryHour();
    Fail(TEXT("PORTE"));
    Check(bScreamer && ScareLane == 0 && Door->IsHidden(), TEXT("Cor door death starts dedicated screamer"));
    UpdateScreamer(.3f);
    Check(FVector::Dist2D(Threats[0].Robot->GetActorLocation(), ScareCameraStart) < 80.f && !CorArm->GetRelativeRotation().IsNearlyZero(), TEXT("Cor rushes and reaches toward camera"));
    UpdateScreamer(1.f);
    Check(bScareImpact && bScreamer, TEXT("Cor impact blackens before audio finishes"));
    UpdateScreamer(3.f);
    RetryHour();
    Check(!bDead && !Door->IsHidden() && CorHead->GetRelativeRotation().IsNearlyZero() && CorArm->GetRelativeRotation().IsNearlyZero(), TEXT("retry restores Cor pose and door"));
    bDead = false;
    SetPaused();
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    Check(PC && PC->bShowMouseCursor && PC->bEnableClickEvents, TEXT("pause enables visible clickable cursor"));
    const float PausedSeconds = Seconds;
    Tick(2.f);
    Check(Seconds == PausedSeconds, TEXT("menu freezes game clock"));
    if (AMetalHUD* HUD = PC ? Cast<AMetalHUD>(PC->GetHUD()) : nullptr)
    {
        HUD->NotifyHitBoxClick(TEXT("Resume"));
        Check(!bPaused && !PC->bShowMouseCursor, TEXT("resume button restores game input"));
        SetPaused();
        HUD->NotifyHitBoxClick(TEXT("NewGame"));
        Check(Hour == 0 && !bPaused && !PC->bShowMouseCursor, TEXT("new game button resets game and hides cursor"));
    }
    else Check(false, TEXT("menu HUD exists"));
    float PreviousApproach = 1000.f;
    for (int32 TestHour = 0; TestHour < 6; ++TestHour)
    {
        Hour = TestHour;
        Intermission = 0;
        StartHour();
        float ApproachTime = 0.f;
        while (Threats[0].Stage < 4 && ApproachTime < 60.f && !bDead)
        {
            Tick(.1f);
            ApproachTime += .1f;
        }
        Check(Threats[0].Stage == 4 && ApproachTime < PreviousApproach, TEXT("robots approach faster each successive hour"));
        UE_LOG(LogTemp, Display, TEXT("METAL_DIFFICULTY hour %d: door arrival %.1f seconds, speed x%.2f"), Hour + 1, ApproachTime, Metal::ThreatSpeed(Hour));
        PreviousApproach = ApproachTime;
    }
    Fail(TEXT("OXYGENE")); Tick(.1f);
    Check(PC && PC->bShowMouseCursor && PC->bEnableClickEvents, TEXT("death screen enables clickable buttons"));
    if (auto* HUD = PC ? Cast<AMetalHUD>(PC->GetHUD()) : nullptr)
    {
        HUD->NotifyHitBoxClick(TEXT("RetryDeath"));
        Check(!bDead && !PC->bShowMouseCursor, TEXT("death retry button resumes checkpoint"));
        Fail(TEXT("OXYGENE"));
        HUD->NotifyHitBoxClick(TEXT("DeathMenu"));
        Check(!bDead && bWelcome && bPaused && PC->bShowMouseCursor, TEXT("death menu button opens welcome menu"));
    }
    UE_LOG(LogTemp, Display, TEXT("METAL_SMOKE_RESULT: %s"), Success ? TEXT("PASS") : TEXT("FAIL"));
    FGenericPlatformMisc::RequestExitWithStatus(false, Success ? 0 : 1);
}

void AMetalHUD::Stroke(float X1, float Y1, float X2, float Y2, FLinearColor Color, float Thickness)
{
    if (FMath::Abs(Y2 - Y1) < .1f)
    {
        DrawRect(Color, FMath::Min(X1, X2), Y1 - Thickness / 2, FMath::Abs(X2 - X1) + Thickness, Thickness);
        return;
    }
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D(X2 - X1, Y2 - Y1).Size()));
    for (int32 I = 0; I <= Steps; ++I)
    {
        const float T = float(I) / Steps;
        DrawRect(Color, FMath::Lerp(X1, X2, T) - Thickness / 2, FMath::Lerp(Y1, Y2, T) - Thickness / 2, Thickness, Thickness);
    }
}

void AMetalHUD::TextAt(const FString& Text, float X, float Y, float Scale, FLinearColor Color)
{
    DrawText(Text, Color, X, Y, GEngine->GetLargeFont(), Scale * 1.65f, false);
}

void AMetalHUD::Bar(const FString& Label, float Value, float X, float Y, float Width, FLinearColor Color)
{
    TextAt(FString::Printf(TEXT("%s  %03d%%"), *Label, FMath::CeilToInt(Value)), X, Y, .85f, FLinearColor(.8f, .86f, .88f));
    DrawRect(FLinearColor(.06f, .075f, .085f, .92f), X, Y + 25, Width, 7);
    DrawRect(Value < 25 ? FLinearColor(1.f, .23f, .15f) : Color, X, Y + 25, Width * Value / 100.f, 7);
}

void AMetalHUD::DrawMainMenu(AMetalGameMode* G)
{
    const float S = FMath::Min(Canvas->SizeX / 1280.f, Canvas->SizeY / 720.f);
    const float OX = (Canvas->SizeX - 1280.f * S) / 2.f;
    const float OY = (Canvas->SizeY - 720.f * S) / 2.f;
    const FLinearColor Ink(.012f, .018f, .026f), Pale(.86f, .88f, .84f), Dim(.38f, .46f, .49f), Accent(.92f, .55f, .22f);
    auto Rect = [&](float X, float Y, float W, float H, FLinearColor C) { DrawRect(C, OX + X*S, OY + Y*S, W*S, H*S); };
    auto Label = [&](const FString& T, float X, float Y, float Scale, FLinearColor C) { TextAt(T, OX + X*S, OY + Y*S, Scale*S, C); };
    DrawRect(Ink, 0, 0, Canvas->SizeX, Canvas->SizeY);
    Rect(680, 0, 600, 720, FLinearColor(.02f, .031f, .043f));
    // A spare illustration of the room: moonlit wall, open door and watching eyes.
    Rect(854, 180, 242, 386, FLinearColor(.075f, .10f, .12f));
    Rect(867, 192, 216, 374, FLinearColor(.004f, .006f, .009f));
    Rect(873, 200, 12, 350, FLinearColor(.20f, .16f, .11f));
    Rect(890, 555, 330, 2, Dim);
    Rect(963, 330, 54, 145, FLinearColor(.016f, .023f, .028f));
    Rect(948, 293, 86, 61, FLinearColor(.025f, .032f, .038f));
    const float Pulse = .6f + .25f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 1.8f);
    Rect(962, 314, 7, 5, Accent * Pulse);
    Rect(1010, 314, 7, 5, Accent * Pulse);
    for (int32 I = 0; I < 50; ++I)
    {
        const float Half = FMath::Sqrt(FMath::Max(0.f, 625.f - FMath::Square(float(I - 25))));
        Rect(1150 - Half, 77 + I, Half * 2, 1, FLinearColor(.34f, .43f, .50f));
    }
    for (int32 I = 0; I < 48; ++I)
    {
        const float Half = FMath::Sqrt(FMath::Max(0.f, 576.f - FMath::Square(float(I - 24))));
        Rect(1161 - Half, 71 + I, Half * 2, 1, FLinearColor(.02f, .031f, .043f));
    }
    for (int32 Y = 0; Y < 720; Y += 4) Rect(680, Y, 600, 1, FLinearColor(0, 0, 0, .08f));
    Label(TEXT("MEtAL  /  PROTOTYPE 01"), 72, 48, .75f, Accent);
    Rect(72, 87, 45, 3, Accent);
    Label(TEXT("6 HEURES"), 67, 111, 3.8f, Pale);
    Label(TEXT("CHEZ VOUS"), 72, 198, 1.55f, Dim);
    Label(TEXT("Le courant coupe. Vous n'etes plus seul."), 74, 253, .85f, Pale);
    auto Button = [&](FName Name, const FString& Title, const FString& Sub, float Y, bool Primary)
    {
        const FVector2D Pos(OX + 72*S, OY + Y*S), Size(478*S, 57*S);
        float MX = -1, MY = -1;
        if (auto* PC = GetOwningPlayerController()) PC->GetMousePosition(MX, MY);
        const bool Hover = MX >= Pos.X && MX <= Pos.X + Size.X && MY >= Pos.Y && MY <= Pos.Y + Size.Y;
        Rect(72, Y, 478, 57, Hover ? FLinearColor(.12f, .13f, .13f) : (Primary ? FLinearColor(.09f, .08f, .065f) : FLinearColor(.025f, .034f, .044f)));
        Rect(72, Y, Hover || Primary ? 4 : 1, 57, Hover || Primary ? Accent : Dim);
        Label(Title, 94, Y + 8, 1.f, Hover ? Accent : Pale);
        Label(Sub, 94, Y + 33, .6f, Dim);
        Label(Hover ? TEXT(">") : TEXT("+"), 518, Y + 17, .9f, Hover ? Accent : Dim);
        AddHitBox(Pos, Size, Name, true);
    };
    if (!bShowControls)
    {
        Button(TEXT("Resume"), G->Hour > 0 ? TEXT("CONTINUER") : TEXT("JOUER"), FString::Printf(TEXT("Heure %d / 6  -  reprendre la partie"), G->Hour + 1), 318, true);
        Button(TEXT("NewGame"), TEXT("NOUVELLE PARTIE"), TEXT("Recommencer depuis la premiere heure"), 386, false);
        Button(TEXT("Controls"), TEXT("COMMENT JOUER"), TEXT("Commandes et regles de survie"), 454, false);
        Button(TEXT("Quit"), TEXT("QUITTER"), TEXT("Retour au bureau"), 522, false);
        Label(TEXT("00:00"), 897, 592, 2.f, Pale);
        Label(TEXT("TENIR JUSQU'A 06:00"), 869, 640, .75f, Dim);
    }
    else
    {
        Rect(660, 35, 575, 655, Ink);
        Label(TEXT("LES COMMANDES"), 700, 70, 1.25f, Accent);
        const TCHAR* Rows[] = { TEXT("ZQSD / WASD    Se deplacer"), TEXT("SOURIS             Regarder"), TEXT("E                       Interagir / ouvrir / poser"), TEXT("F                       Allumer la lampe"), TEXT("H                       Se cacher face au placard"), TEXT("CLIC DROIT       Activer le detecteur"), TEXT("CLIC GAUCHE    Augmenter sa puissance"), TEXT("DOUBLE-CLIC    Revenir au niveau 1"), TEXT("ECHAP              Pause"), TEXT("R                       Reessayer apres une mort") };
        for (int32 I = 0; I < 10; ++I) Label(Rows[I], 700, 120 + I*34, .78f, Pale);
        Label(TEXT("Ecoute. Observe. Economise la batterie."), 700, 500, .82f, Accent);
        Label(TEXT("Toupie sur le socle : porte protegee."), 700, 540, .72f, Dim);
        Label(TEXT("Conduit : marchepied, puis cachette."), 700, 568, .72f, Dim);
        Label(TEXT("Des l'heure 3 : eclaire le robot du placard."), 700, 596, .72f, Dim);
        Label(TEXT("Chaque heure dure 6 minutes."), 74, 330, .85f, Pale);
        Label(TEXT("Les fenetres ouvertes renouvellent l'air."), 74, 367, .78f, Dim);
        Label(TEXT("Ferme-les quand un robot approche."), 74, 398, .78f, Dim);
        Button(TEXT("Controls"), TEXT("RETOUR"), TEXT("Revenir au menu principal"), 522, true);
    }
    Rect(72, 628, 478, 1, FLinearColor(.1f, .13f, .15f));
    Label(TEXT("Sauvegarde automatique au debut de chaque heure."), 74, 648, .66f, Dim);
    Label(TEXT("J / CARNET DES MONSTRES"), 74, 679, .65f, Dim);
}

void AMetalHUD::DrawDeathScreen(AMetalGameMode* G)
{
    const float S = FMath::Min(Canvas->SizeX / 1280.f, Canvas->SizeY / 720.f);
    const float X = (Canvas->SizeX - 1280*S)/2, Y = (Canvas->SizeY - 720*S)/2;
    const float Time = G->DeathScreenTime;
    const FLinearColor Night(.002f, .004f, .009f), Blue(.38f, .67f, .9f), Pale(.77f, .86f, .94f);
    DrawRect(Night, 0, 0, Canvas->SizeX, Canvas->SizeY);
    auto Text = [&](const FString& T, float PX, float PY, float Scale, FLinearColor C) { TextAt(T, X + PX*S, Y + PY*S, Scale*S, C); };
    const float Fade = FMath::Clamp(Time / 1.2f, 0.f, 1.f);
    for (int32 I = 0; I < 42; ++I)
    {
        const float PX = FMath::Fmod(I * 179.3f, 1280.f);
        const float PY = FMath::Fmod(I * 93.7f + Time * (2 + I%4), 720.f);
        DrawRect(FLinearColor(.18f,.34f,.5f,Fade*.22f), X+PX*S,Y+PY*S,2*S,2*S);
    }
    const float CY = 162 + FMath::Sin(Time * 1.1f)*4;
    for (int32 R = 95; R > 45; R -= 5)
    {
        for (int32 A = 0; A < 64; ++A)
        {
            const float Angle = 2*PI*A/64;
            DrawRect(FLinearColor(.06f,.18f,.3f,.035f*Fade), X+(640+FMath::Cos(Angle)*R)*S, Y+(CY+FMath::Sin(Angle)*R)*S, 5*S,5*S);
        }
    }
    for (int32 Row = -45; Row <= 45; ++Row)
    {
        const float Extent = FMath::Sqrt(float(45*45 - Row*Row));
        DrawRect(FLinearColor(.55f,.77f,1.f,Fade), X+(640-Extent)*S,Y+(CY+Row)*S,Extent*2*S,1.5f*S);
    }
    for (int32 Row = -41; Row <= 41; ++Row)
    {
        const float Extent = FMath::Sqrt(float(41*41 - Row*Row));
        DrawRect(Night, X+(657-Extent)*S,Y+(CY-9+Row)*S,Extent*2*S,1.5f*S);
    }
    Text(TEXT("LA LUNE TE REGARDE"), 474, 252, 1.05f, FLinearColor(.38f,.67f,.9f,Fade));
    FString Name = TEXT("LE MANQUE D'AIR"), Advice = TEXT("Rouvre les fenetres lorsque les robots s'eloignent.");
    if (G->DeathReason == TEXT("PORTE")) { Name = TEXT("COR"); Advice = TEXT("Pose la toupie sur son socle avant que la porte ne cede."); }
    else if (G->DeathReason == TEXT("CONDUIT")) { Name = TEXT("LE RAMPANT"); Advice = TEXT("Surveille le conduit. Cache-toi dans l'armoire avant sa descente."); }
    else if (G->DeathReason == TEXT("PLACARD")) { Name = TEXT("LE GUETTEUR"); Advice = TEXT("Ouvre le placard et eclaire son visage pour le faire reculer."); }
    else if (G->DeathReason.StartsWith(TEXT("FENETRE"))) { Name = TEXT("LE ROBOT DE LA FENETRE"); Advice = TEXT("Ferme la fenetre quand il arrive. Ecoute ses pas pour anticiper."); }
    Text(TEXT("Tu n'as pas atteint l'aube."), 412, 306, 1.25f, Pale);
    Text(Name, 465, 354, .85f, Blue);
    const int32 Revealed = FMath::Clamp(FMath::FloorToInt((Time-1.f)*36.f), 0, Advice.Len());
    Text(Advice.Left(Revealed), 225, 408, .88f, Pale);
    if (Time > 2.6f) Text(TEXT("Observe. Apprends. Recommence."), 424, 459, .82f, Blue);
    auto Button = [&](FName Id, const FString& Title, float PX, float Width)
    {
        const FVector2D Pos(X+PX*S,Y+551*S), Size(Width*S,54*S);
        float MX=-1,MY=-1;
        if(auto* PC=GetOwningPlayerController()) PC->GetMousePosition(MX,MY);
        const bool Hover=MX>=Pos.X && MX<=Pos.X+Size.X && MY>=Pos.Y && MY<=Pos.Y+Size.Y;
        DrawRect(Hover?FLinearColor(.07f,.16f,.22f):FLinearColor(.025f,.05f,.075f),Pos.X,Pos.Y,Size.X,Size.Y);
        DrawRect(Blue,Pos.X,Pos.Y,2*S,Size.Y);
        Text(Title,PX+22,569,.88f,Hover?FLinearColor::White:Pale);
        AddHitBox(Pos,Size,Id,true);
    };
    Button(TEXT("RetryDeath"),TEXT("REESSAYER  /  R"),310,315);
    Button(TEXT("DeathMenu"),TEXT("MENU PRINCIPAL"),650,315);
    Text(FString::Printf(TEXT("Retour au debut de l'heure %d / 6"),G->Hour+1),435,635,.72f,Blue);
}

void AMetalHUD::DrawJournal(AMetalGameMode* G)
{
    const float S=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f);
    const float OX=(Canvas->SizeX-1280*S)/2,OY=(Canvas->SizeY-720*S)/2;
    const FLinearColor Ink(.84f,.84f,.76f),Dim(.44f,.49f,.48f),Gold(.82f,.58f,.3f);
    auto Rect=[&](float X,float Y,float W,float H,FLinearColor C){DrawRect(C,OX+X*S,OY+Y*S,W*S,H*S);};
    auto Text=[&](const FString& T,float X,float Y,float K,FLinearColor C){TextAt(T,OX+X*S,OY+Y*S,K*S,C);};
    DrawRect(FLinearColor(.005f,.009f,.015f),0,0,Canvas->SizeX,Canvas->SizeY);
    Rect(65,45,1150,630,FLinearColor(.12f,.14f,.14f)); Rect(405,45,3,630,Dim);
    Text(TEXT("CARNET DE JACK"),95,78,1.4f,Ink);
    Text(TEXT("Ce que la nuit m'a appris."),95,122,.75f,Dim);
    const TCHAR* Names[]={TEXT("COR"),TEXT("ROBOTS DES FENETRES"),TEXT("LE RAMPANT"),TEXT("LE GUETTEUR DU PLACARD")};
    auto Button=[&](FName Id,const FString& Title,float Y,bool Selected)
    {
        FVector2D Pos(OX+90*S,OY+Y*S),Size(290*S,58*S);
        float MX=-1,MY=-1; if(auto* PC=GetOwningPlayerController())PC->GetMousePosition(MX,MY);
        bool Hover=MX>=Pos.X&&MX<=Pos.X+Size.X&&MY>=Pos.Y&&MY<=Pos.Y+Size.Y;
        Rect(90,Y,290,58,Selected||Hover?FLinearColor(.21f,.24f,.23f):FLinearColor(.085f,.105f,.11f));
        Text(Title,102,Y+19,.71f,Selected?Gold:Ink); AddHitBox(Pos,Size,Id,true);
    };
    for(int32 I=0;I<4;++I)
    {
        const int32 L=G->JournalData?G->JournalData->Knowledge[I]:0;
        Button(FName(*FString::Printf(TEXT("Journal%d"),I)),L?Names[I]:TEXT("??? / NON RENCONTRE"),190+I*82,I==JournalPage);
        Text(FString::Printf(TEXT("Notes %d / 3"),L),105,251+I*82,.55f,Dim);
    }
    Button(TEXT("JournalClose"),TEXT("FERMER / J ou ECHAP"),582,false);
    if (G->JournalData && G->JournalData->bMoonlightUnlocked)
        Button(TEXT("JournalMoon"),TEXT("LA LUNE / PAGE RETROUVEE"),518,JournalPage==4);
    if (JournalPage == 4)
    {
        Text(TEXT("PAGE RETROUVEE / NOTES PERSONNELLES"),450,82,.75f,Gold);
        Text(TEXT("MOONLIGHT"),450,130,1.35f,Ink);
        Rect(450,180,715,1,Dim);
        const TCHAR* Lines[] = {
            TEXT("Je croyais que cette voix venait de ma fatigue."),
            TEXT("Mais la lune sait comment ils entrent. Elle connait leurs gestes."),
            TEXT("Elle sait meme ce que j'ai fait juste avant de mourir."),
            TEXT("Comment peut-elle se souvenir de ce que personne n'a vu ?"),
            TEXT("Elle parle du prototype comme si elle l'avait deja utilise."),
            TEXT("Est-ce qu'elle connait MEtAL ? Est-ce qu'elle me connait ?"),
            TEXT("Chaque fois qu'elle m'aide, j'ai l'impression qu'elle attend"),
            TEXT("quelque chose en retour."),
            TEXT("Et si elle ne me sauvait pas... mais me gardait ici ?")
        };
        for(int32 I=0;I<9;++I) Text(Lines[I],450,220+I*36,.78f,I==8?Gold:Ink);
        Text(TEXT("- Jack. Ne pas montrer cette page a MEtAL."),450,589,.76f,Dim);
        Text(TEXT("Certaines reponses ne font qu'ajouter des questions."),450,633,.65f,Dim);
        return;
    }
    const int32 Level=G->JournalData?G->JournalData->Knowledge[JournalPage]:0;
    Text(FString::Printf(TEXT("DOSSIER %02d / %d NOTES SUR 3"),JournalPage+1,Level),450,82,.75f,Gold);
    Text(Level?Names[JournalPage]:TEXT("PRESENCE INCONNUE"),450,130,1.12f,Ink);
    Rect(450,180,715,1,Dim);
    if(!Level)
    {
        Text(TEXT("Cette page est encore vide."),450,233,1.f,Ink);
        Text(TEXT("Rencontre cette creature pour commencer sa fiche."),450,285,.82f,Dim);
        Text(TEXT("Ses secrets se reveleront au fil de tes tentatives."),450,326,.82f,Dim);
        return;
    }
    const TCHAR* Identity[]={TEXT("Un gardien metallique approche par le couloir."),TEXT("Ils guettent aux deux fenetres de la chambre."),TEXT("Il rampe au-dessus de toi, dans le conduit."),TEXT("Il habite le placard et avance peu a peu vers toi.")};
    const TCHAR* Behavior[]={TEXT("Il frappe la porte jusqu'a la briser. Les degats restent."),TEXT("A leur arrivee : 8 secondes pour fermer la fenetre."),TEXT("Il descend puis cherche dans la chambre pendant 6 secondes."),TEXT("Il devient actif a partir de la troisieme heure.")};
    const TCHAR* Defense[]={TEXT("Pose la toupie sur le socle blanc pour proteger la porte."),TEXT("Ferme la bonne fenetre et attends que le robot s'eloigne."),TEXT("H face au placard : cache-toi avant sa descente."),TEXT("Ouvre les portes avec E. Allume ta lampe avec F.")};
    Text(TEXT("01 / RENCONTRE"),450,212,.78f,Gold);
    Text(Identity[JournalPage],450,248,.85f,Ink);
    Text(Level>=2?TEXT("02 / COMPORTEMENT"):TEXT("02 / OBSERVATION MANQUANTE"),450,307,.78f,Level>=2?Gold:Dim);
    Text(Level>=2?Behavior[JournalPage]:TEXT("Observe son approche pour en apprendre davantage."),450,343,.8f,Level>=2?Ink:Dim);
    Text(Level>=3?TEXT("03 / COMMENT SURVIVRE"):TEXT("03 / CONSEIL ENCORE INCONNU"),450,402,.78f,Level>=3?Gold:Dim);
    Text(Level>=3?Defense[JournalPage]:TEXT("Survis a son attaque, ou ecoute la lune apres une mort."),450,438,.8f,Level>=3?Ink:Dim);
    if(Level>=3)
    {
        const TCHAR* Extra[]={TEXT("Le socle consomme la batterie et empeche la detection."),TEXT("Rouvre ensuite pour renouveler l'oxygene de la chambre."),TEXT("Reste cache jusqu'au message de depart. E pour ressortir."),TEXT("Vise son visage. Une porte fermee bloque la lumiere.")};
        Text(Extra[JournalPage],450,478,.76f,Ink);
        if(JournalPage==2)
        {
            Text(TEXT("Actif des l'heure 2. Marchepied : observe le conduit."),450,518,.76f,Ink);
            Text(TEXT("Delai par heure : 6 / 5 / 4 / 3 / 2,5 / 2 secondes."),450,553,.76f,Ink);
        }
        else if(JournalPage==3) Text(TEXT("Il avance meme portes fermees. La lumiere le repousse."),450,518,.76f,Ink);
        else Text(TEXT("Le detecteur ignore les robots encore trop eloignes."),450,518,.76f,Ink);
    }
    Text(TEXT("Notes conservees entre les parties. Lecture en pause."),450,626,.65f,Dim);
}

void AMetalHUD::DrawComputer()
{
    auto* G=Metal::Game(this); if(!G)return;
    const float S=FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f);
    const float OX=(Canvas->SizeX-1280*S)/2,OY=(Canvas->SizeY-720*S)/2;
    const FLinearColor Green(.35f,.85f,.55f),Dim(.22f,.46f,.33f);
    DrawRect(FLinearColor(.003f,.012f,.009f),0,0,Canvas->SizeX,Canvas->SizeY);
    auto Text=[&](const FString& T,float X,float Y,float K,FLinearColor C){TextAt(T,OX+X*S,OY+Y*S,K*S,C);};
    Text(TEXT("MEtAL // TERMINAL LOCAL"),100,80,1.6f,Green);
    Text(TEXT("SESSION : JACK   /   PROTOTYPE 01"),100,134,.75f,Dim);
    DrawRect(Dim,OX+100*S,OY+179*S,1080*S,1*S);
    Text(G->ComputerMessage,100,225,.92f,Green);
    Text(TEXT("Saisir une commande..."),100,295,.85f,Dim);
    const bool Blink=FMath::Fmod(GetWorld()->GetRealTimeSeconds(),1.f)<.5f;
    Text(TEXT("> ")+G->ComputerText+(Blink?TEXT("_"):TEXT(" ")),100,348,1.3f,Green);
    Text(TEXT("ENTREE / Valider     RETOUR ARRIERE / Effacer"),100,505,.8f,Dim);
    const FVector2D Pos(OX+100*S,OY+590*S),Size(400*S,50*S);
    DrawRect(FLinearColor(.03f,.09f,.06f),Pos.X,Pos.Y,Size.X,Size.Y);
    Text(TEXT("QUITTER LE TERMINAL / ECHAP"),118,607,.8f,Green);
    AddHitBox(Pos,Size,TEXT("ComputerClose"),true);
}

void AMetalHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    AMetalGameMode* G = Metal::Game(this);
    AMetalCharacter* P = Cast<AMetalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!G || !P) return;
    if (G->bComputerOpen) { DrawComputer(); return; }
    if (G->bJournalOpen) { DrawJournal(G); return; }
    if (G->bDead && !G->bScreamer) { DrawDeathScreen(G); return; }
    if (G->bWelcome && G->bPaused && !G->bDead) { DrawMainMenu(G); return; }
    const float W = Canvas->SizeX;
    const float H = Canvas->SizeY;
    if (G->bCorPreview)
    {
        TextAt(TEXT("COR"), 35, 28, 1.8f, Metal::Gold);
        TextAt(TEXT("GARDIEN DE LA PORTE / MODELE PROVISOIRE"), 35, 75, .8f, FLinearColor::White);
        return;
    }
    if (G->bScreamer)
    {
        if (G->ScreamerTime >= G->ScreamerBlackoutAt) DrawRect(FLinearColor::Black, 0, 0, W, H);
        else
        {
            DrawRect(FLinearColor::Black, 0, 0, W, H * .055f);
            DrawRect(FLinearColor::Black, 0, H * .945f, W, H * .055f);
        }
        return;
    }
    const FLinearColor White(.87f, .92f, .94f);
    if (G->bHiding && !G->bDead)
    {
        DrawRect(FLinearColor(.008f, .006f, .004f, .97f), 0, 0, W * .44f, H);
        DrawRect(FLinearColor(.008f, .006f, .004f, .97f), W * .56f, 0, W * .44f, H);
        DrawRect(FLinearColor::Black, W * .44f, 0, W * .12f, H * .25f);
        DrawRect(FLinearColor::Black, W * .44f, H * .75f, W * .12f, H * .25f);
    }
    const FLinearColor Muted(.5f, .62f, .65f);
    const float UX = W / 1280.f;
    const float UY = H / 720.f;
    TextAt(TEXT("MEtAL"), 30 * UX, 20 * UY, 1.4f * UX, White);
    TextAt(TEXT("PROTOTYPE / SIX HEURES CHEZ VOUS"), 30 * UX, 55 * UY, .6f * UX, Muted);
    TextAt(FString::Printf(TEXT("%02d:%02d"), FMath::Min(G->Hour, 6), G->Intermission > 0 ? 0 : FMath::Min(59, int32(G->Seconds / 6.f))), W - 155 * UX, 20 * UY, 1.5f * UX, White);
    TextAt(FString::Printf(TEXT("HEURE %d / 6"), FMath::Min(G->Hour + 1, 6)), W - 155 * UX, 57 * UY, .7f * UX, Muted);

    const FVector2D Center(85 * UX, 145 * UY);
    const float Radius = 38 * UX;
    for (int32 I = 0; I < 48; ++I)
    {
        const float A = 2.f * PI * I / 48;
        const float B = 2.f * PI * (I + 1) / 48;
        Stroke(Center.X + FMath::Sin(A) * Radius, Center.Y + FMath::Cos(A) * Radius,
            Center.X + FMath::Sin(B) * Radius, Center.Y + FMath::Cos(B) * Radius, Muted, 1);
    }
    if (G->Target != INDEX_NONE && G->Threats.IsValidIndex(G->Target))
    {
        FVector Direction = G->Threats[G->Target].Robot->GetActorLocation() - P->GetActorLocation();
        float Angle = FMath::DegreesToRadians(Direction.Rotation().Yaw - P->Camera->GetComponentRotation().Yaw);
        FVector2D Dir(FMath::Sin(Angle), -FMath::Cos(Angle));
        FVector2D Tip = Center + Dir * Radius * .85f;
        FVector2D Tail = Center - Dir * Radius * .4f;
        Stroke(Tail.X, Tail.Y, Tip.X, Tip.Y, Metal::Gold, 3 * UX);
        FVector2D Side(-Dir.Y, Dir.X);
        FVector2D WingA = Tip - Dir * 12 * UX + Side * 7 * UX;
        FVector2D WingB = Tip - Dir * 12 * UX - Side * 7 * UX;
        Stroke(Tip.X, Tip.Y, WingA.X, WingA.Y, Metal::Gold, 3 * UX);
        Stroke(Tip.X, Tip.Y, WingB.X, WingB.Y, Metal::Gold, 3 * UX);
    }
    else TextAt(G->bDocked ? TEXT("--") : TEXT("?"), Center.X - 8 * UX, Center.Y - 13 * UY, 1.1f * UX, Muted);
    TextAt(G->Status(), 142 * UX, 122 * UY, .7f * UX, G->Target != INDEX_NONE ? Metal::Gold : White);
    TextAt(FString::Printf(TEXT("PUISSANCE %d/4 / %.1f m"), G->PowerLevel + 1, Metal::Ranges[G->PowerLevel] / 100.f), 142 * UX, 148 * UY, .65f * UX, Muted);
    Bar(TEXT("BATTERIE"), G->Battery, 30 * UX, H - 125 * UY, 260 * UX, Metal::Gold);
    Bar(TEXT("OXYGENE"), G->Oxygen, 330 * UX, H - 125 * UY, 260 * UX, Metal::Cyan);
    Bar(TEXT("PORTE"), G->DoorHealth, 630 * UX, H - 125 * UY, 260 * UX, FLinearColor(.7f, .55f, .38f));
    const int32 Open = (!G->bTopClosed ? 1 : 0) + (!G->bBottomClosed ? 1 : 0);
    TextAt(FString::Printf(TEXT("VENTILATION %d%%"), Open * 50), 930 * UX, H - 119 * UY, .8f * UX, White);
    TextAt(TEXT("ZQSD / WASD : marcher    E : interagir    F : lampe    H : cachette    J : carnet"), 30 * UX, H - 65 * UY, .7f * UX, White);
    TextAt(TEXT("Clic : puissance +    Double-clic : niveau 1    Clic droit : detecteur ON/OFF    Echap : pause"), 30 * UX, H - 38 * UY, .65f * UX, Muted);
    DrawRect(White, W / 2 - 2, H / 2 - 2, 4, 4);
    FString Prompt = G->InteractionLabel(P->FindInteraction());
    if (!Prompt.IsEmpty())
    {
        DrawRect(FLinearColor(.015f, .025f, .035f, .85f), W / 2 - 250 * UX, H / 2 + 35 * UY, 500 * UX, 37 * UY);
        TextAt(Prompt, W / 2 - 235 * UX, H / 2 + 43 * UY, .75f * UX, Metal::Cyan);
    }
    if (G->NoticeTime > 0)
    {
        DrawRect(FLinearColor(.02f, .03f, .04f, .8f), 250 * UX, 215 * UY, 780 * UX, 38 * UY);
        TextAt(G->Notice, 265 * UX, 224 * UY, .75f * UX, White);
    }
    if (G->Intermission > 0)
        TextAt(FString::Printf(TEXT("COURANT PRESENT / prochaine panne dans %d s"), FMath::CeilToInt(G->Intermission)), W / 2 - 255 * UX, H - 178 * UY, .8f * UX, Metal::Gold);
    else
        TextAt(FString::Printf(TEXT("RETOUR DU COURANT / %02d:%02d"), int32(360 - G->Seconds) / 60, int32(360 - G->Seconds) % 60), W / 2 - 165 * UX, H - 178 * UY, .75f * UX, Muted);

    if (G->bDead || G->bPaused || G->bWon)
    {
        DrawRect(FLinearColor(.008f, .012f, .024f, .96f), 0, 0, W, H);
        if (G->bDead)
        {
            const float CX = W / 2;
            const float CY = 170 * UY;
            for (int32 Y = -45; Y <= 45; ++Y)
            {
                const float Extent = FMath::Sqrt(45.f * 45 - Y * Y) * UX;
                Stroke(CX - Extent, CY + Y * UY, CX + Extent, CY + Y * UY, FLinearColor(.75f, .82f, 1.f), 2 * UY);
            }
            for (int32 Y = -42; Y <= 42; ++Y)
            {
                const float Extent = FMath::Sqrt(42.f * 42 - Y * Y) * UX;
                Stroke(CX + 17 * UX - Extent, CY - 8 * UY + Y * UY, CX + 17 * UX + Extent, CY - 8 * UY + Y * UY, FLinearColor(.008f, .012f, .024f), 2 * UY);
            }
            TextAt(TEXT("LA LUNE TE REGARDE"), W / 2 - 200 * UX, 250 * UY, 1.2f * UX, White);
            FString Advice;
            if (G->DeathReason == TEXT("OXYGENE")) Advice = TEXT("Tu as manque d'air. Rouvre les fenetres quand les pas s'eloignent.");
            else if (G->DeathReason == TEXT("PLACARD")) Advice = TEXT("Ouvre le placard et eclaire son visage pour le faire reculer. Fermer les portes ne suffit pas.");
            else if (G->DeathReason == TEXT("CONDUIT")) Advice = TEXT("Surveille le conduit avec le marchepied. Cache-toi dans l'armoire avant sa descente et attends son depart.");
            else if (G->DeathReason == TEXT("PORTE")) Advice = TEXT("La porte a cede. La toupie sur le socle peut repousser ce robot.");
            else Advice = TEXT("Un robot est entre par la fenetre. Ferme-la avant qu'il ne soit trop tard.");
            TextAt(Advice, 210 * UX, 322 * UY, .9f * UX, Metal::Cyan);
            TextAt(TEXT("Le detecteur ignore les robots lointains. Ecoute aussi les bruits."), 210 * UX, 370 * UY, .8f * UX, Muted);
            if (G->Hour >= 2) TextAt(TEXT("MEtAL savait qu'ils trouveraient le prototype..."), 210 * UX, 414 * UY, .8f * UX, Metal::Gold);
            TextAt(FString::Printf(TEXT("R / Reprendre au debut de l'heure %d"), G->Hour + 1), 210 * UX, 485 * UY, 1.f * UX, White);
        }
        else if (G->bWon)
        {
            TextAt(TEXT("06:00 / TU AS SURVECU"), 260 * UX, 240 * UY, 1.6f * UX, White);
            TextAt(TEXT("Le soleil se leve. MEtAL attendra tes explications."), 260 * UX, 315 * UY, 1.f * UX, Muted);
            TextAt(TEXT("N / Nouvelle partie"), 260 * UX, 405 * UY, 1.f * UX, Metal::Cyan);
        }
        else
        {
            TextAt(G->bWelcome ? TEXT("MEtAL / PRET A JOUER") : TEXT("PAUSE"), 260 * UX, 220 * UY, 1.8f * UX, White);
            auto Button = [&](FName Name, const FString& Label, float Y)
            {
                const FVector2D Position(260 * UX, Y * UY);
                const FVector2D Size(760 * UX, 48 * UY);
                float MouseX = -1, MouseY = -1;
                if (APlayerController* PC = GetOwningPlayerController()) PC->GetMousePosition(MouseX, MouseY);
                const bool Hovered = MouseX >= Position.X && MouseX <= Position.X + Size.X && MouseY >= Position.Y && MouseY <= Position.Y + Size.Y;
                DrawRect(Hovered ? FLinearColor(.06f, .23f, .27f) : FLinearColor(.04f, .075f, .10f), Position.X, Position.Y, Size.X, Size.Y);
                DrawRect(Hovered ? Metal::Cyan : Muted, Position.X, Position.Y, 4 * UX, Size.Y);
                TextAt(Label, Position.X + 20 * UX, Position.Y + 11 * UY, 1.f * UX, Hovered ? Metal::Cyan : White);
                AddHitBox(Position, Size, Name, true);
            };
            Button(TEXT("Resume"), G->bWelcome ? TEXT("Commencer   /   Echap") : TEXT("Reprendre   /   Echap"), 290);
            Button(TEXT("NewGame"), TEXT("Nouvelle partie   /   N"), 350);
            Button(TEXT("Quit"), TEXT("Quitter le jeu"), 410);
            TextAt(TEXT("La progression est sauvegardee au debut de chaque heure."), 260 * UX, 485 * UY, .8f * UX, Muted);
            if (G->bWelcome) TextAt(TEXT("ZQSD / marcher     E / interagir     F / lampe     Clic / puissance"), 260 * UX, 545 * UY, .8f * UX, White);
        }
    }
}

void AMetalHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);
    AMetalGameMode* G = Metal::Game(this);
    if (G && G->bComputerOpen)
    {
        if(BoxName==TEXT("ComputerClose"))G->CloseComputer();
        return;
    }
    if (G && G->bJournalOpen)
    {
        if (BoxName == TEXT("JournalClose")) G->ToggleJournal();
        for (int32 I=0; I<4; ++I) if(BoxName == FName(*FString::Printf(TEXT("Journal%d"),I))) JournalPage=I;
        if(BoxName==TEXT("JournalMoon") && G->JournalData && G->JournalData->bMoonlightUnlocked)JournalPage=4;
        return;
    }
    if (G && G->bDead && !G->bScreamer)
    {
        if (BoxName == TEXT("RetryDeath")) G->RetryHour();
        else if (BoxName == TEXT("DeathMenu")) G->ReturnToMenu();
        return;
    }
    if (!G || !G->bPaused || G->bDead || G->bWon) return;
    if (BoxName == TEXT("Controls")) { bShowControls = !bShowControls; return; }
    bShowControls = false;
    if (BoxName == TEXT("Resume")) G->SetPaused();
    else if (BoxName == TEXT("NewGame")) G->StartNewGame();
    else if (BoxName == TEXT("Quit"))
    {
        if (APlayerController* PC = GetOwningPlayerController()) PC->ConsoleCommand(TEXT("quit"));
    }
}
