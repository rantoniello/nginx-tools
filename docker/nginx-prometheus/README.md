
To build docker execute:
```bash
$ docker build -t nginx-prometheus .
```

Run docker:
```
$ docker run -p 8888:8888 -p 9095:9095 -p 9090:9090 -p 8880:8880 -p 8881:8881 -p 8800:8800 -p 8801:8801 -p 8802:8802 -p 9113:9113 -p 9123:9123 -p 3100:3100 -p 9080:9080 nginx-prometheus
```
Open Grafana by browsing "http://localhost:8888".

The following services are launched by 'the supervisor' within the Docker container:
- An Nginx instance running several virtual servers, some playing as origins and others as proxy's, compiled with the "stub_status" and the "vts" statistics modules;
- Nginx Prometheus exporter, to enable serving the statistics exposed by the "stub_status" module to Prometheus;
- Third party Nginx Prometheus log-exporter for testing purposes (not currently included in Grafana dashboards);
- Prometheus server;
- Loki server and Promtail for "scraping" Nginx access logs;
- Grafana.

Mapped ports are (by service):
- Grafana: 8888, 9095
- Prometheus: self-monitoring in 9090, Nginx "official" exporters in 9113 (Proxy_0) and 9123 (Proxy_1); Nginx "VTS" directly pulling proxy's ports (8800, 8801, 8802)
- Promtail: serves on 9080
- Loki: 3100
- Nginx: two origins listen at 8880 and 8881; three proxy's at 8800, 8801 and 8802.

The attached Dockerfile is a self-explanatory detailed command list (no additional documentation is provided).
