# Web Server for static

Simple C web server for static files serving that is trying to compete with nginx.

Works only on Linux. 

Technology: thread pool + epoll

### Launch
1. Setup in `settings.h`
2. Compile with `make`
3. Run `./web_server <bind_addr> <port> <cpu_amount>`

### Unit testing
1. `./web_server test`

### Functional testing:
1. Download `httptest` folder and `httptest.py` from [https://github.com/init/http-test-suite](https://github.com/init/http-test-suite)
2. Put `httptest` and `httptest.py` near server's binary (`web_server`)
3. Launch server
4. Test server with `python3 httptest.py localhost 8081`


### Bench:
1. Complete testing
2. Run ```wrk -d 15 http://localhost:8081/httptest/splash.css```

#### by KoroLion