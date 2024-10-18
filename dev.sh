#!/bin/sh

docker-compose build symqemu-dev
docker-compose up -d symqemu-dev
docker-compose exec -it symqemu-dev /bin/bash
