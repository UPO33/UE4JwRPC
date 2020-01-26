// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "JwRPC.h"
#include "IConsoleManager.h"
#include "CommandLine.h"
#include "CondensedJsonPrintPolicy.h"
#include "JsonBP.h"
#include "WebSocketsModule.h"

void FJwRPCModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	//LoadStates();
}

void FJwRPCModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FJwRPCModule, JwRPC)


DEFINE_LOG_CATEGORY(LogJwRPC)

UJwRpcConnection::UJwRpcConnection()
{
	//UE_SET_LOG_VERBOSITY(LogJwRPC, NoLogging);
}


void UJwRpcConnection::Request(const FString& method, const FString& params, FSuccessCB onSuccess, FErrorCB onError)
{
	FString id = GenId();

	FRequest req;
	req.Method = method;
	req.OnResult = onSuccess;
	req.OnError = onError;
	req.ExpireTime = TimeSinceStart + DefaultTimeout;

	//if (FTimerManager* pTimer = TryGetTimerManager())
	//{
	//	pTimer->SetTimer(req.TimerHandle, [this, id]() {
	//		
	//		UE_LOG(SourenaBK, Warning, TEXT("Request id:%s timed out"), *id);
	//
	//		FRequest* pReq = this->Requests.Find(id);
	//		if (pReq)
	//		{
	//			if (pReq->OnError)
	//				pReq->OnError(GetJSONTimeoutError());
	//
	//			pReq->OnError = nullptr;
	//			this->Requests.Remove(id);
	//		}
	//
	//
	//	}, DefaultTimeout, false);
	//}

	const FString finalData = FString::Printf(TEXT(R"({"id":"%s","method":"%s","params":%s})"), *id, *method, *params);
	if (Connection)
	{
		Connection->Send(finalData);
	}
	
	UE_LOG(LogJwRPC, Warning, TEXT("OutgingData:%s"), *finalData);

	Requests.Add(id, MoveTemp(req));
}

void UJwRpcConnection::Request(const FString& method, TSharedPtr<FJsonValue> params, FSuccessCB onSuccess, FErrorCB onError)
{
	return Request(method, params ? HelperStringifyJSON(params) : FString(), onSuccess, onError);
}


void UJwRpcConnection::Notify(const FString& method, const FString& params)
{
	if (Connection)
	{
		const FString finalData = FString::Printf(TEXT(R"({"method":"%s","params":%s})"), *method, *params);
		Connection->Send(finalData);
		UE_LOG(LogJwRPC, Warning, TEXT("OutgingData:%s"), *finalData);
	}
}

void UJwRpcConnection::Notify(const FString& method, TSharedPtr<FJsonValue> params)
{
	return Notify(method, params ? HelperStringifyJSON(params) : FString());
}

void UJwRpcConnection::K2_Notify(const FString& method, const FString& params)
{
	return Notify(method, params);
}

void UJwRpcConnection::K2_Request(const FString& method, const FString& params, FOnRPCResult onSuccess, FOnRPCError onError)
{
	return Request(method, params, FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> result) {
		//
		FString resultStringified = HelperStringifyJSON(result);
		onSuccess.ExecuteIfBound(resultStringified);

	}), FErrorCB::CreateLambda([onError](const FJwRPCError& err) {
		//
		onError.ExecuteIfBound(err);
	}));
}

void UJwRpcConnection::K2_NotifyJSON(const FString& method, const UJsonValue* params)
{
	return Notify(method, params ? params->ToString(false) : FString());
}

void UJwRpcConnection::K2_RequestJSON(const FString& method, const UJsonValue* params, FOnRPCResultJSON onSuccess, FOnRPCError onError)
{
	Request(method, params ? params->ToString(false) : FString(), FSuccessCB::CreateLambda([onSuccess](TSharedPtr<FJsonValue> result) {
		//
		UJsonValue* blueprinted = UJsonValue::MakeFromCPPVersion(result);
		if (ensureAlways(blueprinted))
			onSuccess.ExecuteIfBound(blueprinted);

	}), FErrorCB::CreateLambda([onError](const FJwRPCError & err) {
		//
		onError.ExecuteIfBound(err);
	}));
}

void UJwRpcConnection::Close(int Code, FString Reason)
{
	if (Connection)
	{
		Connection->Close(Code, Reason);
		Connection = nullptr;
	}
}



