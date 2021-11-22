# SoftUiWarp

## Run

1. Run rping on a server
```bash
rping -d -s -a 10.10.1.2 -p 8888
```

2. Run fake_ping_client on another server
```bash
./fake_ping_client 10.10.1.2 8888
```

rping should output `ESTABLISHED`