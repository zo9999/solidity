FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
clang \
make \
libboost-all-dev \
cmake

WORKDIR /solidity

COPY . .

# Build
RUN mkdir build \
&& cd build \
&& cmake .. \
&& make

CMD ["echo", "Use a command"]
