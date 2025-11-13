// Fill out your copyright notice in the Description page of Project Settings.


#include "EOSLogin.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"


// Sets default values
AEOSLogin::AEOSLogin()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void AEOSLogin::Lonin()
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;

    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return;

    FOnlineAccountCredentials Creds;
    Creds.Type = TEXT("device"); // © "accountportal" ‚È‚Ç‚à‘I‚×‚é
    Creds.Id = TEXT("");
    Creds.Token = TEXT("");

    Identity->Login(0, Creds);
}


// Called when the game starts or when spawned
void AEOSLogin::BeginPlay()
{
	Super::BeginPlay();
	
	Lonin();
}

// Called every frame
void AEOSLogin::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

