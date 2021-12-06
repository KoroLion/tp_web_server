# tp_web_server

Simple C web server for static files that is trying to compete with nginx.


### Launch
1. Setup in `settings.h`
2. Compile with make`
3. Run `./web_server <bind_addr> <port>`

### Unit testing
1. `./web_server test`

### Functional testing:
1. Download `httptest` folder from https://github.com/init/http-test-suite
2. Put `httptest` near server's binary
3. Launch server
4. Test server with `python3 httptest.py localhost 8081`


### Bench:
1. Complete testing
2. Run ```wrk -d 15 http://localhost:8081/httptest/splash.css```

#### by KoroLion