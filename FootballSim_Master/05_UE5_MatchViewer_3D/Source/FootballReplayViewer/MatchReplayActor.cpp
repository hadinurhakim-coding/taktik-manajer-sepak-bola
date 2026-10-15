#include "MatchReplayActor.h"
#include "SMatchRadar.h"
#include "MatchBroadcastCamera.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
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
const FLinearColor Home(0.035f,0.22f,0.8f), Away(0.8f,0.035f,0.025f);
const FLinearColor Ink(0.008f,0.016f,0.025f,0.92f), Muted(0.65f,0.75f,0.79f), Accent(0.25f,0.95f,0.72f);
FString Example(const TCHAR* Name) { return FPaths::Combine(FPaths::ProjectContentDir(),TEXT("Replays"),Name); }
}
AMatchReplayActor::AMatchReplayActor()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ReplayScene")));
}
UMaterialInstanceDynamic* AMatchReplayActor::Material(FLinearColor Color,bool bFlat)
{
    auto* M=UMaterialInstanceDynamic::Create(bFlat?FlatMaterial:ColorMaterial,this);
    if(M) { M->SetVectorParameterValue(TEXT("Tint"),Color); Materials.Add(M); }
    return M;
}
UStaticMeshComponent* AMatchReplayActor::Shape(UStaticMesh* Mesh,FVector Location,FVector Scale,FLinearColor Color,USceneComponent* Parent,bool bFlat)
{
    auto* C=NewObject<UStaticMeshComponent>(this); C->SetupAttachment(Parent?Parent:GetRootComponent());
    C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetMaterial(0,Material(Color,bFlat)); C->SetCastShadow(!bFlat);
    C->SetRelativeLocation(Location); C->SetRelativeScale3D(Scale); C->RegisterComponent(); return C;
}
void AMatchReplayActor::Line(FVector A,FVector B,float Width)
{
    const FVector Delta=B-A;
    auto* C=Shape(Cube,(A+B)*0.5+FVector(0,0,1),FVector(Delta.Size()/100,Width/100,0.012),FLinearColor(0.85f,0.87f,0.8f));
    C->SetRelativeRotation(Delta.Rotation()); C->SetCastShadow(false);
}
void AMatchReplayActor::Tube(FVector A,FVector B,float Diameter,FLinearColor Color)
{
    auto* C=Shape(Cylinder,(A+B)*0.5,FVector(Diameter/100,Diameter/100,(B-A).Size()/100),Color);
    C->SetRelativeRotation(FRotationMatrix::MakeFromZ(B-A).Rotator());
}
void AMatchReplayActor::BuildScene()
{
    Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    ColorMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Viewer/M_StadiumSurface.M_StadiumSurface"));
    FlatMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Viewer/M_ViewerFlat.M_ViewerFlat"));
    if(!ColorMaterial) ColorMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if(!FlatMaterial) FlatMaterial=ColorMaterial;
    auto* Dome=Shape(Sphere,FVector::ZeroVector,FVector(1800),FLinearColor::White,nullptr,true);
    if(auto* SkyMat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Viewer/M_StadiumSky.M_StadiumSky"))) Dome->SetMaterial(0,SkyMat);
    Shape(Cube,FVector(0,0,-35),FVector(170,130,0.5),FLinearColor(0.04f,0.065f,0.04f));
    Shape(Cube,FVector(0,0,-15),FVector(116,76,0.2),FLinearColor(0.045f,0.15f,0.04f));
    for(int32 I=0;I<20;++I)
        Shape(Cube,FVector(-5250+262.5+I*525,0,-5),FVector(5.25,68,0.1),
            I%2?FLinearColor(0.065f,0.205f,0.045f):FLinearColor(0.08f,0.25f,0.055f));
    Line(FVector(-5250,-3400,0),FVector(5250,-3400,0)); Line(FVector(-5250,3400,0),FVector(5250,3400,0));
    Line(FVector(-5250,-3400,0),FVector(-5250,3400,0)); Line(FVector(5250,-3400,0),FVector(5250,3400,0));
    Line(FVector(0,-3400,0),FVector(0,3400,0));
    for(int32 I=0;I<96;++I)
    {
        const double A=I*2*PI/96,B=(I+1)*2*PI/96;
        Line(FVector(915*FMath::Cos(A),915*FMath::Sin(A),0),FVector(915*FMath::Cos(B),915*FMath::Sin(B),0));
    }
    Shape(Cylinder,FVector(0,0,1),FVector(0.18,0.18,0.015),FLinearColor::White)->SetCastShadow(false);
    for(double Side:{-1.0,1.0})
    {
        const double X=Side*5250;
        for(const auto& Box:{FVector2D(1650,2016),FVector2D(550,916)})
        {
            Line(FVector(X,-Box.Y,0),FVector(X-Side*Box.X,-Box.Y,0)); Line(FVector(X,Box.Y,0),FVector(X-Side*Box.X,Box.Y,0));
            Line(FVector(X-Side*Box.X,-Box.Y,0),FVector(X-Side*Box.X,Box.Y,0));
        }
        Shape(Cylinder,FVector(X-Side*1100,0,1),FVector(0.18,0.18,0.015),FLinearColor::White)->SetCastShadow(false);
        // Penalty arc outside the box.
        for(int32 I=0;I<32;++I)
        {
            double A=-0.92+I*1.84/32,B=-0.92+(I+1)*1.84/32;
            Line(FVector(X-Side*(1100+915*FMath::Cos(A)),915*FMath::Sin(A),0),FVector(X-Side*(1100+915*FMath::Cos(B)),915*FMath::Sin(B),0));
        }
        for(double Y:{-366.0,366.0})
        {
            Tube(FVector(X,Y,0),FVector(X,Y,244),10,FLinearColor::White);
            Tube(FVector(X,Y,244),FVector(X+Side*200,Y,244),5,FLinearColor::White);
            Tube(FVector(X+Side*200,Y,0),FVector(X+Side*200,Y,244),5,FLinearColor::White);
            for(int32 H=0;H<=10;++H) Tube(FVector(X,Y,H*24.4),FVector(X+Side*200,Y,H*24.4),1.2,FLinearColor(0.65f,0.7f,0.72f));
            for(int32 D=0;D<=8;++D) Tube(FVector(X+Side*D*25,Y,0),FVector(X+Side*D*25,Y,244),1.2,FLinearColor(0.65f,0.7f,0.72f));
        }
        Tube(FVector(X,-366,244),FVector(X,366,244),10,FLinearColor::White);
        for(int32 N=0;N<=30;++N)
        {
            const double Y=-366+N*24.4;
            Tube(FVector(X+Side*200,Y,0),FVector(X+Side*200,Y,244),1.2,FLinearColor(0.65f,0.7f,0.72f));
            Tube(FVector(X,Y,244),FVector(X+Side*200,Y,244),1.2,FLinearColor(0.65f,0.7f,0.72f));
        }
        for(int32 H=0;H<=10;++H) Tube(FVector(X+Side*200,-366,H*24.4),FVector(X+Side*200,366,H*24.4),1.2,FLinearColor(0.65f,0.7f,0.72f));
    }
    BuildStadium(); BuildPlayers();
    BallRoot=NewObject<USceneComponent>(this); BallRoot->SetupAttachment(RootComponent); BallRoot->RegisterComponent();
    Shape(Sphere,FVector::ZeroVector,FVector(0.22),FLinearColor(0.92f,0.94f,0.9f),BallRoot);
    for(const FVector V:{FVector(0,-10,0),FVector(9,3,4),FVector(-4,5,8)})
        Shape(Sphere,V,FVector(0.065),FLinearColor(0.008f,0.015f,0.025f),BallRoot);
    PossessionRing=NewObject<USceneComponent>(this); PossessionRing->SetupAttachment(RootComponent); PossessionRing->RegisterComponent();
    for(int32 I=0;I<48;++I)
    {
        const double A=I*2*PI/48,B=(I+1)*2*PI/48;
        const FVector P(48*FMath::Cos(A),48*FMath::Sin(A),2),Q(48*FMath::Cos(B),48*FMath::Sin(B),2);
        auto* C=Shape(Cube,(P+Q)*0.5,FVector((Q-P).Size()/100,0.035,0.012),Accent,PossessionRing,true);
        C->SetRelativeRotation((Q-P).Rotation());
    }
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,5000),FRotator(-40,-35,0));
    auto* Light=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
    Light->SetMobility(EComponentMobility::Movable); Light->SetIntensity(3.5f); Light->SetCastShadows(true);
    Light->DynamicShadowDistanceMovableLight=18000; Light->DynamicShadowCascades=4; Light->ShadowBias=0.3f;
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>(); Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.0f); Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(0.32f,0.38f,0.46f));
    Sky->GetLightComponent()->SetLightColor(FLinearColor(0.75f,0.84f,1)); Sky->GetLightComponent()->RecaptureSky();
    Camera=GetWorld()->SpawnActor<AMatchBroadcastCamera>(); Camera->SetActorLocation(FVector(0,-5000,1650)); Camera->SetActorRotation(FRotator(-24.5,90,0));
    auto* C=Camera->GetCameraComponent(); C->ProjectionMode=ECameraProjectionMode::Perspective; C->SetFieldOfView(35);
    C->bConstrainAspectRatio=false; C->bOverrideAspectRatioAxisConstraint=true; C->AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
    C->PostProcessSettings.bOverride_AutoExposureMethod=true; C->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    C->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; C->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    C->PostProcessSettings.bOverride_AutoExposureBias=true; C->PostProcessSettings.AutoExposureBias=0;
    C->PostProcessSettings.bOverride_BloomIntensity=true; C->PostProcessSettings.BloomIntensity=0.15;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->SetViewTarget(Camera);
}
void AMatchReplayActor::BuildStadium()
{
    // Closed four-sided bowl. Instanced seats/crowd keep the geometry affordable.
    TArray<UInstancedStaticMeshComponent*> Seats,Crowds;
    for(const FLinearColor Color:{FLinearColor(0.015f,0.045f,0.10f),FLinearColor(0.06f,0.16f,0.3f),FLinearColor(0.32f,0.34f,0.34f),FLinearColor(0.58f,0.58f,0.5f)})
    {
        auto Make=[&](UStaticMesh* Mesh) { auto* C=NewObject<UInstancedStaticMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetStaticMesh(Mesh); C->SetMaterial(0,Material(Color,false)); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->RegisterComponent(); return C; };
        Seats.Add(Make(Cube)); Crowds.Add(Make(Sphere));
    }
    for(int32 Side=0;Side<4;++Side)
    {
        const bool End=Side>=2; const double Sign=Side%2?1:-1;
        const double Half=End?4200:6800,Edge=End?5950:4250;
        auto P=[&](double Along,double Depth,double Z) { return End?FVector(Sign*(Edge+Depth),Along,Z):FVector(Along,Sign*(Edge+Depth),Z); };
        for(int32 Row=0;Row<18;++Row)
        {
            const double Z=90+Row*70,Depth=Row*105;
            Shape(Cube,P(0,Depth,Z*0.5),End?FVector(1.08,Half*2/100,Z/100):FVector(Half*2/100,1.08,Z/100),FLinearColor(0.14f,0.16f,0.18f));
            for(int32 J=0;J<int32(Half*2/65);++J)
            {
                if(J%22<2) continue; // Stair aisles.
                FVector Location=P(-Half+J*65,Depth,Z+20);
                Seats[(Row/4+J/22)%4]->AddInstance(FTransform(FRotator::ZeroRotator,Location,FVector(0.48,0.48,0.18)));
                if((J*13+Row*7+Side)%5)
                {
                    const int32 Color=(J*17+Row*3)%4;
                    Crowds[Color]->AddInstance(FTransform(FRotator::ZeroRotator,Location+FVector(0,0,38),FVector(0.35,0.28,0.65)));
                    Crowds[(Color+1)%4]->AddInstance(FTransform(FRotator::ZeroRotator,Location+FVector(0,0,77),FVector(0.23)));
                }
            }
        }
        Shape(Cube,P(0,1900,1500),End?FVector(0.4,Half*2/100,8):FVector(Half*2/100,0.4,8),FLinearColor(0.035f,0.06f,0.09f));
        Shape(Cube,P(0,1200,2050),End?FVector(19,Half*2/100,0.3):FVector(Half*2/100,19,0.3),FLinearColor(0.06f,0.08f,0.11f));
    }
    for(int32 Side=0;Side<4;++Side)
    {
        const bool End=Side>=2; const double Sign=Side%2?1:-1;
        const int32 Count=End?12:20;
        for(int32 I=0;I<Count;++I)
        {
            const double Along=(I-(Count-1)*0.5)*540;
            const FVector Location=End?FVector(Sign*5650,Along,55):FVector(Along,Sign*3700,55);
            const FLinearColor Color=I%3==0?FLinearColor(0.035f,0.22f,0.29f):FLinearColor(0.012f,0.035f,0.065f);
            Shape(Cube,Location,End?FVector(0.15,5.35,1.1):FVector(5.35,0.15,1.1),Color,nullptr,true);
            auto* Text=NewObject<UTextRenderComponent>(this); Text->SetupAttachment(RootComponent);
            Text->SetRelativeLocation(Location+(End?FVector(-Sign*9,0,0):FVector(0,-Sign*9,0)));
            Text->SetRelativeRotation(End?FRotator(0,Sign>0?180:0,0):FRotator(0,Sign>0?-90:90,0));
            Text->SetText(FText::FromString(I%3==0?TEXT("FOOTBALLSIM"):I%3==1?TEXT("THE BEAUTIFUL GAME"):TEXT("MATCHDAY  /  2026")));
            Text->SetHorizontalAlignment(EHTA_Center); Text->SetVerticalAlignment(EVRTA_TextCenter); Text->SetWorldSize(35);
            Text->SetTextRenderColor(FColor(215,240,245)); Text->RegisterComponent();
        }
    }
}
void AMatchReplayActor::BuildPlayers()
{
    CharacterMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Footballer01/Characters/SK_Footballer01.SK_Footballer01"));
    if(!CharacterMesh) { ErrorMessage=TEXT("Karakter belum tersedia; jalankan setup_assets.py"); UE_LOG(LogTemp,Error,TEXT("[Stage32] Missing skeletal mesh")); return; }
    for(const TCHAR* Name:{TEXT("idle"),TEXT("jog"),TEXT("sprint"),TEXT("dribble"),TEXT("pass"),TEXT("shot"),TEXT("tackle_slide"),TEXT("gk_ready"),TEXT("gk_dive_left"),TEXT("gk_dive_right"),TEXT("gk_smother"),TEXT("gk_pass")})
        if(auto* A=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Footballer01/Animations/A_%s.A_%s"),Name,Name))) Animations.Add(FName(Name),A);
    const auto Bounds=CharacterMesh->GetBounds();
    const float Scale=180.0f/FMath::Max(1.0,double(Bounds.BoxExtent.Z*2));
    auto* Kit=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Footballer01/Materials/M_Footballer01_Kit.M_Footballer01_Kit"));
    for(int32 I=0;I<22;++I)
    {
        auto* P=NewObject<USceneComponent>(this); P->SetupAttachment(RootComponent); P->RegisterComponent(); PlayerRoots.Add(P);
        auto* Mesh=NewObject<USkeletalMeshComponent>(this); Mesh->SetupAttachment(P); Mesh->SetSkeletalMeshAsset(CharacterMesh);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(true);
        Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode); Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Mesh->SetRelativeScale3D(FVector(Scale)); Mesh->SetRelativeLocation(FVector(0,0,-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
        // Source Blender faces -Y; UE movement uses +X forward.
        Mesh->SetRelativeRotation(FRotator(0,90,0)); Mesh->GlobalAnimRateScale=0; Mesh->RegisterComponent();
        const FLinearColor TeamColor=I%11==0?(I<11?FLinearColor(0.9f,0.6f,0.02f):FLinearColor(0.35f,0.06f,0.65f)):(I<11?Home:Away);
        if(Kit)
        {
            auto* M=UMaterialInstanceDynamic::Create(Kit,this); M->SetVectorParameterValue(TEXT("KitTint"),TeamColor); Materials.Add(M);
            Mesh->SetMaterial(0,M); // Separate hand material retains its skin tone.
        }
        SkeletalPlayers.Add(Mesh);
    }
    UE_LOG(LogTemp,Display,TEXT("[Stage32] 22 skeletal players, 180cm bind height, scale %.4f, %d animation states, bounds %s"),Scale,Animations.Num(),*Bounds.BoxExtent.ToString());
}
void AMatchReplayActor::UpdateCameraFraming()
{
    int32 W=0,H=0; if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->GetViewportSize(W,H);
    if(W>0&&H>0) { FramedViewport=FIntPoint(W,H); if(Camera) Camera->GetCameraComponent()->SetAspectRatio(float(W)/H); }
}
bool AMatchReplayActor::LoadReplay(const FString& File)
{
    FFSimReplay Data; if(!FFSimReplayReader::LoadFile(File,Data,ErrorMessage)) { UE_LOG(LogTemp,Error,TEXT("[ReplayViewer] %s"),*ErrorMessage); return false; }
    Playback.SetReplay(MoveTemp(Data)); ClipLabel=Playback.GetReplay().Version==2?TEXT("HIGHLIGHTS  /  15 MIN"):TEXT("MATCH REPLAY");
    LastSourceTick=CameraSourceTick=-1; LastCarrier[0]=LastCarrier[1]=-1; ErrorMessage.Reset(); ApplyFrame();
    UE_LOG(LogTemp,Display,TEXT("[ReplayViewer] Loaded %s, %.2fs"),*File,Playback.GetReplay().Duration()); return true;
}
void AMatchReplayActor::BeginPlay()
{
    Super::BeginPlay(); BuildScene(); FString File=Example(TEXT("highlights_15min.fsim")); FParse::Value(FCommandLine::Get(),TEXT("ReplayFile="),File); LoadReplay(File);
    FParse::Value(FCommandLine::Get(),TEXT("Stage32CaptureDir="),CaptureDirectory);
    if(FParse::Param(FCommandLine::Get(),TEXT("ReplayAutoplay"))) Playback.SetPlaying(true);
    BuildOverlay();
    // Pick actual active play from the edit timeline; preserve match coordinates.
    for(const auto& S:Playback.GetReplay().Segments)
    {
        if(CaptureBroadcastAt==0 && S.Kind==TEXT("live") && S.FirstFrame>100 && S.Duration>6)
            for(int32 F=S.FirstFrame+3;F<S.LastFrame-10;++F)
            {
                const auto& A=Playback.GetReplay().Frames[F];
                if(A.BallOwner>=0 && A.BallOwner%11!=0 && FMath::Abs(A.Ball.X)<30 && A.Ball.Z>-18 && A.Ball.Z<-5 && A.BallVelocity.Size()>2)
                { CaptureBroadcastAt=S.StartSeconds+(F-S.FirstFrame)*0.3; break; }
            }
        if(CaptureReplayAt==0 && S.Kind==TEXT("replay"))
        {
            const auto& F=Playback.GetReplay().Frames[S.LastFrame];
            if((F.Flags&1) || FMath::Abs(F.Ball.X)>42) CaptureReplayAt=S.StartSeconds+FMath::Min(S.Duration-1,8.0);
        }
    }
    FParse::Value(FCommandLine::Get(),TEXT("BroadcastAt="),CaptureBroadcastAt); FParse::Value(FCommandLine::Get(),TEXT("CloseupAt="),CaptureReplayAt);
}
void AMatchReplayActor::ApplyFrame()
{
    if(!Playback.Sample(Current)) return;
    for(int32 I=0;I<PlayerRoots.Num();++I) { PlayerRoots[I]->SetWorldLocation(FFSimReplayReader::ToUnreal(Current.Players[I])); AnimatePlayer(I); }
    if(BallRoot) BallRoot->SetWorldLocation(FFSimReplayReader::ToUnreal(Current.Ball));
    if(PossessionRing)
    {
        PossessionRing->SetVisibility(Current.BallOwner>=0,true);
        if(Current.BallOwner>=0) PossessionRing->SetWorldLocation(FFSimReplayReader::ToUnreal(Current.Players[Current.BallOwner]));
    }
    if(Current.BallOwner>=0) { const int32 Team=Current.BallOwner/11; LastCarrier[Team]=Current.BallOwner; LastCarrierTick[Team]=Playback.SourceTick(); }
    LastSourceTick=Playback.SourceTick();
}
void AMatchReplayActor::AnimatePlayer(int32 I)
{
    auto* Mesh=SkeletalPlayers[I].Get(); if(!Mesh) return;
    const auto& Frames=Playback.GetReplay().Frames; const int32 Frame=FMath::Clamp(FMath::FloorToInt(Playback.SourceFrame()),0,Frames.Num()-1);
    FVector Motion=FVector::ZeroVector;
    if(Frame+1<Frames.Num() && !(Frames[Frame+1].Flags&6)) Motion=Frames[Frame+1].Players[I]-Frames[Frame].Players[I];
    const double Speed=Motion.Size()/0.3; const bool Keeper=I%11==0;
    FName Name=Speed<0.25?(Keeper?TEXT("gk_ready"):TEXT("idle")):Speed<5?TEXT("jog"):TEXT("sprint");
    double ActionTick=-1;
    // Resolve kicks from possession release, not the AI's unexecuted intention.
    for(int32 F=Frame;F>=FMath::Max(1,Frame-4);--F)
    {
        const auto& Before=Frames[F-1]; const auto& After=Frames[F];
        if(Before.BallOwner==I && After.BallOwner!=I && (Before.Actions[I]==0 || Before.Actions[I]==2))
        { Name=Before.Actions[I]==2?TEXT("shot"):(Keeper?TEXT("gk_pass"):TEXT("pass")); ActionTick=Before.Tick; break; }
        if(Keeper && After.BallOwner==I && (After.Flags&16))
        { Name=(After.Flags&32)?(After.Ball.Z>=After.Players[I].Z?TEXT("gk_dive_right"):TEXT("gk_dive_left")):TEXT("gk_smother"); ActionTick=After.Tick; break; }
    }
    if(ActionTick<0 && Current.BallOwner==I && Speed>0.3) Name=Keeper?TEXT("jog"):TEXT("dribble");
    if(!Keeper && Current.Actions[I]==3 && FVector::Dist(Current.Players[I],Current.Ball)<2) Name=TEXT("tackle_slide");
    if(Name!=LastAnimations[I]) { LastAnimations[I]=Name; if(const auto* A=Animations.Find(Name)) Mesh->PlayAnimation(*A,true); Mesh->GlobalAnimRateScale=0; }
    if(const auto* A=Animations.Find(Name))
    {
        const double Duration=(*A)->GetPlayLength();
        // Source-time evaluation makes seeking, pause and 0.5x poses reproducible.
        const double Pose=ActionTick>=0?FMath::Min((Playback.SourceTick()-ActionTick)/10.0,Duration-0.001):FMath::Fmod(Playback.SourceTick()/10.0+I*0.071,Duration);
        Mesh->SetPosition(float(FMath::Max(0.0,Pose)),false); Mesh->TickAnimation(0,false); Mesh->RefreshBoneTransforms();
        // Footballer01 clips are authored in place. Pelvis motion is intentional
        // articulation; recorded replay positions alone move the player root.
    }
    if(Motion.SizeSquared()>0.0001) PlayerRoots[I]->SetWorldRotation(FRotator(0,FFSimReplayReader::ToUnreal(Motion).Rotation().Yaw,0));
}
void AMatchReplayActor::UpdateBroadcastCamera(float DeltaSeconds)
{
    if(!Camera) return;
    const auto* S=Playback.ActiveSegment(); const bool Alternate=(S&&S->Camera==TEXT("alternate")) || Playback.Rate()==0.5;
    const FVector Ball=FFSimReplayReader::ToUnreal(Current.Ball);
    FVector Location(Ball.X*0.75,-5000,1650),Focus=Ball+FVector(0,0,60);
    float FOV=35;
    if(Alternate)
    {
        const double Side=Ball.X>=0?1:-1;
        // Low action camera on the field side, with the goal in the background.
        // Staying in front of the net prevents goal bars obscuring the players.
        Location=FVector(Ball.X-Side*950,Ball.Y-400,240);
        const FVector Offset=Location-Focus;
        const double Horizontal=FVector2D(Offset.X,Offset.Y).Size();
        if(Horizontal<800) Location.Y=Ball.Y-FMath::Sqrt(900.0*900.0-FMath::Square(Location.X-Ball.X));
        FOV=50;
    }
    FRotator Rotation=(Focus-Location).Rotation();
    if(!Alternate) Rotation.Pitch=FMath::Clamp(Rotation.Pitch,-26.0,-23.0);
    const double Tick=Playback.SourceTick();
    const bool Cut=CameraSourceTick<0 || Alternate!=bLastAlternate || Tick<CameraSourceTick || Tick-CameraSourceTick>15;
    FVector Eye=Cut?Location:FMath::VInterpTo(Camera->GetActorLocation(),Location,DeltaSeconds,4);
    if(Alternate)
    {
        FVector Offset=Eye-Ball; Offset.Z=0;
        const double Distance=Offset.Size();
        const double Vertical=Eye.Z-Ball.Z;
        const double MinHorizontal=FMath::Sqrt(FMath::Max(0.0,800.0*800.0-Vertical*Vertical));
        const double MaxHorizontal=FMath::Sqrt(FMath::Max(0.0,1200.0*1200.0-Vertical*Vertical));
        if(Distance>0) { Offset*=FMath::Clamp(Distance,MinHorizontal,MaxHorizontal)/Distance; Eye.X=Ball.X+Offset.X; Eye.Y=Ball.Y+Offset.Y; }
    }
    Camera->SetActorLocation(Eye);
    Camera->SetActorRotation(Cut?Rotation:FMath::RInterpTo(Camera->GetActorRotation(),Rotation,DeltaSeconds,5));
    Camera->GetCameraComponent()->SetFieldOfView(FOV);
    if(auto* Broadcast=Cast<AMatchBroadcastCamera>(Camera)) { Broadcast->Focus=Focus; Broadcast->bVerticalFraming=!Alternate; }
    CameraSourceTick=Tick; bLastAlternate=Alternate;
}
void AMatchReplayActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); StartupSeconds+=DeltaSeconds; UpdateCameraFraming(); Playback.Advance(DeltaSeconds); ApplyFrame(); UpdateBroadcastCamera(DeltaSeconds); CaptureAutomation();
}
void AMatchReplayActor::CaptureAutomation()
{
    if(CaptureDirectory.IsEmpty()) return;
    if(CaptureStage==0 && StartupSeconds>10)
    {
        IFileManager::Get().MakeDirectory(*CaptureDirectory,true);
        Playback.Seek(CaptureBroadcastAt); Playback.SetPlaying(true); CaptureStage=1; CaptureStageTime=StartupSeconds;
    }
    else if((CaptureStage==1||CaptureStage==3) && StartupSeconds-CaptureStageTime>2)
    {
        const FString Name=CaptureStage==1?TEXT("stage3_2_broadcast_3d"):TEXT("stage3_2_replay_closeup");
        PendingCapture=FPaths::Combine(CaptureDirectory,Name+TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(PendingCapture,true,false);
        WriteCaptureDiagnostics(FPaths::Combine(CaptureDirectory,Name+TEXT(".json")));
        ++CaptureStage; CaptureStageTime=StartupSeconds;
    }
    else if(CaptureStage==2 && StartupSeconds-CaptureStageTime>2 && IFileManager::Get().FileSize(*PendingCapture)>0)
    { Playback.Seek(CaptureReplayAt); Playback.SetPlaying(true); CaptureStage=3; CaptureStageTime=StartupSeconds; }
    else if(CaptureStage==4 && StartupSeconds-CaptureStageTime>3 && IFileManager::Get().FileSize(*PendingCapture)>0)
    { UE_LOG(LogTemp,Display,TEXT("STAGE32_CAPTURE_COMPLETE")); CaptureStage=5; FPlatformMisc::RequestExit(false); }
    if(StartupSeconds>75 && CaptureStage<5) { UE_LOG(LogTemp,Error,TEXT("STAGE32_CAPTURE_TIMEOUT")); FPlatformMisc::RequestExit(false); }
}
void AMatchReplayActor::WriteCaptureDiagnostics(const FString& Path) const
{
    auto J=MakeShared<FJsonObject>(); J->SetNumberField(TEXT("source_tick"),Playback.SourceTick()); J->SetNumberField(TEXT("timeline_seconds"),Playback.Position());
    J->SetStringField(TEXT("projection"),TEXT("Perspective")); J->SetNumberField(TEXT("fov"),Camera->GetCameraComponent()->FieldOfView);
    J->SetStringField(TEXT("camera_position"),Camera->GetActorLocation().ToString()); J->SetStringField(TEXT("camera_rotation"),Camera->GetActorRotation().ToString());
    J->SetStringField(TEXT("ball_position"),FFSimReplayReader::ToUnreal(Current.Ball).ToString()); J->SetNumberField(TEXT("skeletal_players"),SkeletalPlayers.Num());
    J->SetBoolField(TEXT("playing"),Playback.IsPlaying()); J->SetBoolField(TEXT("low_angle"),bLastAlternate);
    TArray<TSharedPtr<FJsonValue>> BoneNames;
    if(!SkeletalPlayers.IsEmpty()) { TArray<FName> Names; SkeletalPlayers[0]->GetBoneNames(Names); for(const auto Name:Names) BoneNames.Add(MakeShared<FJsonValueString>(Name.ToString())); }
    J->SetArrayField(TEXT("bone_names"),BoneNames);
    const auto* S=Playback.ActiveSegment(); J->SetNumberField(TEXT("effective_rate"),Playback.Rate()*(S?S->Rate:1));
    TArray<TSharedPtr<FJsonValue>> Players;
    for(int32 I=0;I<SkeletalPlayers.Num();++I)
    {
        auto P=MakeShared<FJsonObject>(); P->SetNumberField(TEXT("index"),I); P->SetStringField(TEXT("animation"),LastAnimations[I].ToString());
        P->SetStringField(TEXT("mesh_bounds"),SkeletalPlayers[I]->Bounds.BoxExtent.ToString());
        P->SetStringField(TEXT("location"),PlayerRoots[I]->GetComponentLocation().ToString());
        P->SetStringField(TEXT("head"),SkeletalPlayers[I]->GetBoneLocation(TEXT("Head")).ToString());
        P->SetStringField(TEXT("left_hand"),SkeletalPlayers[I]->GetBoneLocation(TEXT("Hand_L")).ToString());
        P->SetNumberField(TEXT("pose_seconds"),SkeletalPlayers[I]->GetPosition());
        Players.Add(MakeShared<FJsonValueObject>(P));
    }
    J->SetArrayField(TEXT("players"),Players); FString Text; FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text,*Path);
}
void AMatchReplayActor::TogglePlayback() { if(Playback.Position()>=Playback.GetReplay().Duration()) Playback.Seek(0); Playback.SetPlaying(!Playback.IsPlaying()); }
void AMatchReplayActor::RestartPlayback() { Playback.Seek(0); Playback.SetPlaying(true); ApplyFrame(); }
void AMatchReplayActor::SeekFraction(float Fraction) { Playback.Seek(Fraction*Playback.GetReplay().Duration()); ApplyFrame(); }
FText AMatchReplayActor::ClockText() const
{
    const int32 Seconds=FMath::FloorToInt(Playback.SourceTick()/10);
    const auto* S=Playback.ActiveSegment();
    return FText::FromString(FString::Printf(TEXT("%02d:%02d  |  %s"),Seconds/60,Seconds%60,S&&S->Kind==TEXT("halftime")?TEXT("JEDA"):Current.Phase==2?TEXT("FT"):Current.Phase==1?TEXT("BABAK 2"):TEXT("BABAK 1")));
}
FText AMatchReplayActor::ProgressText() const { return FText::FromString(FString::Printf(TEXT("%02d:%02d / 15:00"),int32(Playback.Position())/60,int32(Playback.Position())%60)); }
FText AMatchReplayActor::GoalText() const
{
    const auto* S=Playback.ActiveSegment();
    if(S&&S->Kind==TEXT("halftime")) return FText::FromString(TEXT("HALF TIME"));
    if(bLastAlternate) return FText::FromString(FString::Printf(TEXT("REPLAY  /  %gx"),Playback.Rate()*(S?S->Rate:1)));
    return FText::FromString(Playback.IsPlaying()?TEXT("LIVE  /  HIGHLIGHTS"):TEXT("PAUSED"));
}
FText AMatchReplayActor::PossessionText(int32 Team) const
{
    const int32 I=Current.BallOwner>=0&&Current.BallOwner/11==Team?Current.BallOwner:LastCarrier[Team];
    const FString Club=Team==0?Playback.GetReplay().HomeName:Playback.GetReplay().AwayName;
    if(I<0) return FText::FromString(Club);
    const FString Name=Playback.GetReplay().PlayerNames.IsValidIndex(I)?Playback.GetReplay().PlayerNames[I]:FString::Printf(TEXT("Pemain %d"),I%11+1);
    return FText::FromString(FString::Printf(TEXT("%02d  %s"),I%11+1,*Name));
}
void AMatchReplayActor::BuildOverlay()
{
    if(!GEngine||!GEngine->GameViewport) return;
    const auto Text=[](FText T,int32 Size,FLinearColor Color) { return SNew(STextBlock).Text(T).Font(FCoreStyle::GetDefaultFontStyle("Bold",Size)).ColorAndOpacity(Color); };
    const auto Nameplate=[this](int32 Team)
    {
        return SNew(SBox).WidthOverride(300)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(18,12))
        [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this,Team]{return FText::FromString(Team==0?Playback.GetReplay().HomeName:Playback.GetReplay().AwayName);}).Font(FCoreStyle::GetDefaultFontStyle("Regular",10)).ColorAndOpacity(Team==0?FLinearColor(0.2f,0.65f,1):FLinearColor(1,0.3f,0.2f))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).Text_Lambda([this,Team]{return PossessionText(Team);}).Font(FCoreStyle::GetDefaultFontStyle("Bold",16)).ColorAndOpacity(FLinearColor::White)]]];
    };
    Overlay=SNew(SOverlay)
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36,30)
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(18,12))
    [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(FText::FromString(TEXT("FOOTBALLSIM   /   MATCHDAY")),10,Accent)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%s   %u - %u   %s"),*Playback.GetReplay().HomeName,Current.HomeScore,Current.AwayScore,*Playback.GetReplay().AwayName));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",18)).ColorAndOpacity(FLinearColor::White)]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return ClockText();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",12)).ColorAndOpacity(Muted)]]]
    +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(36,35)
    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(16,10))
    [SNew(STextBlock).Text_Lambda([this]{return ErrorMessage.IsEmpty()?GoalText():FText::FromString(ErrorMessage);}).Font(FCoreStyle::GetDefaultFontStyle("Bold",12)).ColorAndOpacity(Accent)]]
    +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,54)
    [SNew(SBox).WidthOverride(240).HeightOverride(150)[SNew(SMatchRadar).Frame(&Current)]]
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(36,0,0,64)[Nameplate(0)]
    +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,36,64)[Nameplate(1)]
    +SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(36,0,36,16)
    [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(10,4)).OnClicked_Lambda([this]{TogglePlayback();return FReply::Handled();})[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Playback.IsPlaying()?TEXT("Jeda"):TEXT("Putar"));})]]
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(10,4)).OnClicked_Lambda([this]{RestartPlayback();return FReply::Handled();})[Text(FText::FromString(TEXT("Ulang")),10,FLinearColor::White)]]
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(10,4)).OnClicked_Lambda([this]{Playback.SetRate(Playback.Rate()==1?0.5:1);return FReply::Handled();})[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Playback.Rate()==1?TEXT("1x"):TEXT("0.5x"));})]]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider).IsFocusable(false).Value_Lambda([this]{return Playback.GetReplay().Duration()>0?float(Playback.Position()/Playback.GetReplay().Duration()):0;}).OnValueChanged_Lambda([this](float F){SeekFraction(F);}).OnMouseCaptureBegin_Lambda([this]{bScrubWasPlaying=Playback.IsPlaying();Playback.SetPlaying(false);}).OnMouseCaptureEnd_Lambda([this]{Playback.SetPlaying(bScrubWasPlaying);}).SliderHandleColor(Accent)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)[SNew(STextBlock).Text_Lambda([this]{return ProgressText();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",10)).ColorAndOpacity(FLinearColor::White)]];
    GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(),10);
}
void AMatchReplayActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Overlay.IsValid()&&GEngine&&GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset(); Super::EndPlay(Reason);
}
