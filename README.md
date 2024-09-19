# gRPC chat
Simple single-room gRPC chat server and http client.

## Quick start

Build & Start:
```
docker compose build server client
docker compose up -d server client
```

cURL examples:
```
$ curl -X POST localhost:8080/sendMessage -d '{"author": "alice", "text": "hey"}'
{"sendTime":"2021-09-12T10:25:22.454093428Z"}

$ curl -X POST localhost:8080/getAndFlushMessages
[{"author":"alice","text":"hey","sendTime":"2021-09-12T10:25:22.454093428Z"},{"author":"alice","text":"hey guys","sendTime":"2021-09-12T10:25:41.296997047Z"}]
```

## Server
The server opens new connections and sends incoming messages on these connections.

### gRPC Methods ([Details](/proto/messenger.proto))
- **ReadMessages:** open a connection to receive messages
- **SendMessage:** send a message to open connections


## Client
The client opens a connection when it starts up and starts buffering incoming messages in a background thread. The http interface is used to send messages to the chat and receive messages from the buffer.

### Http Methods
- **POST /sendMessage**: send a message to the server
```
Body:
{
    "author": "Ivan Ivanov",
    "text": "Hey guys"
}

Response:
{
    "sendTime": "..."
}
```

- **POST /getAndFlushMessage** get messages from buffer

```
Response:
[{
    "author": "Ivan Ivanov",
    "text": "Hey guys",
    "sendTime": "..."
},{
    "author": "Petr Petrov",
    "text": "Hey Ivan",
    "sendTime": "..."
}]
```