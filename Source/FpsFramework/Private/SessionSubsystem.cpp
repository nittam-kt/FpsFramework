// Fill out your copyright notice in the Description page of Project Settings.


#include "SessionSubsystem.h"
#include "OnlineSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// UE側で一般的に使われる固定名（"GameSession"）
static const FName SESSION_NAME = NAME_GameSession;

bool USessionSubsystem::EnsureOnline()
{
    // Online Subsystem を取得（Null: LAN / Steam: Steam / EOS: Epic などプラットフォーム別に切替）
    if (!OSS) OSS = IOnlineSubsystem::Get();
    if (!OSS) { UE_LOG(LogTemp, Error, TEXT("No OnlineSubsystem")); return false; }

    if (!Session.IsValid()) Session = OSS->GetSessionInterface();
    if (!Session.IsValid()) { UE_LOG(LogTemp, Error, TEXT("No SessionInterface")); return false; }

    return true;
}

void USessionSubsystem::ClearDelegates()
{
    if (!Session.IsValid()) return;
    UE_LOG(LogTemp, Warning, TEXT("[ClearDelegates]"));

    if (OnCreateHandle.IsValid())  Session->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateHandle);
    if (OnStartHandle.IsValid())   Session->ClearOnStartSessionCompleteDelegate_Handle(OnStartHandle);
    if (OnDestroyHandle.IsValid()) Session->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroyHandle);
    if (OnFindHandle.IsValid())    Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindHandle);
    if (OnJoinHandle.IsValid())    Session->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinHandle);

    OnCreateHandle.Reset();
    OnStartHandle.Reset();
    OnDestroyHandle.Reset();
    OnFindHandle.Reset();
    OnJoinHandle.Reset();
}

void USessionSubsystem::ClearFindDelegate()
{
    if(!Session.IsValid()) return;
    if(OnFindHandle.IsValid())    Session->ClearOnFindSessionsCompleteDelegate_Handle(OnFindHandle);
    OnFindHandle.Reset();
}


bool USessionSubsystem::CreateSession(FString RoomName, int32 PublicConnections)
{
    if (!EnsureOnline()) return false;

    if (Session->GetNamedSession(SESSION_NAME))
    {
        // 既に同名セッションが残っていたら再起動させる
        return false;
    }

    // === セッション設定 ===
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = false;      // LANではなくインターネットに
    Settings.bShouldAdvertise = true;  // Find に出す
    Settings.bAllowJoinInProgress = true;  // 途中参加OK
    Settings.bUsesPresence = false;
    Settings.bUseLobbiesIfAvailable = false;
    Settings.NumPublicConnections = FMath::Max(1, PublicConnections); // 参加枠（ホスト除く枠数でOK）
    Settings.bAllowJoinViaPresence = false;
//    Settings.bAllowJoinViaPresence = true;
    Settings.bAllowJoinViaPresenceFriendsOnly = false;
//    Settings.BuildUniqueId = 202510;

    // 部屋の名前を設定
    Settings.Set(FName("SERVER_NAME_KEY"), RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    Settings.Set(FName("KEYWORD"), FString("GP3FPS_CLASS"), EOnlineDataAdvertisementType::ViaOnlineService);

    // コールバック登録
    ClearDelegates();
    OnCreateHandle = Session->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnCreateComplete));
    OnStartHandle = Session->AddOnStartSessionCompleteDelegate_Handle(
        FOnStartSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnStartComplete));

    // 実行（UserIndex=0でOK。複数LocalPlayerがある場合は切替）
    const bool bIssued = Session->CreateSession(/*LocalUserNum=*/0, SESSION_NAME, Settings);
    if (!bIssued)
    {
        UKismetSystemLibrary::PrintString(this, "CreateSession: immediate failure",
            true, true, FColor::Red, 4.f, TEXT("None"));
        OnCreateFinished.Broadcast(false);
    }
    return true;
}

