#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ReplayViewerGameMode.generated.h"

UCLASS()
class AReplayViewerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
private:
    void ToggleReplay();
    void RestartReplay();
};

UCLASS()
class AReplayViewerGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AReplayViewerGameMode();
    virtual void BeginPlay() override;
};
