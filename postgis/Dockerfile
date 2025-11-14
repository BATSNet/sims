FROM postgis/postgis:15-3.3

WORKDIR /db
COPY initdb.sh /docker-entrypoint-initdb.d/initdb.sh
COPY create_tables.sql /docker-entrypoint-initdb.d/create_tables.sql

ENV POSTGRES_DB=sims
ENV POSTGRES_USER=postgis
ENV POSTGRES_PASSWORD=postgis
ENV PGDATA=/var/lib/postgresql/data/pgdata

EXPOSE 5432

HEALTHCHECK --interval=30s --timeout=3s --start-period=30s \
    CMD pg_isready -U $POSTGRES_USER -d POSTGRES_DB || exit -1