void USessionSubsystem::OnCreateComplete(FName, bool bOk)
{
    FString ConnectString;
    if(Session->GetResolvedConnectString(NAME_GameSession, ConnectString))
    {
        UE_LOG(LogTemp, Warning, TEXT("ResolvedConnectString: %s"), *ConnectString);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GetResolvedConnectString failed"));
    }

    OnCreateFinished.Broadcast(bOk);
    if (!bOk) { ClearDelegates(); return; }

    const FString CurrentMap = GetWorld()->GetOutermost()->GetName(); // "/Game/Maps/Lobby" など
    UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentMap), true, TEXT("?listen?HostSession=1"));
    UKismetSystemLibrary::PrintString(this, CurrentMap,
        true, true, FColor::Red, 4.f, TEXT("None"));
}

void USessionSubsystem::StartSession()
{
    const FString CurrentMap = GetWorld()->GetOutermost()->GetName(); // "/Game/Maps/Lobby" など
    if(IOnlineSubsystem* os = IOnlineSubsystem::Get())
    {
        if(IOnlineSessionPtr sess = os->GetSessionInterface())
        {
            FNamedOnlineSession* nsess = sess->GetNamedSession(NAME_GameSession);
            if(!nsess)
            {
                UE_LOG(LogTemp, Warning, TEXT("StartSession: session not found"));
                return;
            }

            // セッション開始（内部状態を「スタート」に）
            sess->StartSession(NAME_GameSession);
            UKismetSystemLibrary::PrintString(this, "OnCreateComplete: Success!!",
                true, true, FColor::Cyan, 4.f, TEXT("None"));

            const FOnlineSessionSettings& Settings = nsess->SessionSettings;

            UE_LOG(LogTemp, Log,
                TEXT("SessionState=%s  bShouldAdvertise=%d  NumPublicConnections=%d  NumOpenPublicConnections=%d"),
                EOnlineSessionState::ToString(nsess->SessionState),
                Settings.bShouldAdvertise ? 1 : 0,
                Settings.NumPublicConnections,
                nsess->NumOpenPublicConnections
            );
            FString ConnectString;
            if(Session->GetResolvedConnectString(NAME_GameSession, ConnectString))
            {
                UE_LOG(LogTemp, Warning, TEXT("ResolvedConnectString: %s"), *ConnectString);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("GetResolvedConnectString failed"));
            }
        }
    }
}

void USessionSubsystem::OnStartComplete(FName, bool bOk)
{
    UE_LOG(LogTemp, Log, TEXT("StartSession: %s"), bOk ? TEXT("OK") : TEXT("NG"));
}

void USessionSubsystem::FindSessions(int32 MaxResults)
{
    if (!EnsureOnline()) return;

    // 検索条件を作る
    LastSearch = MakeShared<FOnlineSessionSearch>();
    LastSearch->bIsLanQuery = false;
    LastSearch->MaxSearchResults = FMath::Clamp(MaxResults, 1, 2000);
    LastSearch->QuerySettings.Set(SEARCH_LOBBIES, false, EOnlineComparisonOp::Equals);
    LastSearch->QuerySettings.Set(FName("KEYWORD"), FString("GP3FPS_CLASS"), EOnlineComparisonOp::Equals);

    ClearDelegates();
    OnFindHandle = Session->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnFindComplete));

    UE_LOG(LogTemp, Warning, TEXT("[FindStart] bIsLanQuery=%d MaxResults=%d"),
        LastSearch->bIsLanQuery ? 1 : 0,
        LastSearch->MaxSearchResults);

    const bool bIssued = Session->FindSessions(/*LocalUserNum=*/0, LastSearch.ToSharedRef());
    if (!bIssued)
    {
        ClearFindDelegate();
        UE_LOG(LogTemp, Error, TEXT("FindSessions: immediate failure"));
        LastRows.Reset();
        OnFindFinished.Broadcast(false, LastRows);
    }
    UKismetSystemLibrary::PrintString(this, "FindSessions Start", true, true, FColor::Blue, 4.f, TEXT("None"));
}