void UJwRpcConnection::K2_RegisterNotificationCallback(const FString& method, FNotificationDD callback)
{
	FMethodData md;
	md.bIsNotification = true;
	md.BPNotifyCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::RegisterNotificationCallback(const FString& method, FNotifyCB callback)
{
	FMethodData md;
	md.bIsNotification = true;
	md.NotifyCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::K2_RegisterRequestCallback(const FString& method, FRequestDD callback)
{
	FMethodData md;
	md.bIsNotification = false;
	md.BPRequestCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::RegisterRequestCallback(const FString& method, FRequestCB callback)
{
	FMethodData md;
	md.bIsNotification = false;
	md.RequestCB = callback;
	RegisteredCallbacks.Add(method, md);
}

bool UJwRpcConnection::IsConnected() const
{
	return Connection && Connection->IsConnected();
}

bool UJwRpcConnection::IsConnecting() const
{
	return bConnecting;
}

void UJwRpcConnection::Send(const FString& data)
{
	if (Connection && Connection->IsConnected())
	{
		Connection->Send(data);
		UE_LOG(LogJwRPC, Warning, TEXT("OutgingData:%s"), *data);
	}
}

void UJwRpcConnection::K2_IncomingRequestFinishError(const FJwRpcIncomingRequest& request, const FJwRPCError& error)
{
	request.FinishError(error);
}

void UJwRpcConnection::K2_IncomingRequestFinishSuccess(const FJwRpcIncomingRequest& request, const FString& result)
{
	request.FinishSuccess(result);
}

void UJwRpcConnection::K2_IncomingRequestFinishSuccessJSON(const FJwRpcIncomingRequest& request, const UJsonValue* result)
{
	if (result)
		request.FinishSuccess(result->ToString(false));
}

/*
UJwRpcConnection* UJwRpcConnection::Connect(const FString& url, TFunction<void()> onSucess, TFunction<void(const FString&)> onError)
{
	//UJwRpcConnection* pConn = NewObject<UJwRpcConnection>((UObject*)GetTransientPackage(), UJwRpcConnection::StaticClass());
	UJwRpcConnection* pConn = NewObject<UJwRpcConnection>((UObject*)GetTransientPackage(), UJwRpcConnection::StaticClass(), NAME_None, RF_Transient);

	FWebSocketsModule& wsModule = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	//FWebSocketsModule::Get() retuned null on no editor builds
	TSharedRef<IWebSocket> wsc = wsModule.CreateWebSocket(url);

	wsc->OnConnectionError().AddLambda(onError);
	wsc->OnConnected().AddLambda(onSucess);
	//#Note OnMessage must be binned before Connect()
	wsc->OnMessage().AddUObject(pConn, &UJwRpcConnection::OnMessage);
	wsc->OnClosed().AddUObject(pConn, &UJwRpcConnection::OnClosed);

	pConn->Connection = wsc;

	wsc->Connect();
	return pConn;
}
*/

void UJwRpcConnection::TryReconnect()
{
	if (!Connection->IsConnected() && !bConnecting)
	{
		bConnecting = true;
		LastConnectAttempTime = TimeSinceStart;
		Connection->Connect();
	}
}

UJwRpcConnection* UJwRpcConnection::CreateAndConnect(const FString& url, TSubclassOf<UJwRpcConnection> connectionClass)
{
	//UJwRpcConnection* pConn = NewObject<UJwRpcConnection>((UObject*)GetTransientPackage(), UJwRpcConnection::StaticClass());
	UJwRpcConnection* pConn = NewObject<UJwRpcConnection>((UObject*)GetTransientPackage(), connectionClass, NAME_None, RF_Transient);

	FWebSocketsModule& wsModule = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	//FWebSocketsModule::Get() retuned null on no editor builds
	TSharedRef<IWebSocket> wsc = wsModule.CreateWebSocket(url);

	wsc->OnConnectionError().AddUObject(pConn, &UJwRpcConnection::InternalOnConnectionError);
	wsc->OnConnected().AddUObject(pConn, &UJwRpcConnection::InternalOnConnect);
	//#Note OnMessage must be binned before Connect()
	wsc->OnMessage().AddUObject(pConn, &UJwRpcConnection::OnMessage);
	wsc->OnClosed().AddUObject(pConn, &UJwRpcConnection::OnClosed);

	pConn->SavedURL = url;
	pConn->Connection = wsc;
	pConn->bConnecting = true;
	pConn->LastConnectAttempTime = 0;

	UE_LOG(LogJwRPC, Log, TEXT("connecting to %s"), *url);

	wsc->Connect();
	return pConn;
}

void UJwRpcConnection::OnConnected(bool bReconnect)
{
	UE_LOG(LogJwRPC, Log, TEXT("UJwRpcConnection::OnConnected bReconnect:%d"), bReconnect);

	K2_OnConnected(bReconnect);
}

void UJwRpcConnection::OnConnectionError(const FString& error, bool bReconnect)
{
	UE_LOG(LogJwRPC, Error, TEXT("UJwRpcConnection::OnConnectionError Error:%s bReconnect:%d"), *error, bReconnect);

	LastConnectionErrorTime = TimeSinceStart;
	bConnecting = false;
	K2_OnConnectionError(error, bReconnect);
}

//UJwRpcConnection* UJwRpcConnection::K2_Connect(const FString& url, FOnConnectSuccess onSucess, FOnConnectFailed onError)
//{
//	return Connect(url, [onSucess]() {
//		onSucess.ExecuteIfBound();
//	}, [onError](const FString & error) {
//		onError.ExecuteIfBound(error);
//	});
//}

void UJwRpcConnection::BeginDestroy()
{
	Super::BeginDestroy();

	Close(1001);
}

FString UJwRpcConnection::GenId()
{
	IdCounter = (IdCounter + 1) % 0xFFffFF;
	return FString::FromInt(IdCounter);
	//return FString::FromHexBlob((const uint8*)&IdCounter, sizeof(IdCounter));
}

FJwRPCError JsonToJwError(TSharedPtr<FJsonObject> js)
{
	static FString STR_code("code");
	static FString STR_message("message");

	FJwRPCError ret;
	ret.Code = js->GetIntegerField(STR_code);
	ret.Message = js->GetStringField(STR_message);

	return ret;
}

void UJwRpcConnection::OnMessage(const FString& data)
{
	static FString STR_error("error");
	static FString STR_method("method");
	static FString STR_id("id");
	static FString STR_result("result");

	UE_LOG(LogJwRPC, Warning, TEXT("UJwRpcConnection::OnMessage:%s"), *data);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(data);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogJwRPC, Error, TEXT("failed to deserialize JSON"));
		return;
	}

	if (JsonObject->HasField(STR_method)) //is it request?
	{
		OnRequestRecv(JsonObject);
	}
	else //otherwise its respond
	{
		FString id = JsonObject->GetStringField(STR_id);
		FRequest* pRequest = Requests.Find(id);
		if (!pRequest)
		{
			UE_LOG(LogJwRPC, Error, TEXT("request with id:%s not found"), *id);
			return;
		}

		FRequest requestCopied = *pRequest;

		if (JsonObject->HasField(STR_error)) //is it error respond?
		{
			if (requestCopied.OnError.IsBound())
			{
				FJwRPCError errStruct = JsonToJwError(JsonObject->GetObjectField(STR_error));
				requestCopied.OnError.Execute(errStruct);
			}
		}
		else if (JsonObject->HasField(STR_result)) //is it valid respond?
		{
			if (requestCopied.OnResult.IsBound())
			{
				requestCopied.OnResult.Execute(JsonObject->TryGetField(STR_result));
			}
		}



		Requests.Remove(id);
	}
}


void UJwRpcConnection::InternalOnConnect()
{
	bConnecting = false;
	ReconnectAttempt = 0;

	OnConnected(!bFirstConnect);
	bFirstConnect = false;
}

void UJwRpcConnection::InternalOnConnectionError(const FString& error)
{
	OnConnectionError(error, !bFirstConnect);
}

void UJwRpcConnection::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogJwRPC, Log, TEXT("UJwRpcConnection::OnClosed StatusCode:%d Reason:%s bWasClean:%d"), StatusCode, *Reason, bWasClean);

	LastDisconnectTime = TimeSinceStart;
	bConnecting = false;

	KillAll(FJwRPCError::NoConnection);

	K2_OnClosed(StatusCode, Reason, bWasClean);

	
}

