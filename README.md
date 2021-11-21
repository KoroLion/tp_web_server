# tp_web_server

Simple C web server for static files that is trying to compete with nginx.

### Testing:
1. Download ```httptest``` folder from https://github.com/init/http-test-suite
2. Put ```httptest``` near server's binary
3. Launch server
4. Test server with ```python3 httptest.py```


### Bench:
1. Complete testing
2. Run ```wrk -d 5s http://localhost:8081/httptest/splash.css```

#### by KoroLion