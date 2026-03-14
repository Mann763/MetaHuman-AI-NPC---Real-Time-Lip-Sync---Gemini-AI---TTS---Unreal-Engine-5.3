#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sound/SoundWaveProcedural.h"
#include "AudioHelperLibrary.generated.h"

USTRUCT(BlueprintType)
struct FVisemeKeyframe
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float Time = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float JawAlpha = 0.f;
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FTTSResponseDelegate,
    USoundWaveProcedural*, SoundWave,
    const TArray<uint8>&, PCMData
);

// Now passes both JawOpenAlpha and TeethShowAlpha
DECLARE_DYNAMIC_DELEGATE_TwoParams(
    FJawUpdateDelegate,
    float, JawOpenAlpha,
    float, TeethShowAlpha
);

UCLASS()
class GEMININPC_API UAudioHelperLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TTS")
    static void SendTTSRequest(
        const FString& Text,
        const FTTSResponseDelegate& OnAudioReady,
        const FJawUpdateDelegate& OnJawUpdate
    );

    UFUNCTION(BlueprintCallable, Category = "TTS")
    static USoundWaveProcedural* CreateSoundWaveFromPCM(
        const TArray<uint8>& PCMData,
        int32 SampleRate = 22050
    );

    UFUNCTION(BlueprintCallable, Category = "TTS")
    static void StartJawDrive(
        const TArray<FVisemeKeyframe>& Visemes,
        const FJawUpdateDelegate& OnJawUpdate
    );
};