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

}


void UJwRpcConnection::Request(const FString& method, const FString& params, SuccessCB onSuccess, ErrorCB onError)
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
	Connection->Send(finalData);
	
	UE_LOG(LogJwRPC, Warning, TEXT("OutgingData:%s"), *finalData);

	Requests.Add(id, MoveTemp(req));
}

void UJwRpcConnection::Notify(const FString& method, const FString& params)
{
	if (Connection && Connection->IsConnected())
	{
		const FString finalData = FString::Printf(TEXT(R"({"method":"%s","params":%s})"), *method, *params);
		Connection->Send(finalData);
		UE_LOG(LogJwRPC, Warning, TEXT("OutgingData:%s"), *finalData);
	}
}

void UJwRpcConnection::K2_Notify(const FString& method, const FString& params)
{
	Notify(method, params);
}

void UJwRpcConnection::K2_Request(const FString& method, const FString& params, FOnRPCResult onSuccess, FOnRPCError onError)
{
	Request(method, params, [onSuccess](TSharedPtr<FJsonValue> result) {
		//#TODO

	}, [onError](const FJwRPCError& err) {
		onError.ExecuteIfBound(err);
	});
}

void UJwRpcConnection::K2_NotifyJSON(const FString& method, const UJsonValue* params)
{
	if (!params) 
	{
		//#TODO LOG
		return ;
	}

	Notify(method, params->ToString(false));
}

void UJwRpcConnection::K2_RequestJSON(const FString& method, const UJsonValue* params, FOnRPCResultJSON onSuccess, FOnRPCError onError)
{
	if (!params)
	{
		//#TODO LOG
		return;
	}

	Request(method, params->ToString(false), [onSuccess](TSharedPtr<FJsonValue> result) {
		UJsonValue* bped = UJsonValue::MakeFromCPPVersion(result);
		if (!bped) 
		{
		}
		else
		{
			onSuccess.ExecuteIfBound(bped);
		}
	}, [onError](const FJwRPCError & err) {
			onError.ExecuteIfBound(err);
	});
}

void UJwRpcConnection::Close(int Code, const FString& Reason)
{
	if (Connection)
	{
		Connection->Close(Code, Reason);
		Connection = nullptr;
	}
}

void UJwRpcConnection::Test0()
{
	UJwRpcConnection* pConn = nullptr;
	pConn->Request("req1", "[]", nullptr, nullptr);
	pConn->Notify("noti1", "{}");



	
}

