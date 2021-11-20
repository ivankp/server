# TODO

- `EPOLLRDHUP`, Firefox favicon requests
- Extend `thread_safe_queue` to track active sockets
- File cache
- Finish websockets
- Parse GET and POST form data
- Pipe directly to socket
- Finish numconv

## Bring back

- gzip
- Users

# Ideas

- Temporarily ban IPs that send strange requests
- Lower requests priority if requests are too frequent to mitigate DoS attacks
- Time socket reading to mitigate Slowloris attacks
