#!/bin/bash

set -e

cd /docker-entrypoint-initdb.d

psql -U postgis -d sims -f create_tables.sql

rm create_tables.sql