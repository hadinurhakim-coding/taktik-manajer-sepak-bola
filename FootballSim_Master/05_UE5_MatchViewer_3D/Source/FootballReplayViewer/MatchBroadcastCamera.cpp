#include "MatchBroadcastCamera.h"

double AMatchBroadcastCamera::VerticalShift(const FMinimalViewInfo& View,const FVector& Point)
{
    const FVector Local=View.Rotation.UnrotateVector(Point-View.Location);
    if(Local.X<1) return 0;
    const double TanHalfV=FMath::Tan(FMath::DegreesToRadians(View.FOV*0.5))/FMath::Max(0.1f,View.AspectRatio);
    return Local.Z/(Local.X*TanHalfV)+0.12;
}
void AMatchBroadcastCamera::CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(DeltaTime,OutResult);
    // Fixed Y/Z and -23..-26 pitch cannot geometrically look directly at every
    // touchline. Lens shift keeps the ball framed without breaking that rig.
    if(bVerticalFraming) OutResult.OffCenterProjectionOffset.Y=VerticalShift(OutResult,Focus);
}
