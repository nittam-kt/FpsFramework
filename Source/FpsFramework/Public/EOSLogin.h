// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "EOSLogin.generated.h"

UCLASS()
class FPSFRAMEWORK_API AEOSLogin : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEOSLogin();

	void Login();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void ClearLoginDelegate();
    void OnLoginComplete(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error);
    void ShowStatus();
    IOnlineIdentityPtr LoginIdentity;
    FDelegateHandle LoginHandle;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
