FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
clang \
make \
libboost-all-dev

WORKDIR ..

COPY . /solidity

# Build
RUN cd /solidity/build \
&& cmake .. \
&& make

CMD ["echo", "Use a command"]