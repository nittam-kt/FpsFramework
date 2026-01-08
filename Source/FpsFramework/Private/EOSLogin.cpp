// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSLogin.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
AEOSLogin::AEOSLogin()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AEOSLogin::Login()
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;

    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return;

    TSharedPtr<const FUniqueNetId> Id = Identity->GetUniquePlayerId(0);
    if (Id.IsValid())
    {
        // すでにログインしていてユーザーIDが取得できている
        UE_LOG(LogTemp, Log, TEXT("ProductUserId: %s"), *Id->ToString());
        return;
    }

    FOnlineAccountCredentials Creds;
    Creds.Type = TEXT("accountportal"); // ← "accountportal" なども選べる
    Creds.Id = TEXT("");
    Creds.Token = TEXT("");

    Identity->OnLoginCompleteDelegates->AddLambda(
        [this](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
        {
            if (bWasSuccessful)
            {
                UKismetSystemLibrary::PrintString(this, "EOS Login(): Successful",
                    true, true, FColor::Blue, 4.f, TEXT("None"));
            }
            else
            {
                UKismetSystemLibrary::PrintString(this, Error,
                    true, true, FColor::Blue, 4.f, TEXT("None"));
            }
        });
    UKismetSystemLibrary::PrintString(this, "EOS Login() - Start ---",
        true, true, FColor::Green, 4.f, TEXT("None"));
    Identity->Login(0, Creds);
}


// Called when the game starts or when spawned
void AEOSLogin::BeginPlay()
{
	Super::BeginPlay();
	
	Login();
}

// Called every frame
void AEOSLogin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

