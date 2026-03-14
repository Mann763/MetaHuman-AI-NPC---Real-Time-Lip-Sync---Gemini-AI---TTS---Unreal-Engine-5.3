#include "AudioHelperLibrary.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Misc/Base64.h"

void UAudioHelperLibrary::SendTTSRequest(
    const FString& Text,
    const FTTSResponseDelegate& OnAudioReady,
    const FJawUpdateDelegate& OnJawUpdate
)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL("http://127.0.0.1:5000/tts");
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetTimeout(15.f);

    // Safe JSON serialization
    TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
    JsonObject->SetStringField("text", Text);
    FString JsonBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
    FJsonSerializer::Serialize(JsonObject, Writer);
    Request->SetContentAsString(JsonBody);

    Request->OnProcessRequestComplete().BindLambda(
        [OnAudioReady, OnJawUpdate](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
        {
            if (!bSuccess || !Response.IsValid()) return;
            if (Response->GetResponseCode() != 200) return;

            FString ResponseStr = Response->GetContentAsString();

            // Parse JSON response
            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid()) return;

            // Decode base64 PCM
            FString PCMBase64 = RootObject->GetStringField("pcm");
            TArray<uint8> AudioBytes;
            FBase64::Decode(PCMBase64, AudioBytes);
            if (AudioBytes.Num() == 0) return;

            // Parse viseme keyframes
            TArray<FVisemeKeyframe> Visemes;
            const TArray<TSharedPtr<FJsonValue>>* VisemeArray;
            if (RootObject->TryGetArrayField("visemes", VisemeArray))
            {
                for (const TSharedPtr<FJsonValue>& Entry : *VisemeArray)
                {
                    const TSharedPtr<FJsonObject>* Obj;
                    if (Entry->TryGetObject(Obj))
                    {
                        FVisemeKeyframe KF;
                        KF.Time = (float)(*Obj)->GetNumberField("t");
                        KF.JawAlpha = (float)(*Obj)->GetNumberField("jaw");
                        Visemes.Add(KF);
                    }
                }
            }

            AsyncTask(ENamedThreads::GameThread, [OnAudioReady, OnJawUpdate, AudioBytes, Visemes]()
                {
                    USoundWaveProcedural* Sound =
                        UAudioHelperLibrary::CreateSoundWaveFromPCM(AudioBytes, 22050);

                    if (OnAudioReady.IsBound())
                        OnAudioReady.Execute(Sound, AudioBytes);

                    UAudioHelperLibrary::StartJawDrive(Visemes, OnJawUpdate);
                });
        });

    Request->ProcessRequest();
}

USoundWaveProcedural* UAudioHelperLibrary::CreateSoundWaveFromPCM(
    const TArray<uint8>& PCMData,
    int32 SampleRate
)
{
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(GetTransientPackage());
    SoundWave->SetSampleRate(SampleRate);
    SoundWave->NumChannels = 1;
    SoundWave->Duration = (float)PCMData.Num() / (2.f * SampleRate);
    SoundWave->SoundGroup = SOUNDGROUP_Default;
    SoundWave->bLooping = false;
    SoundWave->bCanProcessAsync = true;
    SoundWave->QueueAudio(PCMData.GetData(), PCMData.Num());
    return SoundWave;
}

void UAudioHelperLibrary::StartJawDrive(
    const TArray<FVisemeKeyframe>& Visemes,
    const FJawUpdateDelegate& OnJawUpdate
)
{
    if (Visemes.Num() == 0) return;

    TSharedPtr<float>  ElapsedTime = MakeShared<float>(0.f);
    TSharedPtr<float>  SmoothedJaw = MakeShared<float>(0.f);
    TSharedPtr<float>  SmoothedTeeth = MakeShared<float>(0.f);
    TSharedPtr<int32>  KeyframeIdx = MakeShared<int32>(0);
    float              TotalTime = Visemes.Last().Time + 0.3f;

    TSharedPtr<TArray<FVisemeKeyframe>> VisemesCopy =
        MakeShared<TArray<FVisemeKeyframe>>(Visemes);

    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [ElapsedTime, SmoothedJaw, SmoothedTeeth, KeyframeIdx, VisemesCopy, TotalTime, OnJawUpdate]
            (float DeltaTime) -> bool
            {
                *ElapsedTime += DeltaTime;

                if (*ElapsedTime >= TotalTime)
                {
                    // Smoothly close jaw and hide teeth
                    *SmoothedJaw = FMath::FInterpTo(*SmoothedJaw, 0.f, DeltaTime, 12.f);
                    *SmoothedTeeth = FMath::FInterpTo(*SmoothedTeeth, 0.f, DeltaTime, 12.f);

                    if (OnJawUpdate.IsBound())
                        OnJawUpdate.Execute(*SmoothedJaw, *SmoothedTeeth);

                    return (*SmoothedJaw > 0.001f || *SmoothedTeeth > 0.001f);
                }

                // Advance keyframe index to current time
                const TArray<FVisemeKeyframe>& KFs = *VisemesCopy;
                while (*KeyframeIdx < KFs.Num() - 2 &&
                    KFs[*KeyframeIdx + 1].Time <= *ElapsedTime)
                {
                    (*KeyframeIdx)++;
                }

                // Interpolate between current and next keyframe
                int32 CurIdx = *KeyframeIdx;
                int32 NextIdx = FMath::Min(CurIdx + 1, KFs.Num() - 1);

                float CurTime = KFs[CurIdx].Time;
                float NextTime = KFs[NextIdx].Time;
                float Alpha = (NextTime > CurTime)
                    ? FMath::Clamp((*ElapsedTime - CurTime) / (NextTime - CurTime), 0.f, 1.f)
                    : 1.f;

                float TargetJaw = FMath::Lerp(KFs[CurIdx].JawAlpha, KFs[NextIdx].JawAlpha, Alpha);

                // Asymmetric smoothing: snap open fast, ease closed slowly
                float JawSpeed = TargetJaw > *SmoothedJaw ? 20.f : 10.f;
                *SmoothedJaw = FMath::FInterpTo(*SmoothedJaw, TargetJaw, DeltaTime, JawSpeed);

                // Teeth only peek out slightly — natural speech, not laughing
                // Threshold raised to 0.25 so teeth stay hidden on small movements
                // Max capped at 0.35 so it's a subtle show, not a grin
                float TargetTeeth = FMath::Clamp(
                    FMath::GetRangePct(0.25f, 0.5f, *SmoothedJaw),
                    0.f, 0.35f
                );

                // Teeth lag slightly behind jaw — slower open and close
                float TeethSpeed = TargetTeeth > *SmoothedTeeth ? 8.f : 5.f;
                *SmoothedTeeth = FMath::FInterpTo(*SmoothedTeeth, TargetTeeth, DeltaTime, TeethSpeed);

                if (OnJawUpdate.IsBound())
                    OnJawUpdate.Execute(*SmoothedJaw, *SmoothedTeeth);

                return true;
            }),
        1.f / 60.f
    );
}