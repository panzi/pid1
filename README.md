pid1
====

Extremely simple pid1 implementation, meant to be used in docker to reap zombies.

Usage
-----

```Dockerfile
# ...
CMD exec pid1 /path/to/server/bin arg1 arg2...
```

`pid1` starts the passed command with the given arguments and stops when that
command stops. The passed command has to be an absolute path. Upon stopping
`pid1` sends `SIGTERM` to all remaining processes and then waits for them to
finish. It also forwards any `SIGTERM` and `SIGINT` signals it receives.
