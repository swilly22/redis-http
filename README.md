# Redis HTTP

Redis-HTTP is a Redis module which enables you to access Redis directly over HTTP,
although we could have exposed most of the Redis API, the primary use-case for this module is
serving static cached content directly from Redis.

## Build

    % make

## Usage

Load module into Redis

    % redis-server --loadmodule redis-http.so

*Please note, Redis-HTTP uses the latest unstable version of Redis, previous versions are not supported.

Store your content

Redis-Cli:

    % redis-cli set '/logo.jpg' <IMAGE DATA>

Python:

    img = open(<PATH_TO_IMAGE>).read()
    r = redis.StrictRedis(host='localhost')
    r.set('/logo.jpg', img)

Access

    % curl http://<REDIS IP>:6380/logo.jpg

Redis-HTTP listens for HTTP requests on port number 6380,
but you can specify an alternative port number

    % redis-server --loadmodule redis-http.so PORT <PORT_NUMBER>

## Limitations

At the moment Redis-HTTP doesn't try to guess your content type, but assumes it's a jpeg image.