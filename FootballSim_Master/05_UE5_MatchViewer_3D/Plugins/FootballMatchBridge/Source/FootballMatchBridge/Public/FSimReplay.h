#pragma once
#include "CoreMinimal.h"

struct FOOTBALLMATCHBRIDGE_API FFSimReplayFrame
{
    uint64 Tick = 0, GoalSequence = 0;
    uint32 HomeScore = 0, AwayScore = 0, Phase = 0, Flags = 0;
    int32 BallOwner = -1;
    FVector Ball = FVector::ZeroVector, BallVelocity = FVector::ZeroVector;
    FVector Players[22]{};
    uint32 Actions[22]{};
};

struct FOOTBALLMATCHBRIDGE_API FFSimReplay
{
    FString HomeName, AwayName, Seed;
    TArray<FString> PlayerIds;
    TArray<FFSimReplayFrame> Frames;
    double Duration() const { return Frames.Num() > 1 ? double(Frames.Last().Tick - Frames[0].Tick) / 60.0 : 0; }
};

class FOOTBALLMATCHBRIDGE_API FFSimReplayReader
{
public:
    static bool LoadFile(const FString& Path, FFSimReplay& Out, FString& Error);
    static bool Decode(const TArray<uint8>& Bytes, FFSimReplay& Out, FString& Error);
    // FSIM metres (x length/y up/z width) -> UE centimetres (X/Y/Z up).
    static FVector ToUnreal(const FVector& Position) { return FVector(Position.X, Position.Z, Position.Y) * 100.0; }
};

class FOOTBALLMATCHBRIDGE_API FFSimReplayPlayback
{
public:
    void SetReplay(FFSimReplay InReplay);
    void Advance(double WallSeconds);
    void Seek(double PresentationSeconds);
    void SetPlaying(bool Value);
    void SetRate(double Value);
    bool IsPlaying() const { return bPlaying; }
    double Position() const { return Seconds; }
    double Rate() const { return PlaybackRate; }
    const FFSimReplay& GetReplay() const { return Replay; }
    bool Sample(FFSimReplayFrame& Out) const;
private:
    FFSimReplay Replay;
    double Seconds = 0, PlaybackRate = 0.5;
    bool bPlaying = false;
};
