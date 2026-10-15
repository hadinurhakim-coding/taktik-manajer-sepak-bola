#include "FSimReplay.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FWireReader
{
    const TArray<uint8>& Data;
    int32 Offset = 0;
    uint32 U32() { uint32 V = 0; for (int32 I = 0; I < 4; ++I) V |= uint32(Data[Offset++]) << (8 * I); return V; }
    uint64 U64() { const uint64 Lo = U32(); return Lo | (uint64(U32()) << 32); }
    float Float() { const uint32 Bits = U32(); float V; FMemory::Memcpy(&V, &Bits, 4); return V; }
    FVector Vector() { const double X = Float(), Y = Float(), Z = Float(); return FVector(X, Y, Z); }
};
bool Finite(const FVector& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z) && V.GetAbsMax() < 100000;
}
bool NumberEquals(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double Expected)
{
    double Value; return Object->TryGetNumberField(Key, Value) && Value == Expected;
}
}

bool FFSimReplayReader::LoadFile(const FString& Path, FFSimReplay& Out, FString& Error)
{
    const int64 Size = IFileManager::Get().FileSize(*Path);
    if (Size < 40 || Size > 2 * 1024 * 1024) { Error = TEXT("File replay tidak ditemukan atau ukurannya tidak valid."); return false; }
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { Error = TEXT("File replay tidak dapat dibaca."); return false; }
    return Decode(Bytes, Out, Error);
}

bool FFSimReplayReader::Decode(const TArray<uint8>& Bytes, FFSimReplay& Out, FString& Error)
{
    Error.Reset();
    const auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
    static const uint8 Magic[8] = {'F','S','I','M',13,10,26,10};
    if (Bytes.Num() < 40 || FMemory::Memcmp(Bytes.GetData(), Magic, 8)) return Fail(TEXT("Magic/header FSIM tidak valid."));
    FWireReader R{Bytes, 8};
    const uint32 Version = R.U32(), Header = R.U32(), MetaSize = R.U32(), Count = R.U32();
    const uint32 FrameSize = R.U32(), Step = R.U32(), RealRate = R.U32(), GameRate = R.U32();
    if (Version != 1 || Header != 40 || FrameSize != 412 || Step != 3 || RealRate != 60 || GameRate != 10)
        return Fail(TEXT("Versi/layout/clock FSIM tidak didukung."));
    if (MetaSize > 1024 * 1024 || Count < 1 || Count > 301 || uint64(Bytes.Num()) != 40ULL + MetaSize + uint64(Count) * 412)
        return Fail(TEXT("Ukuran FSIM terpotong atau mengandung byte tambahan."));
    const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + 40), static_cast<int32>(MetaSize));
    const FString Json(Text.Length(), Text.Get());
    TSharedPtr<FJsonObject> Meta;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Meta) || !Meta.IsValid()) return Fail(TEXT("Metadata JSON tidak valid."));
    FString Format, TimeBase;
    if (!Meta->TryGetStringField(TEXT("format"), Format) || Format != TEXT("fsim.replay.v1") ||
        !Meta->TryGetStringField(TEXT("time_base"), TimeBase) || TimeBase != TEXT("presentation") || !NumberEquals(Meta, TEXT("time_scale"), 6))
        return Fail(TEXT("Kontrak waktu/format metadata tidak valid."));
    FFSimReplay Parsed;
    const TArray<TSharedPtr<FJsonValue>>* Teams = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Identities = nullptr;
    if (!Meta->TryGetArrayField(TEXT("teams"), Teams) || Teams->Num() != 2 || !Meta->TryGetArrayField(TEXT("players"), Identities) || Identities->Num() != 22)
        return Fail(TEXT("Diperlukan 2 tim dan 22 identitas pemain."));
    for (int32 T = 0; T < 2; ++T)
    {
        const auto Team = (*Teams)[T]->AsObject();
        FString Name;
        if (!Team.IsValid() || !Team->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty() || Name.Len() > 256) return Fail(TEXT("Nama tim tidak valid."));
        (T == 0 ? Parsed.HomeName : Parsed.AwayName) = Name;
    }
    Meta->TryGetStringField(TEXT("seed"), Parsed.Seed);
    TSet<FString> Unique;
    for (int32 I = 0; I < 22; ++I)
    {
        const auto Player = (*Identities)[I]->AsObject();
        FString Id;
        if (!Player.IsValid() || !Player->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty() || Id.Len() > 256 || Unique.Contains(Id) ||
            !NumberEquals(Player, TEXT("team"), I / 11) || !NumberEquals(Player, TEXT("lineup_index"), I % 11)) return Fail(TEXT("Urutan/identitas pemain tidak valid."));
        Unique.Add(Id); Parsed.PlayerIds.Add(Id);
    }
    R.Offset = 40 + static_cast<int32>(MetaSize);
    Parsed.Frames.Reserve(static_cast<int32>(Count));
    for (uint32 I = 0; I < Count; ++I)
    {
        FFSimReplayFrame F;
        F.Tick = R.U64(); F.HomeScore = R.U32(); F.AwayScore = R.U32(); F.Phase = R.U32(); F.Flags = R.U32();
        F.GoalSequence = R.U64(); const uint32 OwnerBits = R.U32();
        F.BallOwner = OwnerBits == MAX_uint32 ? -1 : static_cast<int32>(OwnerBits);
        F.Ball = R.Vector(); F.BallVelocity = R.Vector();
        if (F.Tick > 54000 || F.Tick % 3 || F.Phase != (F.Tick == 54000 ? 2U : F.Tick >= 27000 ? 1U : 0U) || (F.Flags & ~7U) ||
            (OwnerBits != MAX_uint32 && OwnerBits >= 22) || F.GoalSequence != uint64(F.HomeScore) + F.AwayScore ||
            !Finite(F.Ball) || !Finite(F.BallVelocity)) return Fail(TEXT("State frame/bola tidak valid."));
        if (I)
        {
            const auto& Prev = Parsed.Frames.Last();
            if (F.Tick != Prev.Tick + 3 || F.HomeScore < Prev.HomeScore || F.AwayScore < Prev.AwayScore ||
                (F.GoalSequence > Prev.GoalSequence && !(F.Flags & 1))) return Fail(TEXT("Timeline/skor/penanda gol tidak konsisten."));
        }
        for (int32 P = 0; P < 22; ++P)
        {
            F.Players[P] = R.Vector(); F.Actions[P] = R.U32();
            if (!Finite(F.Players[P]) || F.Actions[P] > 4) return Fail(TEXT("Koordinat/aksi pemain tidak valid."));
        }
        Parsed.Frames.Add(F);
    }
    if (!NumberEquals(Meta, TEXT("first_tick"), double(Parsed.Frames[0].Tick)) || !NumberEquals(Meta, TEXT("last_tick"), double(Parsed.Frames.Last().Tick)))
        return Fail(TEXT("Timeline metadata berbeda dari frame."));
    Out = MoveTemp(Parsed); // Transactional: a rejected file never replaces a good replay.
    return true;
}