void USessionSubsystem::OnFindComplete(bool bOk)
{
    ClearFindDelegate();
    LastRows.Reset();

    if (bOk && LastSearch.IsValid())
    {
        int32 Index = 0;
        for (const auto& R : LastSearch->SearchResults)
        {
            FFoundSessionRow Row;
            Row.DisplayName = R.Session.OwningUserName; // 表示名（UIの部屋名に使う）
            R.Session.SessionSettings.Get( FName(TEXT("SERVER_NAME_KEY")), Row.DisplayName);
            Row.PingMs = R.PingInMs;
            Row.OpenConnections = R.Session.NumOpenPublicConnections;
            Row.MaxConnections = R.Session.SessionSettings.NumPublicConnections;
            Row.SearchIndex = Index;
            LastRows.Add(Row);

            UKismetSystemLibrary::PrintString(this, "OnFindComplete: Success!! :" + Row.DisplayName,
                true, true, FColor::Yellow, 4.f, TEXT("None"));
            FString room = TEXT("Unknown");
            R.Session.SessionSettings.Get(FName("SERVER_NAME_KEY"), room);

            Index++;
        }
    }
    else
    {
        UKismetSystemLibrary::PrintString(this, "OnFindComplete: Failed!!", true, true, FColor::Red, 4.f, TEXT("None"));
    }

    OnFindFinished.Broadcast(bOk, LastRows);
}

bool USessionSubsystem::JoinBySearchIndex(int32 SearchIndex, FString PlayerName)
{
    if (!EnsureOnline() || !LastSearch.IsValid() || !LastSearch->SearchResults.IsValidIndex(SearchIndex))
    {
        OnJoinFinished.Broadcast(false);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[JoinBySearchIndex] EnsureOnline()=%d SearchIndex=%d SearchResults.IsValidIndex()=%d"),
        EnsureOnline() ? 1 : 0,
        SearchIndex,
        LastSearch->SearchResults.IsValidIndex(SearchIndex) ? 1 : 0);

    ClearDelegates();
    PendingPlayerName = PlayerName;

    OnJoinHandle = Session->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &USessionSubsystem::OnJoinComplete));

    UE_LOG(LogTemp, Warning, TEXT("[Join] delegate bound"));
    const bool bStarted = Session->JoinSession(/*LocalUserNum=*/0, SESSION_NAME, LastSearch->SearchResults[SearchIndex]);
    if (!bStarted)
    {
        UE_LOG(LogTemp, Error, TEXT("JoinSession: immediate failure"));
        OnJoinFinished.Broadcast(false);
        return false;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Join] started=%d"), bStarted ? 1 : 0);
        return true;
    }
}

void USessionSubsystem::OnJoinComplete(FName, EOnJoinSessionCompleteResult::Type Result)
{
    UE_LOG(LogTemp, Warning, TEXT("[JoinComplete] result=%d"), (int32)Result);
    FString ConnectString;
    if(Session->GetResolvedConnectString(NAME_GameSession, ConnectString))
    {
        UE_LOG(LogTemp, Warning, TEXT("ResolvedConnectString: %s"), *ConnectString);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GetResolvedConnectString failed"));
    }

    const bool bOk = (Result == EOnJoinSessionCompleteResult::Success);

    if (!bOk) { ClearDelegates(); return; }

    // 参加先の接続URLを OnlineSubsystem から解決し、クライアント遷移する
    FString addr;
    if (Session->GetResolvedConnectString(SESSION_NAME, addr, NAME_GamePort))
    {
        const FString Enc = FGenericPlatformHttp::UrlEncode(PendingPlayerName);
        addr += FString::Printf(TEXT("?NickName=%s"), *Enc);

        if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
        {
            OnJoinFinished.Broadcast(bOk);

            UKismetSystemLibrary::PrintString(this, "Join & ClientTravel - " + addr,
                true, true, FColor::Orange, 15.f, TEXT("None"));

            PC->ClientTravel(addr, ETravelType::TRAVEL_Absolute);
        }
    }
    ClearDelegates();
}
