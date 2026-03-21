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
