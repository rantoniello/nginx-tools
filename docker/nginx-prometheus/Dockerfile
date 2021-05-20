FROM ubuntu:20.04

# Install building tools
RUN apt update && apt upgrade -y && apt install build-essential autoconf -y
RUN apt install unzip xz-utils -y
RUN apt install ca-certificates -y
RUN DEBIAN_FRONTEND=noninteractive apt install -yq git

##############################################################################
# Supervisor
##############################################################################

RUN apt install supervisor -y && \
  mkdir -p /var/log/supervisor && \
  mkdir -p /etc/supervisor/conf.d
COPY ./supervisor.conf.template /etc/supervisor/supervisor.conf

##############################################################################
# Go-lang
##############################################################################

WORKDIR /
COPY ./go1.16.3.linux-amd64.tar.gz /tmp/go1.16.3.linux-amd64.tar.gz
WORKDIR /tmp
RUN rm -rf /usr/local/go && tar -C /usr/local -xzf /tmp/go1.16.3.linux-amd64.tar.gz
RUN cp -a /usr/local/go/bin/* /usr/local/bin/
RUN go version

##############################################################################
# Nginx
##############################################################################

# Install Nginx dependencies
#RUN add-apt-repository ppa:maxmind/ppa
RUN apt install zlib1g-dev libpcre3-dev libssl-dev libgeoip1 libgeoip-dev geoip-bin libmaxminddb-dev -y

# Copy Nginx source
#USER root
WORKDIR /
RUN mkdir -p /opt/nginx
COPY ./nginx-1.18.0.tar.gz /opt/nginx/
COPY ./nginx-module-vts-master.tar.xz /opt/nginx/
COPY ./ngx_http_geoip2_module.tar.gz /opt/nginx/
COPY ./echo-nginx-module-v0.62.tar.gz /opt/nginx/
COPY ./nginx_mime.types /opt/nginx/
COPY ./nginx.conf.template /opt/nginx/nginx.conf
COPY ./nginx.sv.conf.template /etc/supervisor/conf.d/nginx.sv.conf
COPY ./GeoLiteCity.dat.xz /opt/nginx

# Compile and install Nginx
WORKDIR /opt/nginx
RUN tar -xf nginx-1.18.0.tar.gz
RUN tar -xf nginx-module-vts-master.tar.xz
RUN tar -xf ngx_http_geoip2_module.tar.gz
RUN tar -xf echo-nginx-module-v0.62.tar.gz
RUN xz -d GeoLiteCity.dat.xz

WORKDIR /opt/nginx/nginx-1.18.0
RUN ./configure --prefix=/opt/nginx/_install_dir \
--with-compat \
--with-debug \
--with-select_module \
--with-poll_module \
--with-threads \
--with-http_ssl_module \
--with-http_v2_module \
--with-http_realip_module \
--with-http_addition_module \
--with-http_gunzip_module \
--with-http_gzip_static_module \
--with-http_auth_request_module \
--with-http_random_index_module \
--with-http_secure_link_module \
--with-http_degradation_module \
--with-http_slice_module \
--with-http_stub_status_module \
--with-http_geoip_module \
--add-module=/opt/nginx/nginx-module-vts-master \
--add-dynamic-module=/opt/nginx/ngx_http_geoip2_module-master \
--add-module=/opt/nginx/echo-nginx-module-0.62
RUN make
RUN make install
RUN mkdir -p /opt/nginx/_install_dir/var/run
RUN /opt/nginx/_install_dir/sbin/nginx -t -c /opt/nginx/nginx.conf

##############################################################################
# Nginx prometheus exporter #1 ("official")
# See https://github.com/nginxinc/nginx-prometheus-exporter
##############################################################################

WORKDIR /
COPY ./nginx-prometheus-exporter_v0.9.0.tar.gz /opt/nginx/
COPY ./nginx-prometheus-exporter-1.sv.conf.template /etc/supervisor/conf.d/nginx-prometheus-exporter-1.sv.conf
COPY ./nginx-prometheus-exporter-2.sv.conf.template /etc/supervisor/conf.d/nginx-prometheus-exporter-2.sv.conf
WORKDIR /opt/nginx
RUN tar -xf /opt/nginx/nginx-prometheus-exporter_v0.9.0.tar.gz
WORKDIR /opt/nginx/nginx-prometheus-exporter-0.9.0
RUN make

##############################################################################
# Nginx prometheus exporter #2 ("log" parser & exporter)
# See https://github.com/martin-helmich/prometheus-nginxlog-exporter
##############################################################################

WORKDIR /
COPY ./prometheus-nginxlog-exporter-v1.9.0.tar.gz /opt/nginx/
COPY ./nginxlog_exporter.yml.template /opt/nginx/nginxlog_exporter.yml
COPY ./nginx-prometheus-logexporter.sv.conf.template /etc/supervisor/conf.d/nginx-prometheus-logexporter.sv.conf
WORKDIR /opt/nginx
RUN tar -xf ./prometheus-nginxlog-exporter-v1.9.0.tar.gz
WORKDIR /opt/nginx/prometheus-nginxlog-exporter-1.9.0/
RUN go build

##############################################################################
# Prometheus
# See https://prometheus.io/download/
##############################################################################

# Install Prometheus
RUN mkdir -p /opt/prometheus
WORKDIR /
COPY ./prometheus-2.26.0.linux-amd64.tar.gz /opt/prometheus/
COPY ./prometheus.yml.template /opt/prometheus/prometheus.yml
COPY ./prometheus.sv.conf.template /etc/supervisor/conf.d/prometheus.sv.conf
WORKDIR /opt/prometheus
RUN tar -xf prometheus-2.26.0.linux-amd64.tar.gz

##############################################################################
# Loki
# See https://grafana.com/docs/loki/latest/installation/install-from-source/
##############################################################################

RUN mkdir -p /opt/loki
WORKDIR /
COPY ./loki-2.2.1.tar.gz /opt/loki/
COPY ./loki-local-config.yaml /opt/loki/loki-local-config.yaml
COPY ./promtail-linux-amd64.zip /opt/loki/promtail-linux-amd64.zip
COPY ./promtail-local-config.yaml /opt/loki/promtail-local-config.yaml
COPY ./loki.sv.conf.template /etc/supervisor/conf.d/loki.sv.conf
COPY ./promtail.sv.conf.template /etc/supervisor/conf.d/promtail.sv.conf
WORKDIR /opt/loki
RUN tar -xf loki-2.2.1.tar.gz
RUN unzip promtail-linux-amd64.zip
WORKDIR /opt/loki/loki-2.2.1
RUN go version
RUN make loki

##############################################################################
# Grafana (Install the latest OSS release)
# See https://grafana.com/docs/grafana/latest/installation/debian/?pg=docs
##############################################################################
RUN apt-get install -y apt-transport-https
RUN DEBIAN_FRONTEND=noninteractive apt-get install -yq software-properties-common wget
RUN wget -q -O - https://packages.grafana.com/gpg.key | apt-key add -
# Add this repository for stable releases:
RUN echo "deb https://packages.grafana.com/oss/deb stable main" | tee -a /etc/apt/sources.list.d/grafana.list
# Add this repository if you want beta releases:
RUN echo "deb https://packages.grafana.com/oss/deb beta main" | tee -a /etc/apt/sources.list.d/grafana.list
# Install
RUN apt-get update -y
RUN apt-get -y install grafana
WORKDIR /
# Default dirs:
RUN mkdir -p /var/lib/grafana/plugins
# /var/log/grafana
# /var/lib/grafana
# WorkingDirectory=/usr/share/grafana
COPY ./grafana.sv.conf.template /etc/supervisor/conf.d/grafana.sv.conf
COPY ./grafana.ini.template /etc/grafana/grafana.ini
COPY ./grafana-worldmap-panel-v0.3.2.zip /var/lib/grafana/plugins/
WORKDIR /var/lib/grafana/plugins
RUN unzip grafana-worldmap-panel-v0.3.2.zip
# Dashboards & datasources
RUN mkdir -p /etc/grafana/provisioning
RUN mkdir -p /etc/grafana/provisioning/dashboards
RUN mkdir -p /etc/grafana/provisioning/datasources
WORKDIR /
COPY ./dashboard.yml.template /etc/grafana/provisioning/dashboards/dashboard.yaml
COPY ./grafana-dashboards/* /etc/grafana/provisioning/dashboards/
COPY ./datasources.yml.template /etc/grafana/provisioning/datasources/datasources.yaml

CMD ["supervisord", "-c", "/etc/supervisor/supervisor.conf"]

