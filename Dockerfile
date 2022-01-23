FROM ubuntu:20.04

RUN ln -sf /usr/share/zoneinfo/Asia/Tokyo /etc/localtime

RUN apt-get update
RUN apt-get install git \
    g++ \
    gcc \
    cmake \
    build-essential \
    make -y

WORKDIR /home

COPY . .

RUN ./build.sh clean
RUN ./build.sh build

CMD ./build/rtmServerDemo
