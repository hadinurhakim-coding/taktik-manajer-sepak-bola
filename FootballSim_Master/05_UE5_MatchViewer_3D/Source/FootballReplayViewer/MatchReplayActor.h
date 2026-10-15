#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSimReplay.h"
#include "MatchReplayActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class ACameraActor;
class SWidget;

UCLASS()
class AMatchReplayActor : public AActor
{
    GENERATED_BODY()
public:
    AMatchReplayActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void TogglePlayback();
    void RestartPlayback();
    bool LoadReplay(const FString& File);
private:
    void BuildScene();
    void BuildOverlay();
    void UpdateCameraFraming();
    void ApplyFrame();
    void SeekFraction(float Fraction);
    FText ClockText() const;
    FText ProgressText() const;
    FText GoalText() const;
    UStaticMeshComponent* Shape(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, USceneComponent* Parent = nullptr, bool bFlat = false);
    void Line(FVector A, FVector B, float Width = 24);
    UMaterialInstanceDynamic* Material(FLinearColor Color, bool bFlat);
    FFSimReplayPlayback Playback;
    FFSimReplayFrame Current;
    FString ErrorMessage, ClipLabel;
    bool bScrubWasPlaying = false, bCaptureDone = false;
    double StartupSeconds = 0;
    FIntPoint FramedViewport = FIntPoint::ZeroValue;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UMaterialInterface> ColorMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> FlatMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> PlayerRoots;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Numbers;
    UPROPERTY() TObjectPtr<USceneComponent> BallRoot;
    UPROPERTY() TObjectPtr<USceneComponent> BallMarker;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    TSharedPtr<SWidget> Overlay;
};
