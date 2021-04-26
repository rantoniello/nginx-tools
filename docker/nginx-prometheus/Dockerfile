FROM ubuntu:20.04

# Install building tools
RUN apt update && apt upgrade -y && apt install build-essential -y

##############################################################################
# Supervisor
##############################################################################

RUN apt install supervisor -y && \
  mkdir -p /var/log/supervisor && \
  mkdir -p /etc/supervisor/conf.d
COPY ./supervisor.conf.template /etc/supervisor/supervisor.conf

##############################################################################
# Nginx
##############################################################################

# Install Nginx dependencies
RUN apt install zlib1g-dev libpcre3-dev libssl-dev -y

# Copy Nginx source
#USER root
WORKDIR /
RUN mkdir -p /opt/nginx
COPY ./nginx-1.18.0.tar.gz /opt/nginx/
COPY ./nginx-module-vts-master.tar.xz /opt/nginx/
COPY ./nginx_mime.types /opt/nginx/
COPY ./nginx.conf.template /opt/nginx/nginx.conf
COPY ./nginx.sv.conf.template /etc/supervisor/conf.d/nginx.sv.conf

# Compile and install Nginx
WORKDIR /opt/nginx
RUN tar -xf /opt/nginx/nginx-1.18.0.tar.gz
RUN tar -xf /opt/nginx/nginx-module-vts-master.tar.xz
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
--add-module=/opt/nginx/nginx-module-vts-master
RUN make
RUN make install
RUN mkdir -p /opt/nginx/_install_dir/var/run
RUN /opt/nginx/_install_dir/sbin/nginx -t -c /opt/nginx/nginx.conf

##############################################################################
# Nginx prometheus exporter #1 ("official")
# See https://github.com/nginxinc/nginx-prometheus-exporter
##############################################################################

RUN DEBIAN_FRONTEND=noninteractive apt install -yq git golang
WORKDIR /
COPY ./nginx-prometheus-exporter_v0.9.0.tar.gz /opt/nginx/
COPY ./nginx-prometheus-exporter-1.sv.conf.template /etc/supervisor/conf.d/nginx-prometheus-exporter-1.sv.conf
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

CMD ["supervisord", "-c", "/etc/supervisor/supervisor.conf"]

