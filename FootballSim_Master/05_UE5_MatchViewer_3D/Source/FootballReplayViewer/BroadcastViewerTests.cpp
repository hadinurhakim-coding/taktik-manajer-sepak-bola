#include "MatchBroadcastCamera.h"
#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBroadcastFramingTest,"FootballMatchBridge.Broadcast.BallFramingAcrossPitch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBroadcastFramingTest::RunTest(const FString&)
{
    for(double X:{-5250.0,0.0,5250.0}) for(double Y:{-3400.0,0.0,3400.0})
    {
        const FVector Ball(X,Y,60);
        FMinimalViewInfo V; V.Location=FVector(X*0.75,-5000,1650); V.Rotation=(Ball-V.Location).Rotation();
        V.Rotation.Pitch=FMath::Clamp(V.Rotation.Pitch,-26.0,-23.0); V.FOV=35; V.AspectRatio=16.0f/9;
        V.OffCenterProjectionOffset.Y=AMatchBroadcastCamera::VerticalShift(V,Ball);
        const FVector Local=V.Rotation.UnrotateVector(Ball-V.Location);
        const FVector4 Clip=V.CalculateProjectionMatrix().TransformFVector4(FVector4(Local.Y,Local.Z,Local.X,1));
        TestTrue(TEXT("Ball remains inside broadcast frame across both touchlines and goal lines"),FMath::Abs(Clip.X/Clip.W)<1 && FMath::IsNearlyEqual(Clip.Y/Clip.W,-0.12,0.001));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBroadcastAssetsTest,"FootballMatchBridge.Broadcast.PersistentSkeletonAndAnimations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBroadcastAssetsTest::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Footballer01/Characters/SK_Footballer01.SK_Footballer01"));
    if(!TestNotNull(TEXT("Character mesh on disk"),Mesh)) return false;
    TestNotNull(TEXT("Skeleton dependency survives fresh process"),Mesh->GetSkeleton());
    TestTrue(TEXT("New humanoid rig with fingers"),Mesh->GetRefSkeleton().GetNum()>=42);
    TestTrue(TEXT("New pelvis exists"),Mesh->GetRefSkeleton().FindBoneIndex(TEXT("Pelvis"))!=INDEX_NONE);
    TestTrue(TEXT("Legacy hip bone removed"),Mesh->GetRefSkeleton().FindBoneIndex(TEXT("Hips"))==INDEX_NONE);
    for(const TCHAR* Name:{TEXT("idle"),TEXT("jog"),TEXT("sprint"),TEXT("dribble"),TEXT("pass"),TEXT("shot"),TEXT("tackle_slide"),TEXT("gk_ready"),TEXT("gk_dive_left"),TEXT("gk_dive_right")})
    {
        auto* A=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Footballer01/Animations/A_%s.A_%s"),Name,Name));
        if(!TestNotNull(Name,A)) continue;
        TestTrue(TEXT("Animation uses persisted shared skeleton"),A->GetSkeleton()!=nullptr && A->GetSkeleton()==Mesh->GetSkeleton());
        TestTrue(TEXT("Animation contains bone tracks and duration"),A->GetPlayLength()>0 && A->GetDataModel()->GetNumBoneTracks()>10);
    }
    auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/Footballer01/Materials/M_Footballer01_Kit.M_Footballer01_Kit"));
    if(TestNotNull(TEXT("Textured kit material"),Material)) TestTrue(TEXT("Skeletal shader permutation enabled"),Material->GetUsageByFlag(MATUSAGE_SkeletalMesh));
    return true;
}
#endif
