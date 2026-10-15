#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "MatchBroadcastCamera.generated.h"

UCLASS()
class AMatchBroadcastCamera : public ACameraActor
{
    GENERATED_BODY()
public:
    FVector Focus = FVector::ZeroVector;
    bool bVerticalFraming = true;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
    static double VerticalShift(const FMinimalViewInfo& View, const FVector& Point);
};
