// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSLogin.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SessionSubsystem.h"
#include "Engine/GameInstance.h"


// Sets default values
AEOSLogin::AEOSLogin()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;

}

void AEOSLogin::Login()
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS || OSS->GetSubsystemName() != FName(TEXT("EOS"))) return;

    LoginIdentity = OSS->GetIdentityInterface();
    if (!LoginIdentity.IsValid() || LoginHandle.IsValid()) return;

    // ログイン済みなら再ログインしない。状態は ShowStatus() で表示する。
    if (LoginIdentity->GetLoginStatus(0) == ELoginStatus::LoggedIn) return;

    LoginHandle = LoginIdentity->AddOnLoginCompleteDelegate_Handle(0,
        FOnLoginCompleteDelegate::CreateUObject(this, &AEOSLogin::OnLoginComplete));

    FOnlineAccountCredentials Creds;
    Creds.Type = TEXT("accountportal");
    UKismetSystemLibrary::PrintString(this, TEXT("EOS: logging in..."));
    if (!LoginIdentity->Login(0, Creds))
    {
        ClearLoginDelegate();
        UKismetSystemLibrary::PrintString(this, TEXT("EOS: login request failed"));
    }
}

void AEOSLogin::OnLoginComplete(int32 LocalUserNum, bool bSuccess,
    const FUniqueNetId& UserId, const FString& Error)
{
    ClearLoginDelegate();
    UKismetSystemLibrary::PrintString(this,
        bSuccess ? TEXT("EOS: login successful") : FString(TEXT("EOS login failed: ")) + Error);
    ShowStatus();
}

void AEOSLogin::ClearLoginDelegate()
{
    if (LoginIdentity.IsValid() && LoginHandle.IsValid())
        LoginIdentity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
    LoginHandle.Reset();
}

void AEOSLogin::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // エディタの停止時にも通知を解除する。
    ClearLoginDelegate();
    Super::EndPlay(EndPlayReason);
}

void AEOSLogin::ShowStatus()
{
    auto* Sub = GetGameInstance()->GetSubsystem<USessionSubsystem>();
    const FName Key(*FString::Printf(TEXT("EOSStatus_%u"), GetWorld()->GetUniqueID()));
    UKismetSystemLibrary::PrintString(this, Sub->GetStatusText(),
        true, false, FLinearColor::Yellow, 0.6f, Key);
}

// Called when the game starts or when spawned
void AEOSLogin::BeginPlay()
{
	Super::BeginPlay();
	
    ShowStatus();
	Login();
}

// Called every frame
void AEOSLogin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    ShowStatus();

}