void UJwRpcConnection::KillAll(const FJwRPCError& error)
{
	for (auto& pair : Requests)
	{
		pair.Value.OnError.ExecuteIfBound(error);
	}

	Requests.Reset();
}

void UJwRpcConnection::CheckExpiredRequests()
{
	auto elapsed = TimeSinceStart - LastExpireCheckTime;
	if (elapsed < 2)
		return;

	LastExpireCheckTime = TimeSinceStart;

	static TArray<FString> expired;
	expired.Reset();

	for (auto& pair : Requests)
	{
		if (TimeSinceStart > pair.Value.ExpireTime)
		{
			expired.Add(pair.Key);
			UE_LOG(LogJwRPC, Log, TEXT("request timed out. id:%s"), *pair.Key);
			pair.Value.OnError.ExecuteIfBound(FJwRPCError::Timeout);
		}
	}

	for (const FString& id : expired)
		Requests.Remove(id);


}

void UJwRpcConnection::Tick(float DeltaTime)
{
	TimeSinceStart += DeltaTime;

	{
		//if have connection but its disconnected we try to reconnect
		if (Connection && !Connection->IsConnected() && !bConnecting && bAutoReconnectEnabled && LastDisconnectTime != 0)
		{
			auto elapsed = TimeSinceStart - LastConnectionErrorTime;
			if (elapsed > ReconnectDelay && ReconnectAttempt < ReconnectMaxAttempt)
			{
				ReconnectAttempt++;
				TryReconnect();
			}
		}

	}

	CheckExpiredRequests();
}

