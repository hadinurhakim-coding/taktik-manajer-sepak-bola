#include "FSimReplay.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSimReaderTest, "FootballMatchBridge.Reader.ValidAndCorruptFiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSimReaderTest::RunTest(const FString& Parameters)
{
    const FString File = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Replays/first_goal_last15s.fsim"));
    FFSimReplay Replay; FString Error;
    if (!TestTrue(TEXT("Real FSIM fixture loads"), FFSimReplayReader::LoadFile(File, Replay, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("22 identities"), Replay.PlayerIds.Num(), 22);
    TestEqual(TEXT("301 frames"), Replay.Frames.Num(), 301);
    TestEqual(TEXT("15 presentation seconds"), Replay.Duration(), 15.0);
    TestEqual(TEXT("Home name"), Replay.HomeName, FString(TEXT("AS Roma")));
    TestEqual(TEXT("Metres/y-up conversion"), FFSimReplayReader::ToUnreal(FVector(1,2,3)), FVector(100,300,200));
    TArray<uint8> Bytes; FFileHelper::LoadFileToArray(Bytes, *File);
    const auto Reject = [&](TArray<uint8> Bad, const TCHAR* Label)
    {
        TestFalse(Label, FFSimReplayReader::Decode(Bad, Replay, Error));
        TestEqual(TEXT("Failure preserves previous replay"), Replay.Frames.Num(), 301);
    };
    auto Bad = Bytes; Bad[0] = 'X'; Reject(Bad, TEXT("Bad magic"));
    Bad = Bytes; Bad.SetNum(Bad.Num()-1); Reject(Bad, TEXT("Truncated"));
    Bad = Bytes; Bad.Add(0); Reject(Bad, TEXT("Trailing bytes"));
    Bad = Bytes; Bad[8] = 2; Reject(Bad, TEXT("Unsupported version"));
    Bad = Bytes; Bad[28] = 6; Reject(Bad, TEXT("Unsupported sample interval"));
    const uint32 MetaSize = uint32(Bytes[16]) | (uint32(Bytes[17])<<8) | (uint32(Bytes[18])<<16) | (uint32(Bytes[19])<<24);
    const int32 First = 40 + static_cast<int32>(MetaSize);
    Bad = Bytes; Bad[First+38] = 0xc0; Bad[First+39] = 0x7f; Reject(Bad, TEXT("NaN ball coordinate"));
    Bad = Bytes; Bad[First+412] = Bytes[First]; Reject(Bad, TEXT("Nonconsecutive timestamp"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFSimPlaybackTest, "FootballMatchBridge.Playback.PauseRateSeekAndRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFSimPlaybackTest::RunTest(const FString& Parameters)
{
    FFSimReplay Replay; FString Error;
    if (!FFSimReplayReader::LoadFile(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Replays/first_goal_last15s.fsim")), Replay, Error)) { AddError(Error); return false; }
    FFSimReplayPlayback P; P.SetReplay(Replay);
    P.Advance(1); TestEqual(TEXT("Starts paused"), P.Position(), 0.0);
    P.SetPlaying(true); P.Advance(2); TestEqual(TEXT("0.5x two wall seconds -> one replay second"), P.Position(), 1.0);
    P.SetPlaying(false); P.Advance(5); TestEqual(TEXT("Pause holds"), P.Position(), 1.0);
    P.SetRate(1); P.SetPlaying(true); P.Advance(2); TestEqual(TEXT("1x"), P.Position(), 3.0);
    P.Seek(200); TestEqual(TEXT("Clamp end"), P.Position(), 15.0); TestFalse(TEXT("End stops"), P.IsPlaying());
    P.Seek(-10); TestEqual(TEXT("Clamp start"), P.Position(), 0.0);
    P.Seek(std::numeric_limits<double>::quiet_NaN()); TestEqual(TEXT("Ignore NaN seek"), P.Position(), 0.0);
    FFSimReplay Mini; FFSimReplayFrame A, B;
    A.Tick=0; A.Players[0]=FVector(0,0,0); A.HomeScore=0;
    B.Tick=3; B.Players[0]=FVector(10,0,0); B.HomeScore=1;
    Mini.Frames={A,B}; P.SetReplay(Mini); P.Seek(0.025);
    FFSimReplayFrame Sample; P.Sample(Sample);
    TestEqual(TEXT("Interpolate position"), Sample.Players[0].X, 5.0);
    TestEqual(TEXT("Score stays discrete before next frame"), Sample.HomeScore, 0U);
    Mini.Frames[1].Flags=2; P.SetReplay(Mini); P.Seek(0.025); P.Sample(Sample);
    TestEqual(TEXT("Hold before restart discontinuity"), Sample.Players[0].X, 0.0);
    P.Seek(0.05); P.Sample(Sample);
    TestEqual(TEXT("Jump exactly at restart"), Sample.Players[0].X, 10.0);
    TestEqual(TEXT("Score updates at frame"), Sample.HomeScore, 1U);
    return true;
}
#endif
