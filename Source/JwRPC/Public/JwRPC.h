// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "JsonValue.h"
#include "IWebSocket.h"
#include "Tickable.h"

#include "JwRPC.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogJwRPC, Log, All);

class UJwRpcConnection;
class UJsonValue;

class JWRPC_API FJwRPCModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

USTRUCT(BlueprintType)
struct JWRPC_API FJwRPCError
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	int Code;
	UPROPERTY(BlueprintReadWrite)
	FString Message;

	static FJwRPCError ParseError;
	static FJwRPCError InvalidRequest;
	static FJwRPCError MethodNotFound;
	static FJwRPCError InvalidParams;
	static FJwRPCError InternalError;
	static FJwRPCError ServerError;
	static FJwRPCError Timeout;
	static FJwRPCError NoConnection;
};




DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRPCResult, const FString&, result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRPCResultJSON, const UJsonValue*, result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnRPCError, const FJwRPCError&, error);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FNotificationDD, UJwRpcConnection*, connection, const UJsonValue*, params);

USTRUCT(BlueprintType)
struct FJwRpcIncomingRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UJwRpcConnection> Connection;
	UPROPERTY(BlueprintReadOnly)
	UJsonValue* Params;

	void FinishError(const FJwRPCError& error) const;
	void FinishSuccess(TSharedPtr<FJsonValue> result) const;
	void FinishSuccess(const FString& result) const;
};

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FRequestDD, UJwRpcConnection*, connection, UJsonValue*, params,  const FJwRpcIncomingRequest&, requestHandle);

DECLARE_DYNAMIC_DELEGATE(FOnConnectSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnConnectFailed, const FString&, result);

UCLASS(BlueprintType, Blueprintable)
class JWRPC_API UJwRpcConnection : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:

	using EmptyCB = TFunction<void()>;
	using ErrorCB = TFunction<void(const FJwRPCError&)>;
	using SuccessCB = TFunction<void(TSharedPtr<FJsonValue>)>;

	typedef TFunction<void(TSharedPtr<FJsonValue>)> CallbackJSON;
	typedef FString FMethodName;

	DECLARE_DELEGATE_OneParam(FNotifyCallbackBase, TSharedPtr<FJsonValue> /*params*/);
	DECLARE_DELEGATE_TwoParams(FRequestCallbackBase, TSharedPtr<FJsonValue> /*params*/, FJwRpcIncomingRequest&);


	UJwRpcConnection();

	void Request(const FString& method, const FString& params, SuccessCB onSuccess, ErrorCB onError);
	void Request(const FString& method, TSharedPtr<FJsonValue> params, SuccessCB onSuccess, ErrorCB onError);
	void Notify(const FString& method, const FString& params);
	void Notify(const FString& method, TSharedPtr<FJsonValue> params);

	UFUNCTION(BlueprintCallable, meta=(DisplayName="Notify"))
	void K2_Notify(const FString& method, const FString& params);
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Request"))
	void K2_Request(const FString& method, const FString& params, FOnRPCResult onSuccess, FOnRPCError onError);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Notify"))
	void K2_NotifyJSON(const FString& method, const UJsonValue* params);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Request"))
	void K2_RequestJSON(const FString& method, const UJsonValue* params, FOnRPCResultJSON onSuccess, FOnRPCError onError);


	UFUNCTION(BlueprintCallable)
	void Close(int Code = 1000, FString Reason = "");

	static void Test0();

	struct FMethodData
	{
		FNotifyCallbackBase NotifyCB;
		FRequestCallbackBase RequestCB;
		FNotificationDD BPNotifyCB;
		FRequestDD	BPRequestCB;
		bool bIsNotification;
	};

	TMap<FString, FMethodData> RegisteredCallbacks;

	void RegisterNotificationCallback(FString method, FNotifyCallbackBase callback);
	void RegisterRequestCallback(FString method, FRequestCallbackBase callback);

	UFUNCTION(BlueprintCallable)
	void RegisterNotificationCallback(FString method, FNotificationDD callback);
	UFUNCTION(BlueprintCallable)
	void RegisterRequestCallback(FString method, FRequestDD callback);

	UFUNCTION(BlueprintPure)
	bool IsConnected() const;
	UFUNCTION(BlueprintPure)
	bool IsConnecting() const;

	UFUNCTION(BlueprintCallable)
	void Send(const FString& data);

	UFUNCTION(BlueprintCallable)
	static void IncomingRequestFinishError(const FJwRpcIncomingRequest& request, const FJwRPCError& error);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "FinishSuccess"))
	static void IncomingRequestFinishSuccess(const FJwRpcIncomingRequest& request, const FString& result);
	UFUNCTION(BlueprintCallable, meta=(DisplayName="FinishSuccess"))
	static void IncomingRequestFinishSuccessJSON(const FJwRpcIncomingRequest& request, const UJsonValue* result);

	//static UJwRpcConnection* Connect(const FString& url, TFunction<void()> onSucess, TFunction<void(const FString&)> onError);

	UFUNCTION(BlueprintCallable)
	void TryReconnect();


	UFUNCTION(BlueprintCallable,meta=(DeterminesOutputType="connectionClass"))
	static UJwRpcConnection* CreateAndConnect(const FString& url, TSubclassOf<UJwRpcConnection> connectionClass);

	//template version for c++
	template <class TConnectionClass > static TConnectionClass* CreateAndConnect(const FString& url)
	{
		return (TConnectionClass*)CreateAndConnect(url, TConnectionClass::StaticClass());
	}

	/*
	this is called when we connect for the first time or reconnection happens.
	this function may get called multiple times
	*/
	virtual void OnConnected(bool bReconnect);
	/*
	this is called when connecting failed. may get called multiple times.
	*/
	virtual void OnConnectionError(const FString& error);
	/*
	this is called when a currently active connection is disconnected. 
	*/
	virtual void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnConnected"))
	void K2_OnConnected(bool bReconnect);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "OnConnectionError"))
	void K2_OnConnectionError(const FString& error);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "OnClosed"))
	void K2_OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);

	//static UJwRpcConnection* K2_Connect(const FString& url, FOnConnectSuccess onSucess, FOnConnectFailed onError);

	void BeginDestroy() override;

protected:
	//generates and returns a new id. 
	FString GenId();
	//this is called when we receive data from the server
	void OnMessage(const FString& data);
	void InternalOnConnect();
	//kill all pending requests or any kind of callback who is waiting to be called
	void KillAll(const FJwRPCError& error);
	void CheckExpiredRequests();

	void Tick(float DeltaTime);
	bool IsTickable() const override;
	TStatId GetStatId() const override;

	void OnRequestRecv(TSharedPtr<FJsonObject>& root);

	
	int IdCounter = 0;
	TSharedPtr<IWebSocket> Connection;
	//default timeout in seconds
	float DefaultTimeout = 60;
	//
	float TimeSinceStart = 0;
	float LastExpireCheckTime = 0;
	float LastDisconnectTime = 0;
	float LastConnectAttempTime = 0;
	float LastConnectionErrorTime = 0;

	int ReconnectAttempt = 0;
	bool bFirstConnect = true;
	bool bConnecting = true;

	bool bAutoReconnectEnabled = true;
	float ReconnectDelay = 2;
	int ReconnectMaxAttempt = 20;
	FString SavedURL;

	struct FRequest
	{
		FString Method;
		ErrorCB OnError;
		SuccessCB OnResult;
		float ExpireTime;
	};
	//requests waiting for respond
	TMap<FString, FRequest> Requests;
};


