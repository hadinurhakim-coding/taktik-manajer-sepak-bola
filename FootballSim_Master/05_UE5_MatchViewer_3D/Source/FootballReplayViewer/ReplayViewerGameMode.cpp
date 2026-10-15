#include "ReplayViewerGameMode.h"
#include "MatchReplayActor.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"

AReplayViewerGameMode::AReplayViewerGameMode()
{
    PlayerControllerClass = AReplayViewerController::StaticClass();
    DefaultPawnClass = nullptr;
}
void AReplayViewerGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<AMatchReplayActor>();
}
void AReplayViewerController::BeginPlay()
{
    Super::BeginPlay();
    if (FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"))) { bShowMouseCursor = false; return; }
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}
void AReplayViewerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AReplayViewerController::ToggleReplay);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AReplayViewerController::RestartReplay);
}
void AReplayViewerController::ToggleReplay()
{
    for (TActorIterator<AMatchReplayActor> It(GetWorld()); It; ++It) { It->TogglePlayback(); break; }
}
void AReplayViewerController::RestartReplay()
{
    for (TActorIterator<AMatchReplayActor> It(GetWorld()); It; ++It) { It->RestartPlayback(); break; }
}