bool UJwRpcConnection::IsTickable() const
{
	return !IsTemplate();
}

TStatId UJwRpcConnection::GetStatId() const
{
	return TStatId();
}

void UJwRpcConnection::OnRequestRecv(TSharedPtr<FJsonObject> root)
{
	static FString STR_method("method");
	static FString STR_params("params");
	static FString STR_id("id");

	FString method = root->GetStringField(STR_method);


	const FMethodData* pInfo = RegisteredCallbacks.Find(method);
	if (!pInfo)
	{
		UE_LOG(LogJwRPC, Warning, TEXT("no callback is registered for method '%s'"), *method);
		return;
	}

	if (pInfo->bIsNotification) 
	{
		if (pInfo->NotifyCB.IsBound())
		{
			pInfo->NotifyCB.Execute(root->TryGetField(STR_params));
		}
		else
		{
			pInfo->BPNotifyCB.ExecuteIfBound(this, UJsonValue::MakeFromCPPVersion(root->TryGetField(STR_params)));
		}
	}
	else
	{
		FJwRpcIncomingRequest incReq;
		incReq.Connection = this;
		incReq.Id = root->GetStringField(STR_id);

		if (pInfo->RequestCB.IsBound())
		{
			pInfo->RequestCB.Execute(root->TryGetField(STR_params), incReq);
		}
		else
		{
			pInfo->BPRequestCB.ExecuteIfBound(this, UJsonValue::MakeFromCPPVersion(root->TryGetField(STR_params)), incReq);
		}
	}

}

void FJwRpcIncomingRequest::FinishError(const FJwRPCError& error) const
{
	UJwRpcConnection* pConn = Connection.Get();
	if(pConn && pConn->IsConnected())
	{
		const FString finalData = FString::Printf(TEXT(R"({"id":"%s","error":{"code":%d,"message":"%s"}})"), *Id, error.Code, *error.Message);
		Connection-> Send(finalData);
		
	}
}



void FJwRpcIncomingRequest::FinishError(int code, const FString& message) const
{
	FinishError(FJwRPCError{ code, message });
}

void FJwRpcIncomingRequest::FinishSuccess(TSharedPtr<FJsonValue> result) const
{
	FString resultString  = HelperStringifyJSON(result, false);
	FinishSuccess(resultString);
}

void FJwRpcIncomingRequest::FinishSuccess(const FString& result) const
{
	UJwRpcConnection* pConn = Connection.Get();
	if (pConn && pConn->IsConnected())
	{
		const FString finalData = FString::Printf(TEXT(R"({"id":"%s","result":%s})"), *Id, *result);
		Connection->Send(finalData);
	}
}

//#TODO needs valid code
FJwRPCError FJwRPCError::Timeout{ -1, FString("timeout") };
FJwRPCError FJwRPCError::NoConnection{-2, FString("no connection") };