void UJwRpcConnection::RegisterNotificationCallback(FString method, FNotificationDD callback)
{
	FMethodData md;
	md.bIsNotification = true;
	md.BPNotifyCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::RegisterNotificationCallback(FString method, FNotifyCallbackBase callback)
{
	FMethodData md;
	md.bIsNotification = true;
	md.NotifyCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::RegisterRequestCallback(FString method, FRequestDD callback)
{
	FMethodData md;
	md.bIsNotification = false;
	md.BPRequestCB = callback;
	RegisteredCallbacks.Add(method, md);
}

void UJwRpcConnection::RegisterRequestCallback(FString method, FRequestCallbackBase callback)
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

void UJwRpcConnection::IncomingRequestFinishError(const FJwRpcIncomingRequest& request, const FJwRPCError& error)
{
	request.FinishError(error);
}

void UJwRpcConnection::IncomingRequestFinishSuccess(const FJwRpcIncomingRequest& request, const FString& result)
{
	request.FinishSuccess(result);
}

void UJwRpcConnection::IncomingRequestFinishSuccessJSON(const FJwRpcIncomingRequest& request, const UJsonValue* result)
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

	wsc->OnConnectionError().AddUObject(pConn, &UJwRpcConnection::OnConnectionError);
	wsc->OnConnected().AddUObject(pConn, &UJwRpcConnection::InternalOnConnect);
	//#Note OnMessage must be binned before Connect()
	wsc->OnMessage().AddUObject(pConn, &UJwRpcConnection::OnMessage);
	wsc->OnClosed().AddUObject(pConn, &UJwRpcConnection::OnClosed);

	pConn->SavedURL = url;
	pConn->Connection = wsc;
	pConn->bConnecting = true;
	pConn->LastConnectAttempTime = 0;

	wsc->Connect();
	return pConn;
}

void UJwRpcConnection::OnConnected(bool bReconnect)
{
	K2_OnConnected(bReconnect);
}

void UJwRpcConnection::OnConnectionError(const FString& error)
{
	LastConnectionErrorTime = TimeSinceStart;
	bConnecting = false;
	K2_OnConnectionError(error);
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
	UE_LOG(LogJwRPC, Error, TEXT("UJwRpcConnection::BeginDestroy"));

	Super::BeginDestroy();

	Close(100, FString());
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

	UE_LOG(LogJwRPC, Warning, TEXT("UJwRpcConnection::OnMessage:%s"), *data);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(data);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogJwRPC, Error, TEXT("failed to deserialize JSON"));
		return;
	}

	if (JsonObject->HasField(TEXT("method"))) //is it request?
	{
		OnRequestRecv(JsonObject);
	}
	else
	{
		FString id = JsonObject->GetStringField("id");
		FRequest* pRequest = Requests.Find(id);
		if (!pRequest)
		{
			UE_LOG(LogJwRPC, Error, TEXT("request with id:%s not found"), *id);
			return;
		}


		if (JsonObject->HasField(TEXT("error"))) //is it error respond?
		{
			if (pRequest->OnError)
			{
				FJwRPCError errStruct = JsonToJwError(JsonObject->GetObjectField(STR_error));
				pRequest->OnError(errStruct);
				pRequest->OnError = nullptr;
			}
		}

		if (JsonObject->HasField(TEXT("result"))) //is it valid respond?
		{
			if (pRequest->OnResult)
			{
				pRequest->OnResult(JsonObject->TryGetField(TEXT("result")));
				pRequest->OnResult = nullptr;
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

void UJwRpcConnection::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogJwRPC, Error, TEXT("UJwRpcConnection::OnClosed StatusCode:%d Reason:%s bWasClean:%d"), StatusCode, *Reason, bWasClean);

	LastDisconnectTime = TimeSinceStart;
	bConnecting = false;

	KillAll(FJwRPCError::NoConnection);

	K2_OnClosed(StatusCode, Reason, bWasClean);

	
}

void UJwRpcConnection::KillAll(const FJwRPCError& error)
{
	for (auto& pair : Requests)
	{
		if (pair.Value.OnError)
			pair.Value.OnError(error);
	}

	Requests.Reset();
}

void UJwRpcConnection::CheckExpiredRequests()
{
	auto elapsed = TimeSinceStart - LastExpireCheckTime;
	if (elapsed < 2)
		return;

	LastExpireCheckTime = TimeSinceStart;

	TArray<FString> expired;
	for (auto& pair : Requests)
	{
		if (TimeSinceStart > pair.Value.ExpireTime)
		{
			expired.Add(pair.Key);

			if (pair.Value.OnError)
				pair.Value.OnError(FJwRPCError::Timeout);
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

void UJwRpcConnection::OnRequestRecv(TSharedPtr<FJsonObject>& root)
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

template<class CharType, class PrintPolicy>
bool ToJsonStringInternal(TSharedPtr<FJsonValue> JsonObject, FString& OutJsonString)
{
	TSharedRef<TJsonWriter<CharType, PrintPolicy> > JsonWriter = TJsonWriterFactory<CharType, PrintPolicy>::Create(&OutJsonString, 0);
	bool bSuccess = FJsonSerializer::Serialize(JsonObject, FString(), JsonWriter);
	JsonWriter->Close();
	return bSuccess;
}

void FJwRpcIncomingRequest::FinishSuccess(TSharedPtr<FJsonValue> result) const
{
	if (result)
	{
		FString resultString  = HelperStringifyJSON(result, false);
		if(!resultString.IsEmpty())
			FinishSuccess(resultString);
	}
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
