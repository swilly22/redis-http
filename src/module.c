#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <redismodule.h>
#include "./src/rmutil/util.h"
#include "./deps/picohttpparser/picohttpparser.h"
#include "./deps/thpool/thpool.h"

#define DEFAULT_LISTEN_PORT 6380
#define WORKER_THREAD_COUNT 64

/* structure defining our HTTP server configuration */
typedef struct {    
    unsigned short port;    /* port to listen to */
    threadpool threadPool;  /* pool of worker threads */
} ServerConfig;

typedef struct {
    int connfd;
} RequestContext;

/* handleRequest reads an incoming HTTP request and replies 
 * with the content of the requested key */
void* handleRequest(void *arg) {
    RequestContext *reqCtx = arg;

    /* get a thread safe context */
    RedisModuleCtx *ctx = RedisModule_GetThreadSafeContext(NULL);

    /* read up to 4096 bytes from request */
    /* TODO: use loop to read request in chunks */
    char buffer[4096] = {0};
    int bytesRead = read(reqCtx->connfd, buffer, 4096);
    if (bytesRead == -1) {        
        RedisModule_Log(ctx, "warning", "Failed reading from socket");
        goto clean_connection;
    }

    /* request is bigger then expected  */
    if (bytesRead == sizeof(buffer)) {
        RedisModule_Log(ctx, "warning", "Request is too long");
        goto clean_connection;
    }
    
    /* Parse request */
    const char *method = NULL;
    const char *path = NULL;
    struct phr_header headers[32];
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);
    size_t path_len = 0;
    size_t method_len = 0;
    size_t prevbuflen = 0;
    int minor_version = 0;

    int pret = phr_parse_request(buffer, bytesRead, &method, &method_len, &path, &path_len,
        &minor_version, headers, &num_headers, prevbuflen);
    if (pret == -1) {
        RedisModule_Log(ctx, "warning", "Failed to parse request");
        goto clean_connection;
    }

    /* Log request info */
    /* RedisModule_Log(ctx, "notice", "request is %d bytes long", pret);
     * RedisModule_Log(ctx, "notice", "method is %.*s", (int)method_len, method);
     * RedisModule_Log(ctx, "notice", "path is %.*s", (int)path_len, path);
     * RedisModule_Log(ctx, "notice", "HTTP version is 1.%d", minor_version);
     * RedisModule_Log(ctx, "notice", "headers");
     * for (int i = 0; i != num_headers; ++i) {
     *     RedisModule_Log(ctx, "notice", "%.*s: %.*s", (int)headers[i].name_len, headers[i].name,
     *         (int)headers[i].value_len, headers[i].value);
     * } */
            
    
    RedisModuleString* strKey = RedisModule_CreateString(ctx, path, path_len);

    /* Acquire lock before accessing Redis keys
     * open requested key */
    RedisModule_ThreadSafeContextLock(ctx);
    RedisModuleKey *key = RedisModule_OpenKey(ctx, strKey, REDISMODULE_READ);
    RedisModule_FreeString(ctx, strKey);
    
    /* Make sure key is of type String */
    if (key == NULL || RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_STRING) {
        RedisModule_CloseKey(key);
        RedisModule_ThreadSafeContextUnlock(ctx);
        RedisModule_Log(ctx, "warning", "key is either missing or of the wrong type");
        goto clean_connection;
    }

    /* Read key's value */
    size_t len = 0;
    char *value = RedisModule_StringDMA(key, &len, REDISMODULE_READ);
    
    /* We can free Redis lock */
    RedisModule_CloseKey(key);
    RedisModule_ThreadSafeContextUnlock(ctx);
    
    /* Prepare HTTP response */
    char contentLength[64] = {0};
    sprintf(contentLength, "Content-Length: %zu\r\n", len);
    write(reqCtx->connfd, "HTTP/1.0 200 OK\r\n", 17);
    write(reqCtx->connfd, contentLength, strlen(contentLength));
    /* TODO: either encode content type and content length within the redis key,
     * or determing content type based on key's' name*/
    write(reqCtx->connfd, "Content-Type: image/jpeg\r\n\r\n", 28);
    
    /* Write requested key as our response body */
    if (write(reqCtx->connfd, value, len) == -1) {
        RedisModule_Log(ctx, "warning", "Failed writing to socket");
        goto clean_connection;
    }
    
    /* Close connection with client */
clean_connection:
    close(reqCtx->connfd);
    free(reqCtx);
    RedisModule_FreeThreadSafeContext(ctx);
    return 0;
}

/* startListening is intended to run on its own thread
 * basicly we're listen for new connections and accept them */
void* startListening(void *arg) {
    int listenfd;    
    struct sockaddr_in serv_addr;
    ServerConfig *config = (ServerConfig *)arg;
    RedisModuleCtx *ctx = RedisModule_GetThreadSafeContext(NULL);

    /* Create listening socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (listenfd == -1) {
        RedisModule_Log(ctx, "warning", "Failed to open socket");
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(config->port);

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        RedisModule_Log(ctx, "warning", "Failed to bind socket");
        return 0;
    }

    listen(listenfd, 5);
    RedisModule_Log(ctx, "notice", "listen on port %d", config->port);
    
    /* TODO: move request handling to another thread, use thread-pool */
    /* Accept requests */
    while(1) {
        int connfd = accept(listenfd, (struct sockaddr *) NULL, NULL);
        if (connfd == -1) {
            RedisModule_Log(ctx, "warning", "Failed to accept connection");
            continue;
        }

        RequestContext *reqCtx = malloc(sizeof(RequestContext));
        reqCtx->connfd = connfd;
        thpool_add_work(config->threadPool, (void*)handleRequest, reqCtx);
    }

    /* We're done with our thread safe context. */
    RedisModule_FreeThreadSafeContext(ctx);
    
    /* close listening socket */
    close(listenfd);
}


/* Register our module
 * at the moment there are no commands to register,
 * we're simply creating a listening thread which will accept external connections 
 * each connection will try to read a request Redis key */
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx, "http", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    /* server configuration */
    ServerConfig *config = malloc(sizeof(ServerConfig));
    config->port = DEFAULT_LISTEN_PORT;

    /* Use listen port if specified */
    long long llPort;
    int listenPortIdx = RMUtil_ArgIndex("PORT", argv, argc);
    if(listenPortIdx != -1) {
        if(REDISMODULE_OK == RedisModule_StringToLongLong(argv[listenPortIdx+1], &llPort)) {
            config->port = llPort;
        }
    }

    /* Init thread pool */
    config->threadPool = thpool_init(WORKER_THREAD_COUNT);
    
    /* Start listening thread */
    thpool_add_work(config->threadPool, (void*)startListening, (void*)config);
    // pthread_t tid;
    // if(pthread_create(&tid, NULL, &startListening, config) != 0) {
    //     RedisModule_Log(ctx, "warning", "can't create thread\n");
    // }

    return REDISMODULE_OK;
}