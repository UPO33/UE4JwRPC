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
struct JWRPC_API FJwRpcIncomingRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UJwRpcConnection> Connection;


	void FinishError(const FJwRPCError& error) const;
	void FinishError(int code, const FString& message = "") const;
	void FinishSuccess(TSharedPtr<FJsonValue> result) const;
	void FinishSuccess(const FString& result) const;
};

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FRequestDD, UJwRpcConnection*, connection, UJsonValue*, params,  const FJwRpcIncomingRequest&, requestHandle);

DECLARE_DYNAMIC_DELEGATE(FOnConnectSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnConnectFailed, const FString&, result);

/*
JSON-RPC2 over websocket client.

an instance of this class represents a connection to server. most functions can be used in both Blueprint and C++.


you usually inherit from this class and add your own functions.
then create an instance and connect to server by 'UJwRpcConnection::CreateAndConnect'

*/
UCLASS(BlueprintType, Blueprintable)
class JWRPC_API UJwRpcConnection : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:

	//delegate for successful result of requests
	DECLARE_DELEGATE_OneParam(FSuccessCB, TSharedPtr<FJsonValue> /*result*/);
	//delegate for failure of requests
	DECLARE_DELEGATE_OneParam(FErrorCB, const FJwRPCError& /*error*/);
	
	//
	DECLARE_DELEGATE(FEmptyCB);

	//delegate for incoming notifications
	DECLARE_DELEGATE_OneParam(FNotifyCB, TSharedPtr<FJsonValue> /*params*/);
	//delegate for incoming requests
	DECLARE_DELEGATE_TwoParams(FRequestCB, TSharedPtr<FJsonValue> /*params*/, FJwRpcIncomingRequest& /*requestHandle*/);


	UJwRpcConnection();

	/*
	send a request to the server.
	@param method		- name of method
	@param params		- the string containing any json value. object, array, string, number, ...
	@param onSuccess	- callback to be called when result arrives
	@param onError		- callback to be called if any type of error happened. 
	*/
	void Request(const FString& method, const FString& params, FSuccessCB onSuccess = nullptr, FErrorCB onError = nullptr);
	/*
	send a request to the server.
	@param method		- name of method
	@param params		- the shared pointer contacting any json value. object, array, string, number, ...
	@param onSuccess	- callback to be called when result arrives
	@param onError		- callback to be called if any type of error happened.
	*/
	void Request(const FString& method, TSharedPtr<FJsonValue> params, FSuccessCB onSuccess = nullptr, FErrorCB onError = nullptr);
	/*
	template version that converts the result to a struct
	*/
	template<class TResultStruct, class TSuccess>  void Request_RS(const FString& method, const FString& params, TSuccess onSuccess, FErrorCB onError)
	{
		this->Request(method, params, FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> jsResult) {

			TResultStruct resultStruct;
			if (FJsonObjectConverter::JsonObjectToUStruct<TResultStruct>(jsResult->AsObject().ToSharedRef(), &resultStruct))
			{
				onSuccess.ExecuteIfBound(resultStruct);
			}
		}), onError);
	}
	template<class TResultStruct, class TSuccess>  void Request_RS(const FString& method, TSharedPtr<FJsonValue> params, TSuccess onSuccess, FErrorCB onError)
	{
		this->Request(method, params, FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> jsResult) {

			TResultStruct resultStruct;
			if (FJsonObjectConverter::JsonObjectToUStruct<TResultStruct>(jsResult->AsObject().ToSharedRef(), &resultStruct))
			{
				onSuccess.ExecuteIfBound(resultStruct);
			}
		}), onError);
	}
	/*
	for those who have no result
	*/
	void Request_RE(const FString& method, const FString& params, FEmptyCB onSuccess, FErrorCB onError)
	{
		this->Request(method, params, FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> jsResult) {
			onSuccess.ExecuteIfBound();
		}), onError);
	}
	void Request_RE(const FString& method, TSharedPtr<FJsonValue> params, FEmptyCB onSuccess, FErrorCB onError)
	{
		this->Request(method, params, FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> jsResult) {
			onSuccess.ExecuteIfBound();
		}), onError);
	}
	/*
	send a notification to server.
	*/
	void Notify(const FString& method, const FString& params);
	/*
	send a notification to server.
	*/
	void Notify(const FString& method, TSharedPtr<FJsonValue> params);
	/*
	register a notification callback.
	*/
	void RegisterNotificationCallback(const FString& method, FNotifyCB callback);
	/*
	register a request callback.
	*/
	void RegisterRequestCallback(const FString& method, FRequestCB callback);

	/*
	send a notification to server.
	@param	method	- name of the method
	@param	params	- the string containing any valid json value. object, array, string, number, ... 
	*/
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Notify (string)"))
	void K2_Notify(const FString& method, const FString& params);
	/*
	send a request to the server.
	@param method		- name of the requesting method
	@param params		- the string containing any json value. object, array, string, number, ...
	@param onSuccess	- callback to be called when result arrives
	@param onError		- callback to be called if any type of error happened.
	*/
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Request (string)"))
	void K2_Request(const FString& method, const FString& params, FOnRPCResult onSuccess, FOnRPCError onError);
	/*
	send a notification to server.
	@param	method	- name of the method
	@param	params	- the object containing any valid json value. object, array, string, number, ...
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Notify (json)"))
	void K2_NotifyJSON(const FString& method, const UJsonValue* params);
	/*
	send a request to the server.
	@param method		- name of the requesting method
	@param params		- the object containing any json value. object, array, string, number, ...
	@param onSuccess	- callback to be called when result arrives
	@param onError		- callback to be called if any type of error happened.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Request (json)"))
	void K2_RequestJSON(const FString& method, const UJsonValue* params, FOnRPCResultJSON onSuccess, FOnRPCError onError);
	/*
	register a notification callback.
	@param  method		name of the method
	@param  callback	callback to be called when we receive such a notification
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RegisterNotificationCallback"))
	void K2_RegisterNotificationCallback(const FString& method, FNotificationDD callback);
	/*
	register a request callback. the invoked callback should call FinishSucces() or FinishError() to send the result
	@param  method		name of the method
	@param  callback	callback to be called when we receive such a request
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RegisterRequestCallback"))
	void K2_RegisterRequestCallback(const FString& method, FRequestDD callback);


	/*
	finish a pending request by sending error to the peer.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "FinishError"))
	static void K2_IncomingRequestFinishError(const FJwRpcIncomingRequest& request, const FJwRPCError& error);
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "FinishSuccess"))
	/*
	finish a pending request by sending result.
	@param result		the result to be sent to server. string containing json value. ( object, array, number, ... )
	*/
	static void K2_IncomingRequestFinishSuccess(const FJwRpcIncomingRequest& request, const FString& result);
	/*
	finish a pending request by sending result.
	@param result	the result object to be sent to the server
	
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "FinishSuccess"))
	static void K2_IncomingRequestFinishSuccessJSON(const FJwRpcIncomingRequest& request, const UJsonValue* result);


	/*
	ends all pending requests and closes the connection.
	*/
	UFUNCTION(BlueprintCallable)
	void Close(int Code = 1000, FString Reason = "");







	UFUNCTION(BlueprintPure)
	bool IsConnected() const;
	UFUNCTION(BlueprintPure)
	bool IsConnecting() const;

	UFUNCTION(BlueprintCallable)
	void Send(const FString& data);



	//static UJwRpcConnection* Connect(const FString& url, TFunction<void()> onSucess, TFunction<void(const FString&)> onError);

	UFUNCTION(BlueprintCallable)
	void TryReconnect();


	UFUNCTION(BlueprintCallable,meta=(DeterminesOutputType="connectionClass"))
	static UJwRpcConnection* CreateAndConnect(const FString& URL, TSubclassOf<UJwRpcConnection> connectionClass);

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
	virtual void OnConnectionError(const FString& error, bool bReconnect);
	/*
	this is called when a currently active connection is disconnected. 
	*/
	virtual void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);


	DECLARE_DELEGATE_OneParam(FOnConnected, bool /*bReconnect*/)
	DECLARE_DELEGATE_TwoParams(FOnConnectionError, const FString& /*error*/, bool /*bReconnect*/);
	DECLARE_DELEGATE_ThreeParams(FOnClosed, int32 /*StatusCode*/, const FString& /*Reason*/, bool /*bWasClean*/);

	FOnClosed OnClosedEvent;
	FOnConnected OnConnectedEvent;
	FOnConnectionError OnConnectionErrorEvent;

	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnConnected"))
	void K2_OnConnected(bool bReconnect);
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "OnConnectionError"))
	void K2_OnConnectionError(const FString& error, bool bReconnect);
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
	void InternalOnConnectionError(const FString& error);

	//kill all pending requests or any kind of callback who is waiting to be called
	void KillAll(const FJwRPCError& error);
	void CheckExpiredRequests();

	void Tick(float DeltaTime);
	bool IsTickable() const override;
	TStatId GetStatId() const override;

	void OnRequestRecv(TSharedPtr<FJsonObject> root);

	
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
		FErrorCB OnError;
		FSuccessCB OnResult;
		float ExpireTime;
	};

	struct FMethodData
	{
		FNotifyCB NotifyCB;
		FRequestCB RequestCB;
		FNotificationDD BPNotifyCB;
		FRequestDD	BPRequestCB;
		bool bIsNotification; //whether its notification of request 
	};

	/*
	all the registered methods that other side can send us
	*/
	TMap<FString, FMethodData> RegisteredCallbacks;

	//requests waiting for respond
	TMap<FString, FRequest> Requests;
};


