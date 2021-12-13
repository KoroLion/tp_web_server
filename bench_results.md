# WRK benchmarking

### Start:
* `./web_server 0.0.0.0 8081 [num of cores]`
* `wrk -t 3 -c 200 -d $1 http://localhost:8081/httptest/splash.css`

## 1 core:
```
Running 10s test @ http://localhost:8082/httptest/splash.css
3 threads and 200 connections
Thread Stats   Avg      Stdev     Max   +/- Stdev
Latency    18.21ms   65.19ms   1.77s    99.07%
Req/Sec     3.29k   349.56     4.04k    59.00%
98234 requests in 10.00s, 9.03GB read
Socket errors: connect 0, read 0, write 0, timeout 4
Requests/sec:   9822.30
Transfer/sec:      0.90GB
```

## 2 cores:
```
Running 10s test @ http://localhost:8081/httptest/splash.css
3 threads and 200 connections
Thread Stats   Avg      Stdev     Max   +/- Stdev
Latency    15.39ms   91.30ms   1.68s    97.98%
Req/Sec     8.36k     1.10k   11.84k    69.00%
249692 requests in 10.02s, 22.96GB read
Socket errors: connect 0, read 0, write 0, timeout 1
Requests/sec:  24925.55
Transfer/sec:      2.29GB
```

## 3 cores
```
Running 10s test @ http://localhost:8084/httptest/splash.css
3 threads and 200 connections
Thread Stats   Avg      Stdev     Max   +/- Stdev
Latency     4.42ms   15.45ms 453.35ms   98.96%
Req/Sec    11.72k     1.62k   16.30k    67.67%
351429 requests in 10.07s, 32.32GB read
Requests/sec:  34910.19
Transfer/sec:      3.21GB
```

## nginx (6 cores)
```
Running 10s test @ http://localhost:8080/httptest/splash.css
3 threads and 200 connections
Thread Stats   Avg      Stdev     Max   +/- Stdev
Latency     2.95ms    4.17ms  66.90ms   86.96%
Req/Sec    34.13k     4.02k   41.73k    75.00%
1019961 requests in 10.03s, 93.92GB read
Requests/sec: 101734.14
Transfer/sec:      9.37GB
```
