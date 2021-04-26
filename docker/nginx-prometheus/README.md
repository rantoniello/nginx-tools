
To build docker execute:
```
$ docker build -t nginx-prometheus .
```

Run docker:
```
$ docker run -p 9090:9090 -p 8880:8880 -p 9113:9113 -p 9114:9114 nginx-prometheus
```

