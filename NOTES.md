## 3/19/26
Today I set up the TCP server and client files.

Next is actually parsing the strings that are sent between server in client

Traditionally, Redis uses RESP2. For messages that are sent from server to client, the format is:

`<type><data>\r\n`

Where `<type>` is one of `+`, `-`, `:`, `$`, `*`.

- `+`: String data
- `-`: Error data
- `:`: Integer data
- `$`: Bulk string data
- `*`: Array data

Client commands in Redis are passed as arrays of bulk strings over RESP.

## 3/21/26
Today I updated the TCP server to use `epoll` so that it can handle multiple connections. Redis should be single threaded, so there's no need to spawn any threads upon new client arrivals.

I also started laying out the code for the RESP protocol parsing/serialization.

The current issue I'm facing is figuring out how to parse all the commands. Redis is a streamed protocol, meaning that commands can be sent partially or in groups. This might take some extra care.

I'm probably going to need to give each user their own independent message buffer. This way the server can handle all messages even if they are sent in chunks, queued, etc...

## 4/5/26
Today I implemented the per-client buffers. Now the next step after receiving a byte stream from `epoll` is to parse each client's buffers for commands. To do this, I'll need to build the `RespParser` class.

## 05/23/26
Today I implemented the `RespParser` functions. I'm starting to work on a testing suite using `tests/test_resp_parser.cpp`. After this, I'll probably lay out some light testing for all the other current components (client/server connection, `epoll`). Then, the next step is to integrate the parser into `receive_command` and then I'll probably work towards the command dispatcher.

One thing to note: `RedisServer::connections` is a static inline variable. This is probably not what I want to have if I ever want to spawn two Redis instances on the same process. Should look into changing soon.

## 5/24/26
Today I built the testing suite and started working on the command dispatcher and database. There are a ton of commands that I need to write handlers for. I also made all the functions in `RedisServer` non-static, so that one process can run multiple Redis processes (probably going to need to change the listening port though).

For now, the command handlers only take `(std::vector<std::string_view> args, Database& db)` as arguments, but for more functionality down the line (like with pub/sub or multiple dbs), I'll need to pass in `ClientConnection` as well.

## 5/27/26
Today I finished building all the Database functions. Now all that's left is to build the handlers for each actual Redis command

## 5/29/26
Today I finished writing all the handlers for each Redis command, and I also implemented the `dispatch` function in `CommandDispatcher.cpp`. The next step is to write the serializer for RESP for sending responses to clients.

## 6/1/26
Today I built the RESP
