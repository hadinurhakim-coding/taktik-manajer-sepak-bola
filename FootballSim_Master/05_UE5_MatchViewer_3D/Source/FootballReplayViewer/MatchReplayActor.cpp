#include "MatchReplayActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Home(0.015f, 0.16f, 1.0f), Away(1.0f, 0.025f, 0.015f);
const FLinearColor Outline(0.003f, 0.006f, 0.012f), BallYellow(1.0f, 0.85f, 0.005f);
const FLinearColor Ink(0.018f, 0.028f, 0.05f, 0.97f), Muted(0.52f, 0.65f, 0.71f);
FString Example(const TCHAR* Name) { return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Replays"), Name); }
}

AMatchReplayActor::AMatchReplayActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ReplayScene")));
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("BroadcastSpring"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 15000;
    CameraBoom->SetRelativeRotation(FRotator(-55,90,0));
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 3;
    CameraBoom->bEnableCameraRotationLag = true;
    CameraBoom->CameraRotationLagSpeed = 3;
}
UMaterialInstanceDynamic* AMatchReplayActor::Material(FLinearColor Color, bool bFlat)
{
    auto* M = UMaterialInstanceDynamic::Create(bFlat ? FlatMaterial : ColorMaterial, this);
    if (M) { M->SetVectorParameterValue(TEXT("Tint"), Color); Materials.Add(M); }
    return M;
}
UStaticMeshComponent* AMatchReplayActor::Shape(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, USceneComponent* Parent, bool bFlat)
{
    auto* C = NewObject<UStaticMeshComponent>(this);
    C->SetupAttachment(Parent ? Parent : GetRootComponent());
    C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetMaterial(0, Material(Color, bFlat));
    C->SetCastShadow(!bFlat);
    C->SetRelativeLocation(Location); C->SetRelativeScale3D(Scale);
    C->RegisterComponent();
    return C;
}
void AMatchReplayActor::Line(FVector A, FVector B, float Width)
{
    const FVector Delta = B - A;
    // Raised, opaque markings avoid grazing-angle flicker against the turf.
    auto* C = Shape(Cube, (A + B) * 0.5 + FVector(0,0,8), FVector(Delta.Size() / 100, Width / 100, 0.08), FLinearColor::White, nullptr, true);
    C->SetRelativeRotation(Delta.Rotation());
    C->SetCastShadow(false);
}
void AMatchReplayActor::BuildScene()
{
    Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    ColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Viewer/M_ViewerColor.M_ViewerColor"));
    if (!ColorMaterial) ColorMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    FlatMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Viewer/M_ViewerFlat.M_ViewerFlat"));
    if (!FlatMaterial) FlatMaterial = ColorMaterial;
    Shape(Cube, FVector(0, 0, -55), FVector(126, 86, 0.5), FLinearColor(0.012f, 0.023f, 0.028f), nullptr, true);
    for (int32 I = 0; I < 20; ++I)
        Shape(Cube, FVector(-5250 + 262.5 + I * 525, 0, -20), FVector(5.25, 68, 0.4),
            I % 2 ? FLinearColor(0.012f, 0.12f, 0.045f) : FLinearColor(0.016f, 0.16f, 0.06f), nullptr, true);
    Line(FVector(-5250,-3400,4), FVector(5250,-3400,4));
    Line(FVector(-5250,3400,4), FVector(5250,3400,4));
    Line(FVector(-5250,-3400,4), FVector(-5250,3400,4));
    Line(FVector(5250,-3400,4), FVector(5250,3400,4));
    Line(FVector(0,-3400,4), FVector(0,3400,4));
    for (int32 I = 0; I < 96; ++I)
    {
        const double A = I * 2 * PI / 96, B = (I + 1) * 2 * PI / 96;
        Line(FVector(915 * FMath::Cos(A),915 * FMath::Sin(A),4), FVector(915 * FMath::Cos(B),915 * FMath::Sin(B),4));
    }
    Shape(Cylinder, FVector(0,0,12), FVector(0.45,0.45,0.06), FLinearColor::White, nullptr, true);
    for (double Side : {-1.0, 1.0})
    {
        const double X = Side * 5250;
        for (const auto& Box : {FVector2D(1650,2016), FVector2D(550,916)})
        {
            Line(FVector(X,-Box.Y,4), FVector(X-Side*Box.X,-Box.Y,4));
            Line(FVector(X,Box.Y,4), FVector(X-Side*Box.X,Box.Y,4));
            Line(FVector(X-Side*Box.X,-Box.Y,4), FVector(X-Side*Box.X,Box.Y,4));
        }
        Shape(Cylinder, FVector(X-Side*1100,0,12), FVector(0.45,0.45,0.06), FLinearColor::White, nullptr, true);
        for (double Y : {-366.0, 366.0})
        {
            Shape(Cube, FVector(X,Y,122), FVector(0.22,0.22,2.44), FLinearColor::White, nullptr, true);
            Shape(Cube, FVector(X+Side*180,Y,122), FVector(0.07,0.07,2.44), FLinearColor(0.6f,0.7f,0.75f));
        }
        Shape(Cube, FVector(X,0,244), FVector(0.22,7.32,0.22), FLinearColor::White, nullptr, true);
        Shape(Cube, FVector(X+Side*180,0,244), FVector(0.07,7.32,0.07), FLinearColor(0.6f,0.7f,0.75f));
        for (int32 N = 0; N < 13; ++N)
            Shape(Cube, FVector(X+Side*180,-366+61*N,122), FVector(0.025,0.025,2.44), FLinearColor(0.35f,0.43f,0.45f));
        for (int32 N = 0; N < 5; ++N)
            Shape(Cube, FVector(X+Side*180,0,61*N), FVector(0.025,7.32,0.025), FLinearColor(0.35f,0.43f,0.45f));
    }
    CharacterMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Football/Characters/SK_NicoPaz.SK_NicoPaz"));
    for (const TCHAR* Name : {TEXT("idle"),TEXT("jog"),TEXT("sprint"),TEXT("dribble"),TEXT("pass"),TEXT("shot"),TEXT("tackle_stand"),
        TEXT("gk_ready"),TEXT("gk_dive_left"),TEXT("gk_dive_right"),TEXT("gk_high"),TEXT("gk_smother"),TEXT("gk_pass")})
    {
        const FString Path = FString::Printf(TEXT("/Game/Football/Animations/A_%s.A_%s"),Name,Name);
        if (auto* Animation = LoadObject<UAnimSequence>(nullptr,*Path)) Animations.Add(FName(Name),Animation);
    }
    for (int32 I = 0; I < 22; ++I)
    {
        auto* P = NewObject<USceneComponent>(this); P->SetupAttachment(RootComponent); P->RegisterComponent(); PlayerRoots.Add(P);
        const auto Color = I % 11 == 0 ? (I < 11 ? FLinearColor(0.98f,0.72f,0.08f) : FLinearColor(0.65f,0.15f,0.9f)) : (I < 11 ? Home : Away);
        // Dark rim and team-colour base remain distinct over white pitch markings.
        Shape(Cylinder, FVector(0,0,18), FVector(2.2,2.2,0.06), Outline, P, true);
        Shape(Cylinder, FVector(0,0,23), FVector(1.85,1.85,0.06), Color, P, true);
        USkeletalMeshComponent* Skel = nullptr;
        if (CharacterMesh)
        {
            Skel = NewObject<USkeletalMeshComponent>(this); Skel->SetupAttachment(P);
            Skel->SetSkeletalMeshAsset(CharacterMesh);
            Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Skel->SetRelativeLocation(FVector(0,0,28));
            Skel->SetRelativeScale3D(FVector(1.25)); // Readable at broadcast distance.
            Skel->RegisterComponent(); Skel->bPauseAnims = true;
        }
        else
        {
            Shape(Cylinder, FVector(0,0,100), FVector(1.1,1.1,1.4), Color, P);
            Shape(Sphere, FVector(0,0,180), FVector(0.72), FLinearColor(0.72f,0.53f,0.36f), P);
        }
        SkeletalPlayers.Add(Skel);
        auto* Label = NewObject<UTextRenderComponent>(this); Label->SetupAttachment(P);
        Label->SetRelativeLocation(FVector(0,0,300)); Label->SetWorldSize(165);
        Label->SetHorizontalAlignment(EHTA_Center); Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetText(FText::AsNumber(I % 11 + 1)); Label->SetTextRenderColor(FColor::White);
        Label->RegisterComponent(); Numbers.Add(Label);
    }
    BallRoot = NewObject<USceneComponent>(this); BallRoot->SetupAttachment(RootComponent); BallRoot->RegisterComponent();
    // Visual exaggeration only: all replay coordinates and physics stay unchanged.
    Shape(Sphere, FVector(0,0,26), FVector(0.7), FLinearColor::White, BallRoot);
    Shape(Sphere, FVector(0,-28,44), FVector(0.25), Outline, BallRoot);
    BallMarker = NewObject<USceneComponent>(this); BallMarker->SetupAttachment(RootComponent); BallMarker->RegisterComponent();
    Shape(Cylinder, FVector(0,0,30), FVector(2.7,2.7,0.05), Outline, BallMarker, true);
    Shape(Cylinder, FVector(0,0,34), FVector(2.3,2.3,0.05), BallYellow, BallMarker, true);
    Shape(Cylinder, FVector(0,0,38), FVector(1.65,1.65,0.05), Outline, BallMarker, true);
    auto* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,5000), FRotator(-55,-25,0));
    Sun->GetLightComponent()->SetIntensity(3.0f);
    auto* Sky = GetWorld()->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetIntensity(0.3f);
    Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(0.25f,0.3f,0.35f));
    Camera = GetWorld()->SpawnActor<ACameraActor>();
    Camera->AttachToComponent(CameraBoom, FAttachmentTransformRules::SnapToTargetNotIncludingScale, USpringArmComponent::SocketName);
    auto* C = Camera->GetCameraComponent(); C->ProjectionMode = ECameraProjectionMode::Orthographic;
    C->OrthoWidth = 13600; C->bConstrainAspectRatio = false;
    C->bOverrideAspectRatioAxisConstraint = true;
    C->AspectRatioAxisConstraint = AspectRatio_MaintainXFOV;
    C->PostProcessSettings.bOverride_AutoExposureMethod = true;
    C->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    C->PostProcessSettings.bOverride_AutoExposureBias = true;
    C->PostProcessSettings.AutoExposureBias = 0;
    C->PostProcessSettings.bOverride_BloomIntensity = true;
    C->PostProcessSettings.BloomIntensity = 0;
    UpdateCameraFraming();
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetViewTarget(Camera);
    UE_LOG(LogTemp, Display, TEXT("[ReplayViewer] Scene created: 22 players, skeletal=%d, animation assets=%d, Broadcast Spring."),CharacterMesh != nullptr,Animations.Num());
}
void AMatchReplayActor::UpdateCameraFraming()
{
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !Camera) return;
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0 || FramedViewport == FIntPoint(Width,Height)) return;
    FramedViewport = FIntPoint(Width,Height);
    // Fit the complete pitch and goals while reserving space for both HUD bars.
    // Explicit horizontal-axis framing avoids the engine's default Y-axis zoom-out.
    Camera->GetCameraComponent()->SetOrthoWidth(FMath::Max(11800.0f, 8500.0f * Width / Height));
    UE_LOG(LogTemp, Display, TEXT("[ReplayViewer] Framed %dx%d: ortho width %.0fcm"), Width, Height, Camera->GetCameraComponent()->OrthoWidth);
}
bool AMatchReplayActor::LoadReplay(const FString& File)
{
    FFSimReplay Data;
    if (!FFSimReplayReader::LoadFile(File, Data, ErrorMessage))
    { UE_LOG(LogTemp, Error, TEXT("[ReplayViewer] %s: %s"), *File, *ErrorMessage); return false; }
    Playback.SetReplay(MoveTemp(Data));
    ClipLabel = Playback.GetReplay().Version == 2 ? TEXT("Highlight 15 menit") : FPaths::GetBaseFilename(File).Contains(TEXT("first_goal")) ? TEXT("Gol pertama") : TEXT("Akhir pertandingan");
    LastSourceTick = -1;
    ErrorMessage.Reset(); ApplyFrame();
    UE_LOG(LogTemp, Display, TEXT("[ReplayViewer] Loaded %s: %d frames, %.2fs, rate %.1fx"), *File, Playback.GetReplay().Frames.Num(), Playback.GetReplay().Duration(), Playback.Rate());
    return true;
}
void AMatchReplayActor::BeginPlay()
{
    Super::BeginPlay();
    BuildScene();
    FString File = Example(TEXT("highlights_15min.fsim"));
    FParse::Value(FCommandLine::Get(), TEXT("ReplayFile="), File);
    LoadReplay(File);
    if (FParse::Param(FCommandLine::Get(), TEXT("ReplayAutoplay"))) Playback.SetPlaying(true);
    BuildOverlay();
}
void AMatchReplayActor::ApplyFrame()
{
    if (!Playback.Sample(Current)) return;
    for (int32 I = 0; I < PlayerRoots.Num(); ++I)
    {
        PlayerRoots[I]->SetWorldLocation(FFSimReplayReader::ToUnreal(Current.Players[I]));
        AnimatePlayer(I);
        Numbers[I]->SetWorldRotation((Camera->GetActorLocation() - Numbers[I]->GetComponentLocation()).Rotation());
    }
    const FVector BallPosition = FFSimReplayReader::ToUnreal(Current.Ball);
    if (BallRoot) BallRoot->SetWorldLocation(BallPosition);
    if (BallMarker) BallMarker->SetWorldLocation(FVector(BallPosition.X,BallPosition.Y,0));
    LastSourceTick = Playback.SourceTick();
}
void AMatchReplayActor::AnimatePlayer(int32 I)
{
    auto* Mesh = SkeletalPlayers[I].Get();
    if (!Mesh) return;
    const auto& Frames = Playback.GetReplay().Frames;
    const int32 Frame = FMath::Clamp(FMath::FloorToInt(Playback.SourceFrame()),0,Frames.Num()-1);
    FVector Motion = FVector::ZeroVector;
    if (Frame + 1 < Frames.Num() && !(Frames[Frame+1].Flags & 6)) Motion = Frames[Frame+1].Players[I] - Frames[Frame].Players[I];
    const double Speed = Motion.Size() / 0.3;
    FName Name = Speed < 0.2 ? TEXT("idle") : Speed < 5 ? TEXT("jog") : TEXT("sprint");
    const bool Keeper = I % 11 == 0;
    if (Keeper && Speed < 1) Name = TEXT("gk_ready");
    // Action states are intentions; only the carrier/shot marker triggers a kick.
    if (Current.BallOwner == I)
    {
        if (Current.Actions[I] == 0) Name = Keeper ? TEXT("gk_pass") : TEXT("pass");
        else if (Current.Actions[I] == 1) Name = TEXT("dribble");
        else if (Current.Actions[I] == 2) Name = TEXT("shot");
    }
    if (Current.Actions[I] == 3 && FVector::Dist(Current.Players[I],Current.Ball) < 2) Name = TEXT("tackle_stand");
    // A save marker persists visually for the first second of its captured action.
    for (int32 F = Frame; F >= FMath::Max(0,Frame-3); --F)
        if (Keeper && Frames[F].BallOwner == I && (Frames[F].Flags & 16))
        { Name = (Frames[F].Flags & 32) ? (Frames[F].Ball.Z >= Frames[F].Players[I].Z ? TEXT("gk_dive_right") : TEXT("gk_dive_left")) : TEXT("gk_smother"); break; }
    const double Tick = Playback.SourceTick();
    const bool Discontinuous = LastSourceTick < 0 || Tick < LastSourceTick || Tick - LastSourceTick > 10;
    if (Name != LastAnimations[I] || Discontinuous)
    {
        LastAnimations[I] = Name; AnimationStartTick[I] = Tick;
        if (const auto* Anim = Animations.Find(Name)) Mesh->PlayAnimation(*Anim,true);
        Mesh->bPauseAnims = true;
    }
    if (const auto* Anim = Animations.Find(Name))
    {
        const double Duration = (*Anim)->GetPlayLength();
        const double Elapsed = (Tick - AnimationStartTick[I]) / 10.0;
        if (Duration > 0) Mesh->SetPosition(float(FMath::Fmod(Elapsed,Duration)),false);
    }
    if (Motion.SizeSquared() > 0.0001)
        PlayerRoots[I]->SetWorldRotation(FRotator(0,FFSimReplayReader::ToUnreal(Motion).Rotation().Yaw,0));
}
void AMatchReplayActor::UpdateBroadcastCamera(float DeltaSeconds)
{
    if (!Camera) return;
    const auto* Segment = Playback.ActiveSegment();
    const bool Alternate = Segment && Segment->Camera == TEXT("alternate");
    const FVector Ball = FFSimReplayReader::ToUnreal(Current.Ball);
    const FVector Focus = Alternate ? FVector(FMath::Clamp(Ball.X,-4800.0,4800.0),FMath::Clamp(Ball.Y,-2600.0,2600.0),50) : FVector(FMath::Clamp(Ball.X * 0.10,-500.0,500.0),0,0);
    CameraBoom->SetRelativeLocation(Focus);
    CameraBoom->SetRelativeRotation(Alternate ? FRotator(-38,Ball.X >= 0 ? 150 : 30,0) : FRotator(-55,90,0));
    const float FullWidth = FMath::Max(11800.0f,FramedViewport.Y > 0 ? 8500.0f * FramedViewport.X / FramedViewport.Y : 11800.0f);
    const float Width = Alternate ? 4400.0f : FullWidth;
    auto* C = Camera->GetCameraComponent();
    C->SetOrthoWidth(FMath::FInterpTo(C->OrthoWidth,Width,DeltaSeconds,3));
}
void AMatchReplayActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); StartupSeconds += DeltaSeconds;
    UpdateCameraFraming(); Playback.Advance(DeltaSeconds); ApplyFrame(); UpdateBroadcastCamera(DeltaSeconds);
    FString Capture;
    if (!bCaptureDone && StartupSeconds > 8 && FParse::Value(FCommandLine::Get(), TEXT("ReplayCapture="), Capture))
    {
        if (CaptureSeekTime < 0)
        {
            double At = 14.3; FParse::Value(FCommandLine::Get(), TEXT("ReplayCaptureAt="), At);
            Playback.SetPlaying(false); Playback.Seek(At); ApplyFrame(); CaptureSeekTime = StartupSeconds;
        }
        else if (StartupSeconds - CaptureSeekTime > 3)
        { FScreenshotRequest::RequestScreenshot(Capture, true, false); bCaptureDone = true; }
    }
}
void AMatchReplayActor::TogglePlayback()
{
    if (Playback.Position() >= Playback.GetReplay().Duration()) Playback.Seek(0);
    Playback.SetPlaying(!Playback.IsPlaying());
    UE_LOG(LogTemp, Display, TEXT("[ReplayViewer] %s at %.3fs"), Playback.IsPlaying() ? TEXT("Play") : TEXT("Pause"), Playback.Position());
}
void AMatchReplayActor::RestartPlayback()
{
    Playback.Seek(0); Playback.SetPlaying(true); ApplyFrame();
    UE_LOG(LogTemp, Display, TEXT("[ReplayViewer] Restart"));
}
void AMatchReplayActor::SeekFraction(float Fraction)
{
    Playback.Seek(Fraction * Playback.GetReplay().Duration()); ApplyFrame();
}
FText AMatchReplayActor::ClockText() const
{
    const int32 Seconds = FMath::FloorToInt(Playback.SourceTick() / 10);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d   |   %s"), Seconds / 60, Seconds % 60,
        Current.Phase == 2 ? TEXT("SELESAI") : Current.Phase == 1 ? TEXT("BABAK 2") : TEXT("BABAK 1")));
}
FText AMatchReplayActor::ProgressText() const
{
    return FText::FromString(FString::Printf(TEXT("%04.1f / %04.1f dtk"), Playback.Position(), Playback.GetReplay().Duration()));
}
FText AMatchReplayActor::GoalText() const
{
    if (const auto* Segment = Playback.ActiveSegment())
    {
        if (Segment->Kind == TEXT("halftime")) return FText::FromString(TEXT("JEDA BABAK"));
        if (Segment->Kind == TEXT("replay")) return FText::FromString(TEXT("REPLAY  0.5x"));
    }
    const auto& Frames = Playback.GetReplay().Frames;
    for (int32 I = Frames.Num() - 1; I >= 0; --I)
        if (Frames[I].Tick <= Current.Tick && (Frames[I].Flags & 1))
            return FText::FromString(Current.Tick - Frames[I].Tick < 90 ? TEXT("GOL!") : TEXT(""));
    return FText::GetEmpty();
}
void AMatchReplayActor::BuildOverlay()
{
    if (!GEngine || !GEngine->GameViewport) return;
    const auto Button = [](const FText& Label, FOnClicked Handler)
    {
        return SNew(SButton).IsFocusable(false).ContentPadding(FMargin(18,12)).OnClicked(Handler)
            [SNew(STextBlock).Text(Label).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))];
    };
    Overlay = SNew(SOverlay)
    + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(24)
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(22,16))
        [SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("FOOTBALLSIM"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",18)).ColorAndOpacity(FLinearColor::White)]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("REPLAY VIEWER"))).Font(FCoreStyle::GetDefaultFontStyle("Regular",10)).ColorAndOpacity(Muted)]]
        + SHorizontalBox::Slot().FillWidth(2).VAlign(VAlign_Center)
            [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%s     %u  -  %u     %s"), *Playback.GetReplay().HomeName, Current.HomeScore, Current.AwayScore, *Playback.GetReplay().AwayName));})
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",22)).ColorAndOpacity(FLinearColor::White)]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,5)[SNew(STextBlock).Text_Lambda([this]{return ClockText();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Muted)]]
        + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)
            [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(ClipLabel);}).ColorAndOpacity(FLinearColor::White).Font(FCoreStyle::GetDefaultFontStyle("Bold",13))]
            + SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).Text(FText::FromString(TEXT("Biru: home   /   Merah: away"))).ColorAndOpacity(Muted).Font(FCoreStyle::GetDefaultFontStyle("Regular",10))]]]]
    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,132,0,0)
        [SNew(STextBlock).Text_Lambda([this]{return ErrorMessage.IsEmpty() ? GoalText() : FText::FromString(ErrorMessage);})
            .Font(FCoreStyle::GetDefaultFontStyle("Bold",30)).ColorAndOpacity(FLinearColor(1,0.84f,0.15f)).ShadowOffset(FVector2D(2,2)).ShadowColorAndOpacity(FLinearColor::Black)]
    + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(24)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(22,14))
            [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
                [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)
                    [SNew(STextBlock).Text_Lambda([this]{return ProgressText();}).ColorAndOpacity(FLinearColor::White).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [SNew(SSlider).IsFocusable(false).Value_Lambda([this]{const double D = Playback.GetReplay().Duration(); return D > 0 ? float(Playback.Position()/D) : 0.0f;})
                        .OnValueChanged_Lambda([this](float Value){SeekFraction(Value);})
                        .OnMouseCaptureBegin_Lambda([this]{bScrubWasPlaying=Playback.IsPlaying();Playback.SetPlaying(false);})
                        .OnMouseCaptureEnd_Lambda([this]{Playback.SetPlaying(bScrubWasPlaying);})
                        .SliderHandleColor(FLinearColor(0.12f,0.74f,0.66f)).SliderBarColor(FLinearColor(0.16f,0.27f,0.32f))]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)
                [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
                    [SNew(SButton).IsFocusable(false).ContentPadding(FMargin(22,12)).ButtonColorAndOpacity(FLinearColor(0.09f,0.6f,0.5f))
                        .OnClicked_Lambda([this]{TogglePlayback();return FReply::Handled();})
                        [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Playback.IsPlaying() ? TEXT("Jeda") : TEXT("Putar"));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",13))]]
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Button(FText::FromString(TEXT("Ulang")), FOnClicked::CreateLambda([this]{RestartPlayback();return FReply::Handled();}))]
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
                    [SNew(SButton).IsFocusable(false).ContentPadding(FMargin(18,12)).OnClicked_Lambda([this]{Playback.SetRate(Playback.Rate()==0.5?1.0:0.5);UE_LOG(LogTemp,Display,TEXT("[ReplayViewer] Rate %.1fx"),Playback.Rate());return FReply::Handled();})
                        [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Playback.Rate()==0.5?TEXT("0.5x"):TEXT("1.0x"));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",13))]]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(12,0)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("SPASI  putar / jeda     R  ulang"))).ColorAndOpacity(Muted).Font(FCoreStyle::GetDefaultFontStyle("Regular",11))]
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Button(FText::FromString(TEXT("Highlight")),FOnClicked::CreateLambda([this]{LoadReplay(Example(TEXT("highlights_15min.fsim")));return FReply::Handled();}))]
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[Button(FText::FromString(TEXT("Klip gol")),FOnClicked::CreateLambda([this]{LoadReplay(Example(TEXT("first_goal_last15s.fsim")));return FReply::Handled();}))]
                + SHorizontalBox::Slot().AutoWidth()[Button(FText::FromString(TEXT("Akhir laga")),FOnClicked::CreateLambda([this]{LoadReplay(Example(TEXT("fulltime_last15s.fsim")));return FReply::Handled();}))]]]];
    GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
}
void AMatchReplayActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Overlay.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset();
    Super::EndPlay(Reason);
}