void FFSimReplayPlayback::SetReplay(FFSimReplay InReplay)
{
    Replay = MoveTemp(InReplay); Seconds = 0; bPlaying = false;
}
void FFSimReplayPlayback::Advance(double WallSeconds)
{
    if (!bPlaying || !FMath::IsFinite(WallSeconds) || WallSeconds < 0) return;
    Seek(Seconds + WallSeconds * PlaybackRate);
}
void FFSimReplayPlayback::Seek(double PresentationSeconds)
{
    if (!FMath::IsFinite(PresentationSeconds)) return;
    Seconds = FMath::Clamp(PresentationSeconds, 0.0, Replay.Duration());
    if (Seconds >= Replay.Duration()) bPlaying = false;
}
void FFSimReplayPlayback::SetPlaying(bool Value)
{
    bPlaying = Value && Replay.Frames.Num() > 1 && Seconds < Replay.Duration();
}
void FFSimReplayPlayback::SetRate(double Value)
{
    if (FMath::IsFinite(Value) && (Value == 0.5 || Value == 1.0)) PlaybackRate = Value;
}
bool FFSimReplayPlayback::Sample(FFSimReplayFrame& Out) const
{
    if (Replay.Frames.IsEmpty()) return false;
    const double Offset = Seconds * 20.0;
    const int32 Index = FMath::Clamp(FMath::FloorToInt(Offset + 1e-9), 0, Replay.Frames.Num() - 1);
    Out = Replay.Frames[Index];
    if (Index + 1 < Replay.Frames.Num())
    {
        const auto& Next = Replay.Frames[Index + 1];
        // Hold the old positions until a teleport/restart frame is actually reached.
        if (!(Next.Flags & 6))
        {
            const double Alpha = FMath::Clamp(Offset - Index, 0.0, 1.0);
            Out.Ball = FMath::Lerp(Out.Ball, Next.Ball, Alpha);
            Out.BallVelocity = FMath::Lerp(Out.BallVelocity, Next.BallVelocity, Alpha);
            for (int32 P = 0; P < 22; ++P) Out.Players[P] = FMath::Lerp(Out.Players[P], Next.Players[P], Alpha);
        }
    }
    return true;
}
