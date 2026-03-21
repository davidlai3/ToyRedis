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
