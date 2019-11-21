# UE4JwRPC
UE4 JSON-RPC over Websocket


# WIP not production ready yet !

# Featues 
- APIs for both blueprint and C++. one intance can be shared.
- Bidirectional, both sides can send and recieve Request and Notification
- auto reconnect feature
- ...

# C++ Sample Code 
```C++
#pragma once

#include "JwRPC.h"

#include "SampleCode.generated.h"

UCLASS()
class UMyConnection : public UJwRpcConnection
{
	GENERATED_BODY()
public:
	UMyConnection()
	{
		this->RegisterNotificationCallback(TEXT("greet"), FNotifyCB::CreateUObject(this, &UMyConnection::OnNotification_Greet));
		this->RegisterRequestCallback(TEXT("divide"), FRequestCB::CreateUObject(this, &UMyConnection::OnRequest_Divide));
	}

	void OnConnected(bool bReconnect) override
	{
		//send a notification to server
		this->Notify(TEXT("hi"), MakeShared<FJsonValueString>(TEXT("hi. how is it going ?")));

		//make the parameters and send a request to the server
		auto params = MakeShared<FJsonObject>();
		params->SetNumberField(TEXT("a"), 10);
		params->SetNumberField(TEXT("b"), 20);
		this->Request(TEXT("sum"), MakeShared<FJsonValueObject>(params), FSuccessCB::CreateLambda([](TSharedPtr<FJsonValue> result) {
			//the result has arrived. it must be 20
			check(result->AsNumber() == 20);

		}), nullptr);
	}

	//this is called if server sends us 'greet' notification
	void OnNotification_Greet(TSharedPtr<FJsonValue> params)
	{
		
	}
	//this is called if server sends us 'divide' request
	void OnRequest_Divide(TSharedPtr<FJsonValue> params, FJwRpcIncomingRequest& handle)
	{
		const int left = params->AsObject()->GetIntegerField("left");
		const int right = params->AsObject()->GetIntegerField("right");
        //send error if its divide by zero
		if (right == 0)
			return handle.FinishError(666, TEXT("divide by zero"));
        //send the result of division 
		handle.FinishSuccess(MakeShared<FJsonValueNumber>(left / right));
	}
};


//we create our connection and start conecting ...
UMyConnection* pConnection = UJwRpcConnection::CreateAndConnect<UMyConnection>(TEXT("ws://localhost"));
//the connection is closed if gets garbage collected. we keep a reference in game instance
MyGameInstance->ClientConnection = pConnection;

```

# Blueprint API
![](\Images\Capture.JPG)

# Usage Example
[UE4JwRPC_Example](https://github.com/UPO33/UE4JwRPC_Example)



# See also
[UE4JsonBP](https://github.com/UPO33/UE4JsonBP), 
[UE4JwRPC_Example](https://github.com/UPO33/UE4JwRPC_Example),
[JwRPC](https://github.com/UPO33/JwRPC)
 

