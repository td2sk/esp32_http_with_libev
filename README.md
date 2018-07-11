# Simple http server for esp32 with libev

## Build

```
$ git submodule init && git submodule update
$ cd components/ev && ./get_libev.sh
$ cd ../..
$ make
```

## Note

Now, this server always returns "200 OK". You can easily add routing, parsing params, returning large file, and so on.
If you try to implement some features, please read TODO comments on main/main.c.
